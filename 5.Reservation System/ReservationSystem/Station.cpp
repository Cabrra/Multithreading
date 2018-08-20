///////////////////////////////////////////////////////////////////////////////////
// file:  Station.cpp
// Job:   holds the Station definitions 
//////////////////////////////////////////////////////////////////////////////////

#include "Station.h"
#include "Pump.h"

using namespace std;

Station::Station(void) : freeMask(0)
{
	pumps = nullptr;
	pumpsInStation = 0;
}

Station::~Station(void)
{
	delete []pumps;
}

int Station::fillUp()
{
	/////////////////////////////////////////////////////////////////////////////////////////////
	//   Find a free pump and fill up using that pump, otherwise wait until a pump becomes
	//   available.
	/////////////////////////////////////////////////////////////////////////////////////////////

	bool success = false;

		for (int i = 0; i < this->pumpsInStation; i++)
		{
			this->stationMutex->lock();
			if ((this->freeMask & (1 << i)) == 0)
			{
				this->freeMask |= (1 << i);
				this->stationMutex->unlock();
				success = true;
				this->pumps[i].fillTankUp();

				this->stationMutex->lock();
				this->freeMask &= ~(1 << i);
				
				this->stationMutex->unlock();
				this->stationCondition->notify_all();
				break;
			}
			this->stationMutex->unlock();
		}
		

		//if fails wait 
		if (success == false)
		{
			std::unique_lock<std::mutex> IamWaiting(*this->stationMutex);

			this->stationCondition->wait(IamWaiting);
			//IamWaiting.unlock();
		}
		else
		{
			this_thread::sleep_for(chrono::milliseconds(24 * (this->carsInStation /this->pumpsInStation)));
			return 1;
		}

	return 0;
}

int Station::getPumpFillCount(int num)
{
	if((num >= 0) && (num < pumpsInStation))
	{
		return pumps[num].getFillCount();
	}
	else 
	{
		return -1;
	}
}

void Station::createPumps(int numOfPumps)
{
	pumps = new Pump[numOfPumps];
	pumpsInStation = numOfPumps;
}

int Station::getCarsInStation(void)
{
	return this->carsInStation;
}

void Station::setCarsInStation(int num)
{
	this->carsInStation = num;
}

std::mutex* Station::getstationMutex(void)
{
	return this->stationMutex;
}

std::condition_variable* Station::getStationCondition(void)
{
	return this->stationCondition;
}

void Station::setStationMutex(std::mutex* m)
{
	this->stationMutex = m;
}

void Station::setStationCondition(std::condition_variable* cv)
{
	this->stationCondition = cv;
}
