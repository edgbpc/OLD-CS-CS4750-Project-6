#include "oss.h"



unsigned int * simClock;
Process * process;



int main (int argc, char *argv[]){
	printf("USER %d has launched\n", getpid());
	
	time_t timeSeed;
	srand((int)time(&timeSeed) % getpid()); //%getpid used because children were all getting the same "random" time to run. 
	int myPID = getpid();
	int location = atoi(argv[1]);

	int randomTerminateConstant = 90; //change this to increase or decrease chance process will terminate.  currently 50% to terminate
	int readFromMemoryChance; //holds precent chance memory reference will be a read.
	int numPages = (rand() % 30) + 1; //set number of pages this process has from 0 to 31	
	int index; //loop counter variable
	int pageReference; //holds what page is being reference
	int simulatedInvalidPageReferenceChance; //sets percent chance to cause a segmentation fault
	int referenceType; //holds if reference type will be read or write

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
		//
	
	//set message for values that remain same through lifetime of process

	message.mesg_type = getppid();
	message.pid = myPID;
	while (1){
		//roll dice
		readFromMemoryChance = (rand() % 100) + 1; //sets precent chance memory reference will be a read.
		numPages = (rand() % 30) + 1; //set number of pages this process has from 0 to 31	
		simulatedInvalidPageReferenceChance = (rand() % 100) + 1; //sets percent chance to cause a segmentation fault
	

		//determine if there is going to be a seg fault
		if (simulatedInvalidPageReferenceChance > 99){ 
			pageReference = numPages + 1;  //simulated seg fault.  change value in live above to increase odds.  currently 1%
			printf("Process %d - Seg fault\n", myPID);
		} else {
			pageReference = (rand() % numPages);
		}

		printf("%d\n", readFromMemoryChance);

		
		//determine precent chance of read or a write.  higher value, the greater the chance of a read
		if (readFromMemoryChance > 50){
			referenceType = read; //defined as 1
		} else {
			referenceType = write; //defined as 0
		}
	
		//build message for parent
		message.pageReference = pageReference;
		message.referenceType = referenceType;
	
		//check used in development	
	//	printf("Process: %d - %ld %d %d %d\n", getpid(), message.mesg_type, message.pageReference, message.pid, message.referenceType);
	
		//send a message to parent
		if(msgsnd(messageBoxID, &message, sizeof(message), 0) ==  -1){
			perror("oss: failed to send message to user");
		}

		//wait for a message from parent
		msgrcv(messageBoxID, &message, sizeof(message), myPID, 0); //retrieve message from box.  child is blocked unless there is a message its PID available
	
	//	printf("Process %d made a reference to %d page and type is %d\n",myPID, pageReference, referenceType);
	

sleep(5);
	} //end while loop
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



	

