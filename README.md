Multithreading
==============

## 1. Race Condition

![Race](https://github.com/Cabrra/cabrra.github.io/blob/master/Images/multithread/race.png)

Situations arise where several processes or threads access and manipulate the same data concurrently. The outcome of the execution depends on the particular order in which the accesses take place.  This is called a race condition.

+ Run types
	+ 0 (zero): all threads run simultaneously; not synchronized.
	+ 1 (one): only one thread modifies the string but all threads are synchronized.
	+ 2 (two): all threads are synchronized but no other thread begins working until the current thread has completed all of its work.
	+ 3 (three): same as run type 2, but threads must execute their work in order of thread ID.

+ Arguments
	+ threadCount                  Number of threads to create.
	+ sharedStringLength           Length of string to generate.
	+ numberOfStringsToGenerate    Number of strings to generate per thread.
	+ waitTime                     Time to wait before generating the string.
	
## 2. Thread Types

Threads have two different modes of execution: joinable and detached. this small project runs multiple threads of both types at the same time, perform some work, print data in a separate thread and get the threads to synchronize and communicate with main in a thread safe manner.

+ Arguments:
	+ threadCount                  Number of threads to create for each type.

## 3. Tic Tac Toe

![TicTac](https://github.com/Cabrra/cabrra.github.io/blob/master/Images/multithread/TicTacToe.png)

This plays multiple games of Tic-Tac-Toe using multiple threads controlling the flow of each game using condition variables.

+ Arguments
	+ gameCount                    Number of games.
	+ playerCount                  Number of players.
	
## 4. Drinking Game

When dealing with threads and the need to access multiple mutex-protected resources, there is the possibility for “deadlock” to occur. Deadlock (sometimes called “deadly embrace”) is a situation where two or more competing actions are waiting for the other to finish, and thus neither ever does. 

+ Arguments:
	+ drinkerCount                 Number of drinkers.
	+ bottleCount                  Number of bottles.
	+ openerCount                  Number of openers.

## 5. Reservation

![Gas Station](https://github.com/Cabrra/cabrra.github.io/blob/master/Images/multithread/ReservationSystem.png)

Simulation of a gas station with a fixed number of cars trying to full up at the station's two gas pumps.

The time it takes to fill up at a pump is 30ms, and since the station has two pumps, each pump can be used simultaneously.  There is no order for trying to fill up so the distribution will be on a first come first served basis. 

## Built With

* [Visual Studio](https://visualstudio.microsoft.com/) 					- For C++ development

## Contributing

Please read [CONTRIBUTING.md](https://github.com/Cabrra/Contributing-template/blob/master/Contributing-template.md) for details on the code of conduct, and the process for submitting pull requests to me.

## Authors

* **Jagoba "Jake" Marcos** - Particle Effect Engineer, Particle Tool, Gluttony, Ambush Ninjas AI, Puzzle Logic, Stealth System - [Cabrra](https://github.com/Cabrra)

## License

This project is licensed under the MIT license - see the [LICENSE](LICENSE) file for details

## Acknowledgments

* Full Sail University - Game Development Department
* Charles Graves - Course director
