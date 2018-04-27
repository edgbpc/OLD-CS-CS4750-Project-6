#include "oss.h"



unsigned int * simClock;
Process * process;



int main (int argc, char *argv[]){
	printf("USER %d has launched\n", getpid());
	
	time_t timeSeed;
	srand((int)time(&timeSeed) % getpid()); //%getpid used because children were all getting the same "random" time to run. 
	int myPID = getpid();
	int location = atoi(argv[1]);

	int terminationChance; //change this to increase or decrease chance process will terminate.  currently 50% to terminate
	int readFromMemoryChance; //holds precent chance memory reference will be a read.
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

	int numPages = (rand() % 30) + 1; //set number of pages this process has from 0 to 31	
	process[location].pageTable.delimiter = numPages;
	printf("Process %d has %d pages\n", getpid(), process[location].pageTable.delimiter);
	
	//set pageTable locations to empty
	for (index = 0; index < 31; index++){
		process[location].pageTable.pageFrame[index] = empty;
	}

		//verification 
		//for (index = 0; index < 256; index++){
		//	printf("%d\n", process[location].pageTable.frame[index]);
		//}
	
	//set message for values that remain same through lifetime of process

		message.pid = myPID;
		message.location = location;
		
	while (1){
		//roll dice
	//	printf("process %d is rolling the dice\n", myPID);
		readFromMemoryChance = (rand() % 100) + 1; //sets precent chance memory reference will be a read.
		simulatedInvalidPageReferenceChance = (rand() % 100) + 1; //sets percent chance to cause a segmentation fault
		terminationChance = (rand() % 100) + 1; 
		
		if (terminationChance > 90){
			printf("Process %d has terminated\n", getpid());
			message.terminate = true;
			message.mesg_type = getppid();
			
			if(msgsnd(messageBoxID, &message, sizeof(message), 1) ==  -1){
				perror("oss: failed to send message to user");
			}
			break;
		}
	

	

		//determine if there is going to be a seg fault
		if (simulatedInvalidPageReferenceChance > 99){ 
			pageReference = numPages + 1;  //simulated seg fault.  change value in live above to increase odds.  currently 1%
		} else {
			pageReference = (rand() % numPages);
		}


		
		//determine precent chance of read or a write.  higher value, the greater the chance of a read
		if (readFromMemoryChance > 50){
			referenceType = read; //defined as 1
		} else {
			referenceType = write; //defined as 0
		}
	
		//build message for parent
		message.pageReference = pageReference;
		message.referenceType = referenceType;
		message.mesg_type = getppid();
	
		//check used in development	
	//	printf("Process: %d - %ld %d %d %d\n", getpid(), message.mesg_type, message.pageReference, message.pid, message.referenceType);
	
		//send a message to parent
		//printf("%d\n", message.mesg_type);
		if(msgsnd(messageBoxID, &message, sizeof(message), 1) ==  -1){
			perror("oss: failed to send message to user");
		}

		//wait for a message from parent
		//printf("%d waiting on message from parent\n", myPID);
		msgrcv(messageBoxID, &message, sizeof(message), myPID, 0); //retrieve message from box.  child is blocked unless there is a message its PID available
	//	printf("%d received message from parent\n", myPID);
			
	//	printf("Process %d made a reference to %d page and type is %d\n",myPID, pageReference, referenceType);
	

	} //end while loop
} //end main



void handle(int signo){
	if(signo == SIGINT){
		fprintf(stderr, "User process %d shut down by signal\n", getpid());
		exit(0);
	}
}



	

