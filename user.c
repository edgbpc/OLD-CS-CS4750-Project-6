#include "oss.h"

unsigned int * simClock;
Process * process;


int main (int argc, char *argv[]){


	printf("USER %d has launched\n", getpid());
	
	int myPID = getpid();
	int location = atoi(argv[1]);

	int randomTerminateConstant = 90; //change this to increase or decrease chance process will terminate.  currently 50% to terminate
	int randomRequestConstant = 50;
	//seed random number generator
	time_t timeSeed;
	srand((int)time(&timeSeed) % getpid()); //%getpid used because children were all getting the same "random" time to run. 

	//set number of pages this process has from 0 to 31	
	int numPages = (rand() % 30) + 1;

	int index; //loop counter variable

	//signal handling
	if (signal(SIGINT, handle) == SIG_ERR){
		perror("signal failed");
	}
	//Shared Memory Keys
	keySimClock = 59566;
	keyProcess = 59567;
	//Get Shared Memory

	if ((shmidSimClock = shmget(keySimClock, (2*(sizeof(unsigned int))), 0666)) == -1){
		perror("user: could not get shmidSimClock\n");
		return 1;
	}

	if ((shmidProcess = shmget(keyProcess, (18*(sizeof(Process))), 0666)) == -1){
		perror("user: could not get shmidProcess\n");
		return 1;
	}
	
	//get the shared Clock and resourceTable
	simClock = (int *)(shmat(shmidSimClock, 0, 0));
	process = (Process *)(shmat(shmidProcess, 0, 0));
	
	//message queue key
	key_t messageQueueKey = 59569;
	//get message box
	if ((messageBoxID = msgget(messageQueueKey, 0666)) == -1){
		perror("user: failed to acceess parent message box");
	}

	//save location in the resource table to the process structure
	//copy maxCLaims to the process structure

	//assign numPages to delimiter	
	process[location].pageTable.delimiter = numPages;
	printf("Process %d has %d pages\n", getpid(), process[location].pageTable.delimiter);

	for (index = 0; index < numPages; index++){
		process[location].page[index] = (rand() % 50); //just want some kind of data in the page
	}
	
		//verification to see if data was generated and assigned to the page	
		//for (index = 0; index < numPages; index++){
		//	printf("process %d has %d in page %d\n", getpid(), process[location].page[index], index ); //just want some kind of data in the page
		//}

sleep(5);

} //end main
/*
int findResource(int allocated[][maxResources], int location){
	int index;
	for (index = 0; index < maxProcesses; index++){
		if (allocated[location][index] != 0){
			return index;
			break;
		} else {
			return -1;
		}
	}	
}
*/

void handle(int signo){
	if(signo == SIGINT){
		fprintf(stderr, "User process %d shut down by signal\n", getpid());
		exit(0);
	}
}



	

