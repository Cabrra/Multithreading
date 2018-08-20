#include <iostream>  
#include <vector>
#include <condition_variable>
#include <thread> 
#include <chrono>
#include <random>
#include <stdarg.h>
#include <stdio.h>

using namespace std;

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

///////////////////////////////////////////////////////////////////////////////////
// The various types of resources that can be found in the pool. An 'Unknown'
//   should never appear unless the logic is incorrect.
///////////////////////////////////////////////////////////////////////////////////
enum class ResourceType
{
	Unknown,
	Bottle,
	Opener
};

///////////////////////////////////////////////////////////////////////////////////
// Contains all resource-specific data. Each resource has its own unique mutex.
///////////////////////////////////////////////////////////////////////////////////
struct Resource
{
	// ID of the resource. For output purposes only.
	int id;
	// Number of times the resource has been 'used'. A resource is 'used' whenever
	//   a drinker drinks.
	int useCount;
	// Number of times the resource has been locked.
	int lockCount;
	// The resource type. Either a Bottle or an Opener
	ResourceType type;
	// The resource mutex.
	std::mutex resourceMutex;
};

///////////////////////////////////////////////////////////////////////////////////
// Contains all of the data for drinkers
///////////////////////////////////////////////////////////////////////////////////
struct DrinkerPool 
{
	// Total number of drinkers allocated in the 'drinkers' array.
	int totalDrinkers;
	// Current number of drinkers currently running.
	int drinkerCount;
	// Flag to break the drinkers out of their drinking loop.
	bool stopDrinkingFlag;
	// A mutex to control access to the drinkerCount
	std::mutex drinkerCountMutex;
	// The condition variable to be used with the drinkerCount
	std::condition_variable drinkerCountCondition;
	// The mutex to control access to the starting gun
	std::mutex startingGunMutex;
	// The condition variable used to notify and wait for changes on the starting gun
	std::condition_variable startingGunCondition;
	// An array of drinkers
	struct Drinker *drinkers;
	bool* isGunOpen;
};

///////////////////////////////////////////////////////////////////////////////////
// Holds and of the resources. Has a mutex+condition pair to let threads know when
//   resources become available in the pool.
///////////////////////////////////////////////////////////////////////////////////
struct ResourcePool
{
	// Total number of resources allocated in the resource pool
	int totalResources;
	// The mutex used to control access to the poolCondition
	std::mutex poolMutex;
	// The condition variable used to notify and wait for changes in the resource pool.
	std::condition_variable poolCondition;
	// An array of resources
	Resource *resources;
};

///////////////////////////////////////////////////////////////////////////////////
// Contains all drinker specific information. Each drinker has a pointer to an opener
//   and a pointer to a bottle. When the drinker goes to drink it will use these two
//   resources.
///////////////////////////////////////////////////////////////////////////////////
struct Drinker 
{
	// ID of the drinker. For output purposes only.
	int id;
	// Number of times the drinker has taken a drink.
	int drinkCount;
	// Number of resources the drinker has Tried to lock
	int resourceTryCount;
	// A pointer to a bottle resource to use when drinking.
	Resource *bottle;
	// A pointer to an opener resource to use when drinking.
	Resource *opener;
	// A pointer to the pool of drinkers
	DrinkerPool *drinkerPool;
	// A pointer to the pool of resources
	ResourcePool *resourcePool;
	// Random number generator for this thread
	UniformRandInt myRand;
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
// Causes the specified drinker to drink.
//
// Arguments:
//   currentDrinker - The current drinker
///////////////////////////////////////////////////////////////////////////////////
void Drink(Drinker *currentDrinker)
{
	int drinkTime = 20 + (currentDrinker->myRand() % 20);
	int drunkTime = 40 + (currentDrinker->myRand() % 10);
	int bathroomTime = 60 + (currentDrinker->myRand() % 10);

	currentDrinker->bottle->useCount++;
	currentDrinker->opener->useCount++;

	std::this_thread::sleep_for(std::chrono::milliseconds(drinkTime)); 

	// We are done drinking so release the bottle and opener
	currentDrinker->bottle->resourceMutex.unlock();
	currentDrinker->opener->resourceMutex.unlock();
	currentDrinker->bottle = nullptr;
	currentDrinker->opener = nullptr;
	currentDrinker->drinkCount++;

	if ((currentDrinker->drinkCount % 5) ==  0)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(drunkTime));
	}
	else if((currentDrinker->drinkCount % 10) == 0)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(bathroomTime));
	}
}

///////////////////////////////////////////////////////////////////////////////////
// Attempts to acquire a bottle and an opener resource.
//
// Arguments:
//   currentDrinker - The current drinker
//
// Return:
//   True if both resources were successfully acquired by the drinker
///////////////////////////////////////////////////////////////////////////////////
bool TryToGetResources(Drinker *currentDrinker)
{
	int totalResources = currentDrinker->resourcePool->totalResources;
	int trying = currentDrinker->myRand() % totalResources;

	currentDrinker->resourceTryCount++;

	Resource *firstResource = &currentDrinker->resourcePool->resources[trying];
	firstResource->resourceMutex.lock();
	firstResource->lockCount++; 

	if (firstResource->type == ResourceType::Bottle)
	{
		currentDrinker->bottle = firstResource;
	}
	else
	{
		currentDrinker->opener = firstResource;
	}

	///////////////////////////////////////////////////////////////////////////////////
	//    Acquire the second resource. The first resource has already been acquired.
	//    Each resource has a mutex that will determine which thread 'owns' the specified 
	//    resource. If the mutex is not locked then that resource is free to be acquire by a 
	//    drinker.
	//    Each resource also has a lock count and each drinker has its own a try count, both 
	//    of which should always represent the number of times a resource has been locked
	//    or the number of times the drinker has tried to lock a resource.
	//
	// Example of the potential deadlock with two threads and two resources:
	//   * Thread1 locks Resource1
	//   * Thread2 locks Resource2
	//   * Thread1 locks Resource2 and blocks because Thread2 has it locked.
	//   * Thread2 locks Resource1 and blocks because Thread1 has it locked.
	//  Both threads are now deadlocked.
	//
	//  Example of the avoided deadlock with two threads and two resources:
	//   * Thread1 locks Resource1
	//   * Thread2 locks Resource2
	//   * Thread1 tries to lock Resource2, but doesn't because it's already locked
	//   * Thread2 tries to lock Resource1, but doesn't because it's already locked
	//  No deadlock.
	///////////////////////////////////////////////////////////////////////////////////

	for (int i = 0; i < totalResources; i++)
	{
		Resource *second = &currentDrinker->resourcePool->resources[i];
		if (firstResource->type == ResourceType::Bottle)
		{
			if (second->type == ResourceType::Opener && second->resourceMutex.try_lock() == true)
			{
				currentDrinker->opener = second;
				second->lockCount++;
				return true;
			}
		}
		else
		{
			if (second->type == ResourceType::Bottle && second->resourceMutex.try_lock() == true)
			{
				currentDrinker->bottle = second;
				second->lockCount++;
				return true;
			}
		}
	}

	firstResource->resourceMutex.unlock(); 

	return false;
}

///////////////////////////////////////////////////////////////////////////////////
// Attempts to perform a drink.
//
// Arguments:
//   currentDrinker - The current drinker
//
// Return:
//   True if the drinker was successfully able to drink.
///////////////////////////////////////////////////////////////////////////////////
bool TryToDrink(Drinker *currentDrinker)
{
	bool wasAbleToDrink = false;
	if (TryToGetResources(currentDrinker) != 0)
	{
		Drink(currentDrinker);
		wasAbleToDrink = true;
	}

	///////////////////////////////////////////////////////////////////////////////////
	//   We're done using at least one resource when we reach this point. Let all
	//   waiting threads know that we've just unlocked at least one resource so they can
	//   try to drink again.
	///////////////////////////////////////////////////////////////////////////////////

	currentDrinker->resourcePool->poolCondition.notify_all();
	return wasAbleToDrink;
}

///////////////////////////////////////////////////////////////////////////////////
// Begins the primary drinking game logic. The specified drinker will constantly
//   attempt to drink until the stopDrinkingFlag has been set. 
//
// Arguments:
//   currentDrinker - The current drinker
///////////////////////////////////////////////////////////////////////////////////
void StartDrinker(Drinker *currentDrinker)
{
	printf("Drinker %d, starting\n", currentDrinker->id);

	while(true)
	{
		////////////////////////////////////////////////////////////////////////////////////
		// Make all access to the stopDrinkingFlag thread safe.
		////////////////////////////////////////////////////////////////////////////////////
		currentDrinker->drinkerPool->startingGunMutex.lock();

		if (currentDrinker->drinkerPool->stopDrinkingFlag)
		{
			break;
		}
		currentDrinker->drinkerPool->startingGunMutex.unlock();
		////////////////////////////////////////////////////////////////////////////////////

		if (!TryToDrink(currentDrinker))
		{
			///////////////////////////////////////////////////////////////////////////////////
			// Make the access to the stopDrinkingFlag thread safe.
			///////////////////////////////////////////////////////////////////////////////////
			currentDrinker->drinkerPool->startingGunMutex.lock();
			if(currentDrinker->drinkerPool->stopDrinkingFlag)
			{
				break;
			}

			else
			{	
			
				///////////////////////////////////////////////////////////////////////////////////
				// Wait for a resource to become available using the resource pool
				///////////////////////////////////////////////////////////////////////////////////
				std::unique_lock<std::mutex> IamWaiting(currentDrinker->resourcePool->poolMutex);
				currentDrinker->drinkerPool->startingGunMutex.unlock();

				currentDrinker->resourcePool->poolCondition.wait(IamWaiting);
				IamWaiting.unlock();

			}
			
		}
	}
	currentDrinker->drinkerPool->startingGunMutex.unlock();
}

///////////////////////////////////////////////////////////////////////////////////
// Entry point for drinker threads.
//
// Arguments:
//   currentDrinker - Pointer to a Drinker struct that is unique to this thread
///////////////////////////////////////////////////////////////////////////////////
void DrinkerThreadEntrypoint(Drinker *currentDrinker)
{
	int totalResources = currentDrinker->resourcePool->totalResources;
	DrinkerPool *drinkerPool = currentDrinker->drinkerPool;

	printf("Drinker %d, is ready to start\n", currentDrinker->id);
	
	////////////////////////////////////////////////////////////////////////////////////////////////
	//   Let main know there's one more drinker thread running then wait for a notification
	//   from main via the DrinkerPool struct.
	////////////////////////////////////////////////////////////////////////////////////////////////

	drinkerPool->drinkerCountMutex.lock();

	(drinkerPool->drinkerCount)++;
	drinkerPool->drinkerCountCondition.notify_one();

	std::unique_lock<std::mutex> locked(drinkerPool->startingGunMutex);
	drinkerPool->drinkerCountMutex.unlock();
	drinkerPool->startingGunCondition.wait(locked, [drinkerPool](){return *drinkerPool->isGunOpen == true; });

	locked.unlock();
	StartDrinker(currentDrinker);

	///////////////////////////////////////////////////////////////////////////////////
	// Let main know there's one less drinker thread running. 
	///////////////////////////////////////////////////////////////////////////////////

	drinkerPool->drinkerCountMutex.lock();

	(drinkerPool->drinkerCount)--;
	drinkerPool->drinkerCountCondition.notify_one();

	drinkerPool->drinkerCountMutex.unlock();

	return;
}

///////////////////////////////////////////////////////////////////////////////////
// Displays the results of all drinkers and resources to the console.
//
// Arguments:
//   poolOfDrinkers - The pool of drinkers
//   resourcePool - The pool of resources.
///////////////////////////////////////////////////////////////////////////////////
void PrintResults(const DrinkerPool &poolOfDrinkers, const ResourcePool &poolOfResources)
{
	int resourceUseCount = 0;
	int resourceLockCount = 0;
	int drinkCount = 0;
	int resourceTryCount = 0;

	printf("*********Drinkers**********\n");
	for (int i = 0; i < poolOfDrinkers.totalDrinkers; i++)
	{
		printf("Drinker %d, Drank %d, %d tries\n", poolOfDrinkers.drinkers[i].id, poolOfDrinkers.drinkers[i].drinkCount, poolOfDrinkers.drinkers[i].resourceTryCount); 
		drinkCount += poolOfDrinkers.drinkers[i].drinkCount;
		resourceTryCount += poolOfDrinkers.drinkers[i].resourceTryCount;
	}
	printf("Total Drinkers %d, Drinks %d, Resource tries %d\n\n\n", poolOfDrinkers.totalDrinkers, drinkCount, resourceTryCount);

	printf("*********Resource Results **********\n");
	for (int i = 0; i < poolOfResources.totalResources; i++)
	{
		printf("Resource %d - type:%s , locked %d, used %d\n", 
			poolOfResources.resources[i].id,
			(poolOfResources.resources[i].type == ResourceType::Bottle) ? "bottle" : "opener",
			poolOfResources.resources[i].lockCount,
			poolOfResources.resources[i].useCount);
		resourceUseCount += poolOfResources.resources[i].useCount;
		resourceLockCount += poolOfResources.resources[i].lockCount;
	}

	printf("Total Resources = %d, %d use count, %d locked count\n\n\n", poolOfResources.totalResources, resourceUseCount, resourceLockCount);
}

int main(int argc, char **argv)
{
	ENABLE_LEAK_DETECTION();

	int resourceCount;
	int bottleCount;
	int openerCount;
	int drinkerCount;
	DrinkerPool poolOfDrinkers;
	ResourcePool poolOfResources;

	//my stuff
	int readyDrinkers = 0;
	bool gunopen = false;

	if (argc != 4)
	{
		fprintf(stderr, "Usage: DrinkingGame drinkerCount bottleCount openerCount\n\n");
		fprintf(stderr, "Arguments:\n");
		fprintf(stderr, "    drinkerCount                 Number of drinkers.                           \n");
		fprintf(stderr, "    bottleCount                  Number of bottles.                            \n");
		fprintf(stderr, "    openerCount                  Number of openers.                            \n");
		Pause();
		return 1;
	}

	drinkerCount = atoi(argv[1]);
	bottleCount = atoi(argv[2]);
	openerCount = atoi(argv[3]);
	resourceCount = bottleCount + openerCount;

	if(drinkerCount < 0 || bottleCount < 0 || openerCount < 0)
	{
		fprintf(stderr, "Error: All arguments must be positive integer values.\n");
		Pause();
		return 1;
	}
	if(resourceCount == 0)
	{
		fprintf(stderr, "Error: Requires at least one resource.\n");
		Pause();
		return 1;
	}

	printf("%s starting %d drinker(s), %d bottle(s), %d opener(s)\n", argv[0], drinkerCount, bottleCount, openerCount);

	// Initialize drinker pool
	poolOfDrinkers.totalDrinkers = drinkerCount;
	poolOfDrinkers.drinkerCount = 0;
	poolOfDrinkers.stopDrinkingFlag = false;
	poolOfDrinkers.drinkers = new Drinker[drinkerCount];

	//my stuff

	poolOfDrinkers.drinkerCount = readyDrinkers;
	poolOfDrinkers.isGunOpen = &gunopen;

	// Initialize resource pool
	poolOfResources.totalResources = resourceCount;
	poolOfResources.resources = new Resource[resourceCount];

	// Initialize individual resources
	for (int i = 0; i < resourceCount; i++)
	{
		poolOfResources.resources[i].id = i;
		poolOfResources.resources[i].type = (i < bottleCount) ? ResourceType::Bottle : ResourceType::Opener;
		poolOfResources.resources[i].lockCount = 0;
		poolOfResources.resources[i].useCount = 0;
	}

	// Initialize individual drinkers
	for (int i = 0; i < drinkerCount; i++)
	{
		poolOfDrinkers.drinkers[i].drinkerPool = &poolOfDrinkers;
		poolOfDrinkers.drinkers[i].resourcePool = &poolOfResources;
		poolOfDrinkers.drinkers[i].id = i;
		poolOfDrinkers.drinkers[i].drinkCount = 0;
		poolOfDrinkers.drinkers[i].resourceTryCount = 0;
		poolOfDrinkers.drinkers[i].bottle = nullptr;
		poolOfDrinkers.drinkers[i].opener = nullptr;		
		poolOfDrinkers.drinkers[i].myRand.Init(0, INT_MAX);

	}

	///////////////////////////////////////////////////////////////////////////////////
	// TO1DO:: Create detached drinker threads
	///////////////////////////////////////////////////////////////////////////////////

	for (int i = 0; i < drinkerCount; i++)
		std::thread(DrinkerThreadEntrypoint, &poolOfDrinkers.drinkers[i]).detach();
	///////////////////////////////////////////////////////////////////////////////////
	//   Wait for all drinkers to be ready. Wait for changes in the pool of drinkers 
	//   to avoid burning CPU cycles.
	///////////////////////////////////////////////////////////////////////////////////

	std::unique_lock<std::mutex> locked(poolOfDrinkers.drinkerCountMutex);
	poolOfDrinkers.drinkerCountCondition.wait(locked, [&](){return poolOfDrinkers.drinkerCount == drinkerCount; });

	locked.unlock();
	printf("Main: Firing gun\n");

	///////////////////////////////////////////////////////////////////////////////////
	// Start all of the drinkers. All of the drinkers must be ready by this point. 
	///////////////////////////////////////////////////////////////////////////////////

	poolOfDrinkers.startingGunMutex.lock();
	gunopen = true;
	poolOfDrinkers.startingGunCondition.notify_all();
	poolOfDrinkers.startingGunMutex.unlock();

	// Wait for user input before telling the drinkers to stop
	Pause();

	///////////////////////////////////////////////////////////////////////////////////
	// Set the stopDrinkingFlag so the drinkers break out of their drinking loop.
	///////////////////////////////////////////////////////////////////////////////////

	poolOfDrinkers.startingGunMutex.lock();
	poolOfDrinkers.stopDrinkingFlag = true;
	poolOfDrinkers.startingGunCondition.notify_all();
	poolOfDrinkers.startingGunMutex.unlock();

	///////////////////////////////////////////////////////////////////////////////////
	//   Wakeup anyone waiting on a resource.
	///////////////////////////////////////////////////////////////////////////////////
	poolOfResources.poolMutex.lock();
	poolOfResources.poolCondition.notify_all();
	poolOfResources.poolMutex.unlock();
	
	///////////////////////////////////////////////////////////////////////////////////
	// Wait for all drinkers to finish.
	///////////////////////////////////////////////////////////////////////////////////
	locked.lock();
	poolOfDrinkers.drinkerCountCondition.wait(locked, [&](){return poolOfDrinkers.drinkerCount == 0; });
	locked.unlock();
	PrintResults(poolOfDrinkers, poolOfResources);

	///////////////////////////////////////////////////////////////////////////////////
	// Clean up.
	///////////////////////////////////////////////////////////////////////////////////

	delete[] poolOfDrinkers.drinkers;
	delete[] poolOfResources.resources;

	Pause();
	return 0;
}
