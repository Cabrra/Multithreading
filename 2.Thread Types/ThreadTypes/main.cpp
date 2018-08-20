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
#include <condition_variable>
#include <thread> 
#include <chrono>
#include <random>

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
};

struct ThreadStruct
{
	int id;									// thread number
	UniformRandInt myRand;					// random number generator for this thread
	
	std::mutex* Mutex;
	std::mutex* conditionMutex;
	std::condition_variable* cv;
	int* detachNumber;
	bool* detachFlag;
};

///////////////////////////////////////////////////////////////////////////////////
// Prompts the user to press enter and waits for user input
///////////////////////////////////////////////////////////////////////////////////
void Pause()
{
	printf("Press enter to continue\n");
	getchar();
}

////////////////////////////////////////////////////////////////////////////////////
//   The purpose of this function is to delay the printing of 'work' until it's been generated.
////////////////////////////////////////////////////////////////////////////////////

void WorkDelay(std::promise<int> &myProm, ThreadStruct *threadData)
{
	std::future<int> myFut = myProm.get_future();
	printf("FINISH: Joinable thread %d, finished with value %d\n", threadData->id, myFut.get());
}

///////////////////////////////////////////////////////////////////////////////////
// Entry point for joinable threads. 
//
// Arguments:
//   threadData - Pointer to the thread specific data
///////////////////////////////////////////////////////////////////////////////////
void JoinableThreadEntrypoint(ThreadStruct *threadData)
{
	std::promise<int> Promeise;

	std::thread waitThread = std::thread(WorkDelay, std::ref(Promeise), threadData);

	int workLimit = (threadData->id + 1) + (threadData->myRand());
	int work = 0;

	// Performs some arbitrary amount of work.
	printf("START: Joinable Thread %d, starting limit = %d\n", threadData->id, workLimit);
	for (int i = 0; i < workLimit; i += (threadData->id + 1)) 
	{ 
		work++;
	}

	printf("FINISH: Joinable thread %d, finished with value %d\n", threadData->id, work);

	Promeise.set_value(work);
	WAIT_FOR_THREAD(&waitThread);
}

///////////////////////////////////////////////////////////////////////////////////
// Entry point for detached threads. 
//
// Arguments:
//   threadData - Pointer to the thread specific data
///////////////////////////////////////////////////////////////////////////////////
void DetachedThreadEntrypoint(ThreadStruct *threadData)
{
	std::promise<int> Promeise;

	std::thread waitThread = std::thread(WorkDelay, std::ref(Promeise), threadData);


	int workLimit = (threadData->id + 1) + (threadData->myRand());
	int work = 0;

	printf("START: Detached Thread %d, starting limit = %d\n", threadData->id, workLimit);
	while(true)
	{ 
		//Break out of the work loop in a thread safe way.
		
		threadData->Mutex->lock();
		if (*threadData->detachFlag == true)
		{
			threadData->Mutex->unlock();
			break;
		}
		threadData->Mutex->unlock();

		// Performs some arbitrary amount of work.
		for (int i = 0; i < workLimit; i += (threadData->id + 1)) 
		{ 
			work++;
		}
	}
	printf("FINISH: Detached thread %d, finished with value %d\n", threadData->id, work);

	//Set the std::promise's value and wait for the printer thread to finish.

	Promeise.set_value(work);
	WAIT_FOR_THREAD(&waitThread);

	//Let main know there's one less thread running.
	
	threadData->conditionMutex->lock();
	(*threadData->detachNumber)--;
	threadData->conditionMutex->unlock();

	threadData->cv->notify_one();
}

int main(int argc, char **argv)
{
	ENABLE_LEAK_DETECTION();

	if (argc != 2)
	{
		fprintf(stderr, "Usage: ThreadTypes threadCount\n\n");
		fprintf(stderr, "Arguments:\n");
		fprintf(stderr, "    threadCount                  Number of joinable and detached threads to    \n");
		fprintf(stderr, "                                 create.                                       \n");
		Pause();
		return 1;
	}

	int totalThreadCount = 2 * atoi(argv[1]);
	
	if(totalThreadCount < 0)
	{
		fprintf(stderr, "Error: All arguments must be positive integer values.\n");
		Pause();
		return 1;
	}

	ThreadStruct *perThreadData = new ThreadStruct[totalThreadCount];
	printf("Main thread starting %d thread(s)\n", totalThreadCount);

	std::vector<std::thread*> myThreads;
	std::condition_variable myCV = std::condition_variable();
	int number = (int)totalThreadCount / 2;
	std::mutex myMutex = std::mutex();
	std::mutex threMutex = std::mutex();
	bool myFlag = false;

	for(int i = totalThreadCount - 1; i >= 0; i--)
	{		
		perThreadData[i].id = i;
		perThreadData[i].myRand.Init(0, 100);

		perThreadData[i].detachNumber = &number;
		perThreadData[i].cv = &myCV;
		perThreadData[i].detachFlag = &myFlag;
		perThreadData[i].Mutex = &threMutex;
		perThreadData[i].conditionMutex = &myMutex;



		if (i % 2 == 0) //joinable
		{
			std::thread*  thre = new std::thread(JoinableThreadEntrypoint, &perThreadData[i]);
			myThreads.push_back(thre);
		}
		else //detached
		{
			std::thread(DetachedThreadEntrypoint, &perThreadData[i]).detach();
		}
		
	}

	//Wait for all joinable threads to complete
	
	for (int i = 0; i < totalThreadCount/2; i++)
	{
		WAIT_FOR_THREAD(myThreads[i]);
	}
	
	threMutex.lock();
	myFlag = true;
	threMutex.unlock();

	//Wait for all detached threads to finish without using a busy-wait loop.
	std::unique_lock<std::mutex> locked(myMutex);
	myCV.wait(locked, [&number](){return number == 0; });

	//Cleanup
	
	for (int i = 0; i < totalThreadCount/2; i++)
	{
		delete myThreads[i];
	}

	delete[] perThreadData;
	
	Pause();
	
	return 0;
}
