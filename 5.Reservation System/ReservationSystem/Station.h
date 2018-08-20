///////////////////////////////////////////////////////////////////////////////////
// file:  Station.h
// Job:   holds the station class 
//////////////////////////////////////////////////////////////////////////////////

// mult protection
#ifndef _STATION_
#define _STATION_

// include needed files
#include <thread>
#include <mutex>
#include <condition_variable>

// forward declarations
class Pump;

// class Station
class Station
{
private:
	//Variables
	unsigned int freeMask;						// used for seeing if a pump is in use or not 
	Pump *pumps;								// an array of pumps 
	int pumpsInStation;							// number of pumps in the station
	int carsInStation;							// number of cars that will visit the station
	std::mutex* stationMutex;					// mutex for protecting memory in the station
	std::condition_variable* stationCondition;	// cv used for scheduling in the station

public:
	//constructor and destructor
	Station(void);
	~Station(void);

	//accessors  
	int getPumpFillCount(int num);
	int getCarsInStation(void);
	std::mutex* getstationMutex(void);
	std::condition_variable* getStationCondition(void);

	// mutators
	void setCarsInStation(int num);
	void setStationMutex(std::mutex* m);
	void setStationCondition(std::condition_variable* cv);	
	
	///////////////////////////////////////////////////////////////////////
	// Name:		fillUp
	//
	// Arguments:	none
	//
	// Notes:		This function will be the reservation system.  It will 
	//				fill up the gas tanks of cars and control thier access 
	//
	// Returns:		int - (1) if the fill up was successfull and (-1) if it failed
	//////////////////////////////////////////////////////////////////////
	int fillUp();
	
	///////////////////////////////////////////////////////////////////////
	// Name:		createPumps
	//
	// Arguments:	numOfPumps - number of pumps for that station
	//
	// Notes:		This function will allocate memory for the pumps in the station 
	//
	// Returns:		void
	//////////////////////////////////////////////////////////////////////
	void createPumps(int numOfPumps);
};

#endif