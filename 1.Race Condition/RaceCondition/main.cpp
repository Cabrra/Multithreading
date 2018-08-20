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

#define WAIT_FOR_THREAD(r) if ((r)->joinable()) (r)->join();

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
using namespace std;

struct ThreadStruct
{
	// ID of the thread
	int id;
	// Length of the shared string
	int sharedStringLength;
	// Number of strings a single thread will generate
	int numberOfStringsToGenerate;
	// Amount of time to sleep before generating strings
	int waitTime;
	// Shared string that will be generate in each thread. This memory is shared among all threads.
	char *sharedString;
	//my data
	int runType;
	std::mutex* Mutex;
	std::condition_variable* cv;
	int* currID;
};



///////////////////////////////////////////////////////////////////////////////////////////
// Prompts the user to press enter and waits for user input
///////////////////////////////////////////////////////////////////////////////////////////
void Pause()
{
	printf("Press enter to continue\n");
	getchar();
}

///////////////////////////////////////////////////////////////////////////////////
// Entry point for worker threads. 
//
// Arguments:
//   threadData - Pointer to per-thread data for this thread.
///////////////////////////////////////////////////////////////////////////////////
void ThreadEntryPoint(ThreadStruct *threadData)
{
	if (threadData->runType == 2)
		threadData->Mutex->lock();
	if (threadData->runType == 3)
	{
		std::unique_lock<std::mutex> locked(*threadData->Mutex);
		threadData->cv->wait(locked, [threadData](){return *threadData->currID == threadData->id; });
	}

	for(int i = 0; i < threadData->numberOfStringsToGenerate; i++)
	{
			if (threadData->waitTime != 0)
			{
				// Blocks the current thread for a given amount of time.
				std::this_thread::sleep_for(std::chrono::milliseconds(threadData->waitTime));
			}

			if (threadData->runType == 1) //type 1
				threadData->Mutex->lock();
			for (int j = 0; j < threadData->sharedStringLength; j++)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				threadData->sharedString[j] = 'A' + threadData->id;
			}
			
			printf("Thread %d: %s\n", threadData->id, threadData->sharedString);

			if (threadData->runType == 1) //type 1
				threadData->Mutex->unlock();
	}
	if (threadData->runType == 2)
		threadData->Mutex->unlock();
	if (threadData->runType == 3)
	{
		(*threadData->currID)++;
		threadData->cv->notify_all();
	}
}

int main(int argc, char** argv)
{
	ENABLE_LEAK_DETECTION();

	int threadCount = 0;
	int sharedStringLength = 0;
	int numberOfStringsToGenerate = 0;
	int waitTime = 0;
	char *sharedString = nullptr;
	int runType = 0;
	ThreadStruct *perThreadData = nullptr;

	if (argc - 1 != 5)
	{
		fprintf(stderr, "Error: missing or incorrect command line arguments\n\n"); 
		fprintf(stderr, "Usage: RaceCondition threadCount sharedStringLength numberOfStringsToGenerate waitTime runType\n\n");
		fprintf(stderr, "Arguments:\n");
		fprintf(stderr, "    threadCount                  Number of threads to create.\n");
		fprintf(stderr, "    sharedStringLength           Length of string to generate.\n");
		fprintf(stderr, "    numberOfStringsToGenerate    Number of strings to generate per thread.\n");
		fprintf(stderr, "    waitTime                     Time to wait before generating the string.\n");
		fprintf(stderr, "    runType                      The run type.\n\n");
		Pause();
		return 1;
	}

	threadCount = atoi(argv[1]);
	sharedStringLength = atoi(argv[2]);
	numberOfStringsToGenerate = atoi(argv[3]);
	waitTime = atoi(argv[4]);
	runType = atoi(argv[5]);

	if(threadCount < 0 || sharedStringLength < 0 || numberOfStringsToGenerate < 0 || waitTime < 0 || runType < 0)
	{
		fprintf(stderr, "Error: All arguments must be positive integer values.\n");
		Pause();
		return 1;
	}

	printf("%d thread(s), string sharedStringLength %d, %d iterations, %d ms pause\n",
		threadCount, sharedStringLength, numberOfStringsToGenerate, waitTime);
	
	sharedString = new char[sharedStringLength + 1];
	memset(sharedString, 0, sharedStringLength + 1);
	perThreadData = new ThreadStruct[threadCount];
	
	//container to store the thread classes
	std::vector<std::thread*> myThreads;
	std::mutex myMutex = std::mutex();
	std::condition_variable myCV = std::condition_variable();
	int currentID = 0;

	for (int i = threadCount - 1; i >= 0; i--)
	{
		perThreadData[i].id = i;
		perThreadData[i].sharedStringLength = sharedStringLength;
		perThreadData[i].numberOfStringsToGenerate = numberOfStringsToGenerate;
		perThreadData[i].waitTime = waitTime;
		perThreadData[i].sharedString = sharedString;
		//my variables
		perThreadData[i].runType = runType;
		perThreadData[i].Mutex = &myMutex;
		perThreadData[i].cv = &myCV;
		perThreadData[i].currID = &currentID;

		//Setup any additional variables in perThreadData and start the threads.

		std::thread* thre = new std::thread(ThreadEntryPoint, &perThreadData[i]);

		myThreads.push_back(thre);
	}


	///////////////////////////////////////////////////////////////////////////////////
	//   Wait for all of the threads to finish. Since we are using
	//   Joinable threads we must Join each one. Joining a thread will cause
	//   the calling thread (main in this case) to block until the thread being
	//   joined has completed executing.
	///////////////////////////////////////////////////////////////////////////////////	

	for (int i = 0; i < threadCount; i++)
		WAIT_FOR_THREAD(myThreads[i]);

	for (int i = 0; i < threadCount; i++)
	{
		delete myThreads[i];
	}

	delete[] sharedString;
	delete[] perThreadData;

	Pause();
	return 0;
}
