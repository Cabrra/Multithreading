// Include file and line numbers for memory leak detection for visual studio in debug mode
// NOTE: The current implementation of C++11 shipped with Visual Studio 2012 will leak a single
//   44-byte mutex (at_thread_exit_mutex) internally if any threads have been created. This
//   will show up in the output window without a filename or line number.
#if defined _MSC_VER && defined _DEBUG
	#include <crtdbg.h>
	#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)
	#define ENABLE_LEAK_DETECTION() _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF)
#else
	#define ENABLE_LEAK_DETECTION()
#endif
#include <iostream>  
#include <vector>
#include <future> 
#include <condition_variable>
#include <thread> 
#include <chrono>
#include <random>
#include <stdarg.h>
#include <stdio.h>

using namespace std;


#define WAIT_FOR_THREAD(r) if ((r)->joinable()) (r)->join();

class UniformRandInt
{
public:
	void Init(int min, int max)
	{
		// Seed our random number generator with a non-deterministic random value. If no such capabilities exist then
		//   the number will be pulled from a pseudo random number generator.
		randEngine.seed(randDevice());

		// We want to generate values in the range of [min, max] (inclusive) with a uniform distribution.
		distro = std::uniform_int_distribution<int>(min, max);
	}

	int operator()()
	{
		return distro(randEngine);
	}

private:
	std::random_device randDevice;
	std::mt19937 randEngine;
	std::uniform_int_distribution<int> distro;
	std::mutex randMutex;
};

///////////////////////////////////////////////////////////////////////////////////
// The various states the game can be in
///////////////////////////////////////////////////////////////////////////////////
enum class GameState
{
	StillPlaying,
	Won,
	Draw
};

///////////////////////////////////////////////////////////////////////////////////
// The player types
///////////////////////////////////////////////////////////////////////////////////
enum class PlayerType
{
	None,
	X,
	O
};

///////////////////////////////////////////////////////////////////////////////////
// Various types of operations that can be performed on our synchronization object
//   via LogSync.
///////////////////////////////////////////////////////////////////////////////////
enum class LogSyncOperation
{
	 // Create and initialize resources
	Init,
	// Release all resources and cleanup
	Release,
	// Lock the synchronization object
	Lock,
	// Unlock the synchronization object
	Unlock
};

///////////////////////////////////////////////////////////////////////////////////
// Contains all game related data
///////////////////////////////////////////////////////////////////////////////////
struct Game
{
	// Number of players that are currently playing in this game
	int playerCount;
	// ID of the game
	int gameNumber;
	// Determines which player is currently playing
	PlayerType currentTurn;
	// The current state of the board. It will always be StillPlaying until the game's complete.
	GameState currentGameState;
	// Thread ID of the X player or -1 if X player doesn't exist for this game
	int playerX;
	// Thread ID of the O player or -1 if O player doesn't exist for this game
	int playerO;
	// Mutex to access playerCount in a thread safe manner.
	std::mutex playerCountMutex;
	// Primary mutex that controls the game play. The player that has this mutex locked
	//  will be playing, while the other player will be waiting on the gameCondition.
	std::mutex gameMutex;
	// Primary conditional that controls the game play
	std::condition_variable gameCondition;
	// Unique lock which will be constructed with the gameMutex. This will ONLY be valid in
	//  the PlayGame function.
	std::unique_lock<std::mutex> *gameUniqueLock;
	// A 3x3 array of PlayerTypes that represents the game board. Each entry will represent
	//  which player currently owns that spot or 'None' if the spot is not taken.
	PlayerType gameBoard[3][3];
};

///////////////////////////////////////////////////////////////////////////////////
// Contains all player related data
///////////////////////////////////////////////////////////////////////////////////
struct Player
{
	// ID of the player
	int id;
	// Number of games this player has played
	int gamesPlayed;
	// Number of games this player won
	int winCount;
	// Number of games this player lost
	int loseCount;
	// Number of games this player tied
	int drawCount;
	// Type of player this player represents
	PlayerType type;
	// Pointer to the pool of games. See GamePool for more details.
	struct GamePool *gamePool;
	// Pointer to the pool of players. See PlayerPool for more details.
	struct PlayerPool *playerPool;
	// random number generator for this thread
	UniformRandInt myRand;
	//MY VARIABLES
	int* ready;
	std::mutex* count;
	bool* isGunOpen;
	std::mutex* gun;
	std::condition_variable* gunCV;
	std::condition_variable* mainCV; //count
};

///////////////////////////////////////////////////////////////////////////////////
// Holds all of the games
///////////////////////////////////////////////////////////////////////////////////
struct GamePool
{ 
	// An array of game specific data with exactly one entry for each game. See Game for more details.
	Game *perGameData;
	// Total number of games and the number of entries in perGameData
	int totalGameCount;
};

///////////////////////////////////////////////////////////////////////////////////
// This structure is provided to store data for keeping track of the total number
//   of player threads in a thread-safe manner and implementing the starting gun
//   logic.
///////////////////////////////////////////////////////////////////////////////////
struct PlayerPool
{ 

};

///////////////////////////////////////////////////////////////////////////////////
// Prompts the user to press enter and waits for user input
///////////////////////////////////////////////////////////////////////////////////
void Pause()
{
	printf("Press Enter to continue\n");
	getchar();
}

///////////////////////////////////////////////////////////////////////////////////
// Controls all access to the lockable object used for synchronizing output.
//
// Arguments:
//   operationToperform - Operation we'll be performing on the lockable object.
// 
// Note: 
//   This function will primarily be used in your synchronized print function,
//   but in some cases you may need to use it to print several strings while
//   still preventing other threads from printing until you're done.
//
// Example:
//   The following will print "Hi!" as three separate prints, but because we
//   locked our printing object no other threads will be able to interrupt us
//   until we've invoked the Unlock operation.
//
//       LogSync(LogSyncOperation::Lock);
//       printf("H");
//       printf("i");
//       printf("!");
//       LogSync(LogSyncOperation::Unlock);
///////////////////////////////////////////////////////////////////////////////////
void LogSync(LogSyncOperation operationToPerform)
{

	static std::mutex logTex = std::mutex();

	switch (operationToPerform)
	{
	case(LogSyncOperation::Lock):
	{
		logTex.lock();
		break;
	}
	case(LogSyncOperation::Unlock) :
	{
		logTex.unlock();
		break;
	}
	}

}

///////////////////////////////////////////////////////////////////////////////////
// Prints a formatted string to the standard output in a thread safe manner. 
//
// Arguments:
//   format - Format of string to print.
//   ... - Additional arguments. See documentation for printf().
//
// Returns: 
//   Result of vprintf
///////////////////////////////////////////////////////////////////////////////////
int Log(const char *format, ...)
{
	int result = 0;

	LogSync(LogSyncOperation::Lock);

	va_list args;
	va_start(args, format);
	fputs("Error: ", stderr);
	result = vfprintf(stderr, format, args);
	va_end(args);

	LogSync(LogSyncOperation::Unlock);

	return result;
}

///////////////////////////////////////////////////////////////////////////////////
// Prints the current game board to the console
//
// Arguments:
//   currentGame - Pointer to the game that is being joined
///////////////////////////////////////////////////////////////////////////////////
void PrintGameBoard(const Game *currentGame)
{
	///////////////////////////////////////////////////////////////////////////////////
	//   Print the game board to the screen as a single block of text without 
	//   interruption from other threads that are attempting to print. Any other thread
	//   that attempts to print must block until the entire game board has been printed.
	//   Do not use Log in this function, use printf.
	///////////////////////////////////////////////////////////////////////////////////
	LogSync(LogSyncOperation::Lock);
	for (int row = 0; row < 3; row++)
	{
		for(int col = 0; col < 3; col++)
		{
			if (currentGame->gameBoard[row][col] == PlayerType::None)
			{
				printf("[ ]");
			}
			else
			{
				printf("[%c]", (currentGame->gameBoard[row][col] == PlayerType::X) ? 'X' : 'O');
			}
			std::this_thread::yield();
		}
		printf("\n");
	}
	LogSync(LogSyncOperation::Unlock);

}

///////////////////////////////////////////////////////////////////////////////////
// Determines if the player made a winning move on the game board
//
// Arguments:
//   row - The row 'player' picked to play at
//   col - The column 'player' picked to play at
//   game - Pointer to the game being checked
//   player - Pointer to the player that made the move
//
// Return:
//   True if player won, otherwise false
///////////////////////////////////////////////////////////////////////////////////
bool DidWeWin(int row, int col, const Game *game, const Player *player)
{
	bool completeRow = true;
	bool completeCol = true;
	bool completeDiagonalA = true;
	bool completeDiagonalB = true;

	// Check specified row, column, and both diagonals to see if this player made
	//  a winning move.
	for (int i = 0; i < 3; i++)
	{
		if(game->gameBoard[row][i] != player->type)
			completeRow = false;
		if(game->gameBoard[i][col] != player->type)
			completeCol = false;
		if(game->gameBoard[i][i] != player->type)
			completeDiagonalA = false;
		if(game->gameBoard[2 - i][i] != player->type)
			completeDiagonalB = false;
	}

	return completeRow || completeCol || completeDiagonalA || completeDiagonalB;
}

///////////////////////////////////////////////////////////////////////////////////
// Play the entire game of Tic-Tac-Toe as 'currentPlayer' in 'currentGame'
//
// Arguments:
//   currentPlayer - Pointer to the player that is trying to play each game
//   currentGame - Pointer to the game that is being joined
//
//  gameUniqueLock is the primary synchronization object 
///////////////////////////////////////////////////////////////////////////////////
GameState MakeAMove(Player *currentPlayer, Game *currentGame)
{
	int possibleMoves[9];
	int totalPossibleMoves = 0;

	// Find all valid moves this player can make
	for (int row = 0; row < 3; row++)
	{
		for(int col = 0; col < 3; col++)
		{
			if (currentGame->gameBoard[row][col] == PlayerType::None)
			{
				possibleMoves[totalPossibleMoves++] = (row * 3) + col;
			}
		}
	}

	if (totalPossibleMoves != 0) 
	{ 
		// There are valid moves left on the board, pick a random valid location
		int randomMoveIndex = currentPlayer->myRand() % totalPossibleMoves;

		int row = possibleMoves[randomMoveIndex] / 3;
		int col = possibleMoves[randomMoveIndex] % 3;
		currentGame->gameBoard[row][col] = currentPlayer->type;

		Log("Game %d: Player %d: Picked [Row: %d, Col: %d]\n", currentGame->gameNumber, currentPlayer->id, row, col);

		if (DidWeWin(row, col, currentGame, currentPlayer)) 
		{ 
			Log("Game %d:Player %d - Won\n", currentGame->gameNumber, currentPlayer->id);
			currentPlayer->winCount++;

			return GameState::Won;
		}
		else 
		{ 
			return GameState::StillPlaying;
		}
	}

	// There are no more moves left, game resulted in a draw.
	Log("Game %d:Player %d - Draw\n", currentGame->gameNumber, currentPlayer->id);
	currentPlayer->drawCount++;

	return GameState::Draw;
}

///////////////////////////////////////////////////////////////////////////////////
// Play the entire game of Tic-Tac-Toe as 'currentPlayer' in 'currentGame'
//
// Arguments:
//   currentPlayer - Pointer to the player that is trying to play each game
//   currentGame - Pointer to the game that is being joined
//
//  gameUniqueLock is the primary synchronization object 
///////////////////////////////////////////////////////////////////////////////////
void PlayGame(Player *currentPlayer, Game *currentGame)
{
	Log("Game %d:Player %d vs Player %d (Player %d) starting\n", currentGame->gameNumber, currentGame->playerX, currentGame->playerO, currentPlayer->id);

	if(currentGame->playerO == -1 || currentGame->playerX == -1)
	{
		Log("ERROR: Playing game with only one player present. Did you forget to wait for the second player in JoinGame()?\n");
		Pause();
		exit(1);
	}

	while (currentGame->currentGameState == GameState::StillPlaying) 
	{ 
		if (currentGame->currentTurn != currentPlayer->type) 
		{
			Log("ERROR: Wrong player is playing. You broke it.\n");
			Pause();
			exit(1);
		}

		currentGame->currentTurn = (currentPlayer->type == PlayerType::X) ? PlayerType::O : PlayerType::X;

		// Make a move on the game board. The result of this function will determine the current state of the board.
		currentGame->currentGameState = MakeAMove(currentPlayer, currentGame);
		PrintGameBoard(currentGame);

		switch (currentGame->currentGameState)
		{
		case GameState::StillPlaying:
			///////////////////////////////////////////////////////////////////////////////////
			//   The game is not over yet. We need to notify the other player that it's
			//   their turn and then we must wait until they tell us it's our turn.
			///////////////////////////////////////////////////////////////////////////////////
		{
			currentGame->gameCondition.notify_one();
			currentGame->gameCondition.wait(*currentGame->gameUniqueLock);

			continue;
		}
		case GameState::Won:
			///////////////////////////////////////////////////////////////////////////////////
			//   We have won the game, we must wake up the other player so they can break
			//   out of the PlayGame function.
			///////////////////////////////////////////////////////////////////////////////////
		{
			currentGame->gameCondition.notify_one();
			return;
		}
		case GameState::Draw:
			///////////////////////////////////////////////////////////////////////////////////
			//   The game ended in a tie, we must wake up the other player so they can break
			//   out of the PlayGame function
			///////////////////////////////////////////////////////////////////////////////////
		{
			currentGame->gameCondition.notify_one();
			return;
		}
		}
	}

	// Only one player will execute this logic. The winning/Tied player will exit this function
	//   upon finding out the game is over.
	if (currentGame->currentGameState == GameState::Won) 
	{ 
		Log("Game %d:Player %d - Lost\n", currentGame->gameNumber, currentPlayer->id);
		(currentPlayer->loseCount)++;
	}
	else if(currentGame->currentGameState == GameState::Draw) 
	{ 
		Log("Game %d:Player %d - Draw\n", currentGame->gameNumber, currentPlayer->id);
		(currentPlayer->drawCount)++; // count draw
	}
}

///////////////////////////////////////////////////////////////////////////////////
// Makes 'currentPlayer' join 'currentGame' and either waits for another player to
//  join or begins playing the game if both players are now present.
//
// Arguments:
//   currentPlayer - Pointer to the player that is trying to play each game
//   currentGame - Pointer to the game that is being joined
///////////////////////////////////////////////////////////////////////////////////
void JoinGame(Player *currentPlayer, Game *currentGame)
{
	// The player thread has joined a game and will begin playing it now.
	std::unique_lock<std::mutex> gameUniqueLock(currentGame->gameMutex);
	currentGame->gameUniqueLock = &gameUniqueLock;

	if (currentGame->playerO == -1)
	{
		Log("Player %d joining game %d as 'O'\n", currentPlayer->id, currentGame->gameNumber);

		currentGame->playerO = currentPlayer->id;
		currentPlayer->type = PlayerType::O;

		///////////////////////////////////////////////////////////////////////////////////
		//   We're the only player in the game right now so we need to wait for the 
		//   other player to join the game and play it's turn.
		///////////////////////////////////////////////////////////////////////////////////

		currentGame->gameCondition.wait(*currentGame->gameUniqueLock);
	}
	else 
	{
		Log("Player %d joining game %d as 'X'\n", currentPlayer->id, currentGame->gameNumber);

		currentGame->playerX = currentPlayer->id;
		currentPlayer->type = PlayerType::X;


	}



	PlayGame(currentPlayer, currentGame);
	currentGame->gameUniqueLock = nullptr;
	currentPlayer->gamesPlayed++;
	gameUniqueLock.unlock();
}

///////////////////////////////////////////////////////////////////////////////////
// Makes the specified player try to sequentially join and play each game in the
//   pool of games.
//
// Arguments:
//   currentPlayer - Pointer to the player that is trying to play each game
///////////////////////////////////////////////////////////////////////////////////
void TryToPlayEachGame(Player *currentPlayer)
{
	Log("Player %d starting to play games...\n", currentPlayer->id);

	Game *listOfGames = currentPlayer->gamePool->perGameData;
	int totalGameCount = currentPlayer->gamePool->totalGameCount;

	// All of our player threads will be going through the pool of games looking for the any
	//   games that aren't full. The player will join and play any non-full games it finds while
	//   iterating through the list of games.
	for (int i = 0; i < totalGameCount; i++) 
	{
		// Check to see if we can join this game
		listOfGames[i].playerCountMutex.lock();
		if (listOfGames[i].playerCount == 2)
		{
			// Game is full, skip it
			listOfGames[i].playerCountMutex.unlock();
			continue;
		}

		// This game wasn't full so we will join it
		listOfGames[i].playerCount++;
		listOfGames[i].playerCountMutex.unlock();

		// We joined the game so we can start playing it
		JoinGame(currentPlayer, &listOfGames[i]);
	}
}

///////////////////////////////////////////////////////////////////////////////////
// Entry point for player threads. 
//
// Arguments:
//   currentPlayer - Pointer to a player struct that is unique to this thread
///////////////////////////////////////////////////////////////////////////////////
void PlayerThreadEntrypoint(Player *currentPlayer)
{
	Log("Player %d waiting on starting gun\n", currentPlayer->id);

	///////////////////////////////////////////////////////////////////////////////////
	//   Let main know there's one more player thread running then wait for a
	//   notification from main via the condition variable you created in the PlayerPool
	//   struct.
	///////////////////////////////////////////////////////////////////////////////////

	currentPlayer->count->lock();

	(*currentPlayer->ready)++;
	currentPlayer->mainCV->notify_one();
	
	std::unique_lock<std::mutex> locked(*currentPlayer->gun);
	currentPlayer->count->unlock();
	currentPlayer->gunCV->wait(locked, [currentPlayer](){return *currentPlayer->isGunOpen == true; });

	// Attempt to play each game, all of the game logic will occur in this function
	Log("Player %d running\n", currentPlayer->id);
	
	locked.unlock();
	
	TryToPlayEachGame(currentPlayer);

	///////////////////////////////////////////////////////////////////////////////////
	//   Let main know there's one less player thread running.
	///////////////////////////////////////////////////////////////////////////////////

	currentPlayer->count->lock();

	(*currentPlayer->ready)--;
	currentPlayer->mainCV->notify_one();

	currentPlayer->count->unlock();
}

///////////////////////////////////////////////////////////////////////////////////
// Displays the results of all players and all games to the console.
//
// Arguments:
//   perPlayerData - An array of player structs; one entry for each player.
//   totalPlayerCount - Total number of players
//   perGameData - An array of game data; one entry for each game.
//   gameCount - Total number of games
///////////////////////////////////////////////////////////////////////////////////
void PrintResults(const Player *perPlayerData, int totalPlayerCount, const Game *perGameData, int totalGameCount)
{
	int totalGamesWon = 0;
	int totalGamesTied = 0;
	int totalPlayerWins = 0;
	int totalPlayerLoses = 0;
	int totalPlayerTies = 0;

	printf("********* Player Results **********\n");
	for (int i = 0; i < totalPlayerCount; i++) 
	{
		printf("Player %d, Played %d game(s), Won %d, Lost %d, Draw %d\n",
			perPlayerData[i].id,
			perPlayerData[i].gamesPlayed,
			perPlayerData[i].winCount,
			perPlayerData[i].loseCount,
			perPlayerData[i].drawCount
		); 

		totalPlayerWins += perPlayerData[i].winCount;
		totalPlayerLoses += perPlayerData[i].loseCount;
		totalPlayerTies += perPlayerData[i].drawCount;
	}

	printf("Total Players %d, Wins %d, Losses %d, Draws %d\n\n\n", totalPlayerCount, totalPlayerWins, totalPlayerLoses, (totalPlayerTies / 2));

	printf("********* Game Results **********\n");
	for (int i = 0; i < totalGameCount; i++)
	{
		printf("Game %d - 'X' player %d, 'O' player %d, game result %s\n", 
			perGameData[i].gameNumber,
			perGameData[i].playerX,
			perGameData[i].playerO,
			((perGameData[i].currentGameState == GameState::Won) ? "Won" : "Draw")
		);

		if (perGameData[i].currentGameState == GameState::Won)
		{
			totalGamesWon++;
		}
		else
		{
			totalGamesTied++;
		}
	}
	printf("Total Games = %d, %d Games Won, %d Games were a Draw\n\n\n", totalGameCount, totalGamesWon, totalGamesTied);
}

int main(int argc, char **argv)
{
	ENABLE_LEAK_DETECTION();


	// Total number of games we're going to be playing.
	int totalGameCount;	
	// Total number of players that will be playing.
	int totalPlayerCount;
	// An array of player specific data with exactly one entry for each player. See Player for more details.
	Player *perPlayerData;
	// Contains all data needed to keep track of players. See PlayerPool for more details.
	PlayerPool poolOfPlayers;
	// An array of game specific data with exactly one entry for each game. See Game for more details.
	Game *perGameData;
	// Contains all of the games. See GamePool for more details.
	GamePool poolOfGames; 

	if (argc != 3)
	{
		fprintf(stderr, "Usage: TicTacToe gameCount playerCount\n\n");
		fprintf(stderr, "Arguments:\n");
		fprintf(stderr, "    gameCount                    Number of games.                              \n");
		fprintf(stderr, "    playerCount                  Number of players.                            \n");
		Pause();
		return 1;
	}
	totalGameCount = atoi(argv[1]);
	totalPlayerCount = atoi(argv[2]);

	if(totalGameCount < 0 || totalPlayerCount < 0)
	{
		fprintf(stderr, "Error: All arguments must be positive integer values.\n");
		Pause();
		return 1;
	}

	if(totalPlayerCount < 2)
	{
		fprintf(stderr, "Error: Requires at least two players.\n");
		Pause();
		return 1;
	}

	Log("%s starting %d player(s) for %d game(s)\n", argv[0], totalPlayerCount, totalGameCount);

	// Allocate and array of players
	perPlayerData = new Player[totalPlayerCount];

	// Allocate array of games
	perGameData = new Game[totalGameCount];

	// Initialize pool of games
	poolOfGames.perGameData = perGameData;
	poolOfGames.totalGameCount = totalGameCount;

	std::condition_variable myMainCV = std::condition_variable(); //count
	std::condition_variable mygunCV = std::condition_variable();
	std::mutex countMutex = std::mutex(); 
	std::mutex gunMutex = std::mutex(); 
	int readyPlayers = 0;
	bool gunopen = false;

	// Initialize each game
	for (int i = 0; i < totalGameCount; i++) 
	{
		perGameData[i].playerO = -1;
		perGameData[i].playerX = -1;
		perGameData[i].gameNumber = i + 1;
		perGameData[i].currentTurn = PlayerType::X;
		perGameData[i].currentGameState = GameState::StillPlaying;
		perGameData[i].playerCount = 0;
		memset(perGameData[i].gameBoard, 0, sizeof(perGameData[i].gameBoard));
	}

	// Initialize each player
	for (int i = 0; i < totalPlayerCount; i++) 
	{
		perPlayerData[i].id = i;
		perPlayerData[i].drawCount = 0;
		perPlayerData[i].gamesPlayed = 0;
		perPlayerData[i].loseCount = 0;
		perPlayerData[i].winCount = 0;
		perPlayerData[i].gamePool = &poolOfGames;
		perPlayerData[i].playerPool = &poolOfPlayers;
		perPlayerData[i].type = PlayerType::None;
		perPlayerData[i].myRand.Init(0, INT_MAX);
		//Mein
		perPlayerData[i].ready = &readyPlayers;
		perPlayerData[i].count = &countMutex;
		perPlayerData[i].gun = &gunMutex;
		perPlayerData[i].isGunOpen = &gunopen;
		perPlayerData[i].gunCV = &mygunCV;
		perPlayerData[i].mainCV = &myMainCV;
	}

	///////////////////////////////////////////////////////////////////////////////////
	//   Start the player threads. The player threads should begin executing in
	//   the PlayerThreadEntrypoint function. Make sure to detach the threads.
	///////////////////////////////////////////////////////////////////////////////////
	for (int i = 0; i < totalPlayerCount; i++)
	{
		std::thread(PlayerThreadEntrypoint, &perPlayerData[i]).detach();
	}
	///////////////////////////////////////////////////////////////////////////////////
	// Wait for all players to be ready 
	///////////////////////////////////////////////////////////////////////////////////

	std::unique_lock<std::mutex> locked(countMutex);
	myMainCV.wait(locked, [&readyPlayers, &totalPlayerCount](){return readyPlayers == totalPlayerCount; });

	///////////////////////////////////////////////////////////////////////////////////
	// Notify all waiting threads that they can start playing.
	///////////////////////////////////////////////////////////////////////////////////
	gunMutex.lock();
	gunopen = true;
	mygunCV.notify_all();
	gunMutex.unlock();

	///////////////////////////////////////////////////////////////////////////////////
	// Wait for all detached player threads to complete.
	///////////////////////////////////////////////////////////////////////////////////

	myMainCV.wait(locked, [&readyPlayers](){return readyPlayers == 0; });

	PrintResults(perPlayerData, totalPlayerCount, perGameData, totalGameCount);

	///////////////////////////////////////////////////////////////////////////////////
	// Cleanup
	///////////////////////////////////////////////////////////////////////////////////

	delete [] perPlayerData;
	delete [] perGameData;

	Pause();
	return 0;
}
