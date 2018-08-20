///////////////////////////////////////////////////////////////////////////////////
// file:  Car.h
// Job:   holds the Car class 
//////////////////////////////////////////////////////////////////////////////////

// mult protection
#ifndef _CAR_
#define _CAR_

// include needed files
#include <thread>
#include <mutex>
#include <condition_variable>

// forward declarations
class Station;

// class Car
class Car
{
private:
	//Variables
	int fillCount;							// count of the fills
	int tryCount;							// count of the tries to fill
	int* numberWaitingInLine;				// number of cars on the starting line
	bool* testOver;							// test over boolean
	std::thread *thread;					// thread for the car
	Station *stationToUse;					// station used for this car
	std::mutex* countMutex;					// mutex for protecting the counts
	std::mutex* gunMutex;					// mutex for protecting the gun
	std::condition_variable* gunCondition;	// cv used for scheduling for the gun
	std::condition_variable* countCondition;// cv used for scheduling for the count

public:
	//constructor and destructor
	Car(void);
	~Car(void);	
	
	// accessors  
	int getFillCount(void);
	int getTryCount(void);
	Station* getStationToUse(void);
	std::mutex* getCountMutex(void);
	std::condition_variable* getCountCondition(void);
	std::mutex* getGunMutex(void);
	std::condition_variable* getGunCondition(void);
	bool* getTestOver(void);

	// mutators
	void setStationToUse(Station *myStation);
	void setCountMutex(std::mutex* m);
	void setCountCondition(std::condition_variable* cv);
	void setGunMutex(std::mutex* m);
	void setGunCondition(std::condition_variable* cv);
	void setNumberStandingInLine(int* num);
	void incNumberWaitingInLine();
	void decNumberWaitingInLine();
	void setTestOver(bool* b);


	///////////////////////////////////////////////////////////////////////
	// Name:		startCar
	//
	// Arguments:	function pointer - function that will be the thread
	//
	// Notes:		This function will start the thread for the car
	//
	// Returns:		void
	//////////////////////////////////////////////////////////////////////
	void startCar(void testACar(Car* car));
	
	///////////////////////////////////////////////////////////////////////
	// Name:		waitForCarToStop
	//
	// Arguments:	void
	//
	// Notes:		This function will do the join thread functionality
	//				and delete the thread memory
	//
	// Returns:		void
	//////////////////////////////////////////////////////////////////////
	void waitForCarToStop(void);
	
	///////////////////////////////////////////////////////////////////////
	// Name:		fillTank
	//
	// Arguments:	void
	//
	// Notes:		This function will inc the try count, fill count and 
	//				call the stations fillup function
	//
	// Returns:		int - (1) if the fill up was successful and (-1) if it failed
	//////////////////////////////////////////////////////////////////////
	int fillTank(void);	
};

#endif