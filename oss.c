#include "oss.h" //contains all other includes necessary, macro definations, and function prototypes




//Global Variables

char fileName[10] = "data.log";
FILE * fp;
pid_t childPid; //pid id for child processes
unsigned int *simClock; // simulated system clock  simClock [0] = seconds, simClock[1] = nanoseconds
MemoryBlock memoryBlock[256]; //simulates memoryBlock
Process *process; //holds information regarding individual processes


int FindIndex(int value);
int bitVector[maxProcesses];
int pidArray[maxProcesses];

//prototypes
static int setperiodic(double sec);
void terminateSharedResources();
void convertTime(unsigned int simClock[]);
void printMemoryMap();
void print_usage();
void calculateStatistics();

//record keeping variables
float numMemAccessPerSec;
float numMemAccessPerNanoSec;
float numPageFaultsPerMemAccess;
int averageMemAccessSpeed;
int numSegFaults;
int throughput;
int totalProcessesCreated; //keeps track of all processes created	
int numMemoryAccess;
int numEvictions;
int line;
int numPageFaults;

int main (int argc, char *argv[]) {
	//keys
	messageQueueKey = 59569;
	keySimClock = 59566;
	keyProcess = 59567;

	int maxProcess = 18; //default
	int processLimit = 100; //max number of processes allowed by assignment parameters
	double terminateTime = 3; //used by setperiodic to terminate program
	unsigned int newProcessTime[2] = {0,0};
	bool messageReceived;
	bool pagePresent;
	int index; //loop counting variable
	bool frameAvailable;
	bool noEmptyFrames;
	int status;
	Queue* FIFOQueue = createQueue(256);

//open file for writing	
	fp = fopen(fileName, "w");
//seed random number generator
	srand(time(NULL));
//signal handler to catch ctrl c
	if(signal(SIGINT, handle) == SIG_ERR){
		perror("Signal Failed");
	}
	if(signal(SIGALRM, handle) == SIG_ERR){
		perror("Signal Failed");
	}	
//set timer. from book
	if (setperiodic(terminateTime) == -1){
		perror("oss: failed to set run timer");
		return 1;
	}
//Create Shared Memory
	if ((shmidSimClock = shmget(keySimClock, (2*(sizeof(unsigned int))), IPC_CREAT | 0666)) == -1){
		perror("oss: could not create shared memory for simClock");
		return 1;
	}

	if ((shmidProcess = shmget(keyProcess, (18*(sizeof(Process))), IPC_CREAT | 0666)) == -1){
		perror("oss: could not create shared memory for process");
		return 1;
	}


//Attach to shared memory
	simClock = shmat(shmidSimClock, NULL, 0);
	simClock[0] = 0; //nanoseconds
	simClock[1] = 0; //seconds
	process = shmat(shmidProcess, NULL, 0);
	
//create mail box
	if ((messageBoxID = msgget(messageQueueKey, IPC_CREAT | 0666)) == -1){
		perror("oss: Could not create child mail box");
		return 1;
	}

	for (index = 0; index < 256; index++){
		memoryBlock[index].frame = empty;
	}



	char option;
	while ((option = getopt(argc, argv, "s:h")) != -1){
		switch (option){
			case 's' : 
				maxProcess = atoi(optarg);
				if (maxProcess > 18){
					printf("max allowed processes is 18.  Setting to max");
					maxProcess = 18;
				}
				break;
			case 'h': print_usage();
				exit(0);
				break;
			default : print_usage(); 
				exit(EXIT_FAILURE);
		}
	}

printf("Max processes selected was %d\n", maxProcess);

	while(1){ //setting max loops during development
		//check to see if its time for a new process to start
		

		printf("starting\n");
		if(line >= 100000){
			terminateSharedResources;
			printf("Terminated due to data.log exceeding 100,000 lines\n");
			kill(getpid(), SIGINT);
		}	
		if ((simClock[1] == newProcessTime[1] && simClock[0] >= newProcessTime[0]) || simClock[1] >= newProcessTime[1]){
			bool spawnNewProcess = false;
			//find available location in the resouceTable
			for (index = 0; index < maxProcess; index++){
				if (pidArray[index] == 0){
					spawnNewProcess = true;
					printf("forking child\n");
					childPid = fork();
					break;
				}
			}		
			if (spawnNewProcess == true){
				if(childPid == -1){
					perror("OSS:  Failed to fork child\n");
					kill(getpid(), SIGINT);
				}
				if(childPid == 0){
					//convert index to string to pass to User so it knows its location in the resouceTable
					char intBuffer[3];
					sprintf(intBuffer, "%d", index);
					printf("%s", intBuffer);
					execl("./user","user", intBuffer, NULL);
				} 
	
				pidArray[index] = childPid;
				fprintf(fp, "Process %d created at system %d.%d.  Saved to location %d\n", childPid, simClock[1], simClock[0], index);
				fflush(fp);
				line++;
				totalProcessesCreated++;
			}

			newProcessTime[0] = (rand() % 500000000) + 1000000 + simClock[0];
			newProcessTime[1] = simClock[1];
			convertTime(simClock);
	
		}

 		//wait for a message from a child
		msgrcv(messageBoxID, &message, sizeof(message), getpid(), 1);

		numMemoryAccess++;

		noEmptyFrames = false;
		pagePresent = false;
	
		printf("message received\n");
		fprintf(fp, "Process  %d made reference to page %d.  its delimiter is %d\n", message.pid, message.pageReference, process[message.location].pageTable.delimiter);
		fflush(fp);
		line++;

	
		//check for segmentation fault
		if (message.pageReference > process[message.location].pageTable.delimiter){
			
			//if seg fault
			fprintf(fp, "Process %d made a request that resulted in a segmentation fault.  Terminated by OSS at time %d.%d\n", message.pid, simClock[1], simClock[0]);
			fflush(fp);
			line++;
			printf("Process %d - SEGMENTATION FAULT\n", message.pid);
			pidArray[message.location] = 0;
			kill(message.pid, SIGTERM);
			wait(NULL);
			numSegFaults++;
			continue;
		}


		//check for page fault
		printf("Process %d has made a reference to page %d at time %d.%d\n", message.pid, message.pageReference, simClock[1], simClock[0]);
		//check pocess pageTable if page exists in memory
		if(process[message.location].pageTable.pageFrame[message.pageReference] != -1){
			pagePresent = true;
			//set reference bit to 1 since existing page was referenced
			fprintf(fp, "Process %d made reference to page already in memory.  Its type is %d\n", message.pid, message.referenceType);
			fflush(fp);
			line++;
			memoryBlock[process[message.location].pageTable.pageFrame[message.pageReference]].referenceBit = 1; 
			//set dirtyBit if type was write
			if(message.referenceType == write){
				fprintf(fp, "Process %d made a write request to page %d\n at time %d.%d", message.pid, message.pageReference, simClock[1], simClock[0]);
				fflush(fp);
				line++;
				memoryBlock[process[message.location].pageTable.pageFrame[message.pageReference]].dirtyBit = 1; 
			}			
			if(message.referenceType == read){
				fprintf(fp, "Process %d made a read request to page %d\n at time %d.%d", message.pid, message.pageReference, simClock[1], simClock[0]);
				fflush(fp);
				line++;
				memoryBlock[process[message.location].pageTable.pageFrame[message.pageReference]].dirtyBit = 0; 
			}
				 
			printMemoryMap();
			//increment clock
			simClock[0] += 10;
	
		}		
		

		if (!pagePresent){
			numPageFaults++;
		//check if room in the memoryBlock for page
			for (index = 0; index < 256; index++){
//			printf("check memoryBlock %d\n", index);
				if (memoryBlock[index].frame == empty){
					//assign page to memoryBlock frame
					printf("memoryBlock location %d is empty\n", index);

					memoryBlock[index].frame = used; 
					memoryBlock[index].pid = message.pid;
					memoryBlock[index].referenceBit = 0;
					memoryBlock[index].dirtyBit = 0;
					enqueue(FIFOQueue, index); //keep track of order pages were added
					printf("%d was saved to FIFOQueue\n", index);
					process[message.location].pageTable.pageFrame[message.pageReference] = index;
					fprintf(fp, "memoryBlock location %d added to FIFOQueue\n", index);
					fprintf(fp, "Process %d page %d saved to memoryBlock frame %d at time %d.%d\n", message.pid, message.pageReference, index, simClock[1], simClock[0]);
					fflush(fp);
					line++;
					line++;
					//simulate read/write time 
					//increment clock 15ms
					simClock[0] += 15000000;
					convertTime(simClock);
					break;
				} else if (index == 255){
					noEmptyFrames = true;
				}	
			}
			printMemoryMap();		
			if (noEmptyFrames){
				//find a page to discard
				int victim; //which memory location to evict 
				//check memoryBlock reference bits
				int FIFOIndex = 0;
				fprintf(fp, "Searching for a page to evict\n");
				fflush(fp);
				bool searching = true;
			
	
				while(searching){
					int candidate = front(FIFOQueue);
					if (isEmpty(FIFOQueue)){
						break;
						} else {
						dequeue(FIFOQueue);
					}
	
					if(memoryBlock[candidate].referenceBit == 0 && memoryBlock[candidate].dirtyBit == 0){
							victim = candidate;
							fprintf(fp, "Selected %d for eviction\n", victim);
							fflush(fp);	
							searching = false;
							break;
						} else {
							if (memoryBlock[candidate].dirtyBit == 1){
								memoryBlock[candidate].dirtyBit = 0;
								//simulate disk writing time
								//this location eligible for eviction on next check if not written to again
								simClock[0] += 15000000;
								convertTime(simClock);
							}
							if (memoryBlock[candidate].referenceBit == 1){
								memoryBlock[candidate].referenceBit = 0;			
							}
						enqueue(FIFOQueue, candidate);
					}
					
				}
				fprintf(fp, "Page in Memory Block %d being evicted at time %d.%d\n", victim, simClock[1], simClock[0]);
				fflush(fp);
				line++;
				numEvictions++;
				//change sentry for evicted process to -1
//				int victimIndex = findPid(memoryBlock[victim].pid);
				process[message.location].pageTable.pageFrame[victim] = -1;
				//increment clock 15ms
				simClock[0] += 15000000;
				convertTime(simClock);
				memoryBlock[victim].frame = used;
				memoryBlock[victim].pid = message.pid;
				memoryBlock[victim].referenceBit = 0;
				memoryBlock[victim].dirtyBit = 0;
				process[message.location].pageTable.pageFrame[message.pageReference] = victim;
				fprintf(fp, "Process %d page %d saved to memoryBLock frame %d at time %d.%d\n", message.pid, message.pageReference, victim, simClock[1], simClock[0]);
				fflush(fp);
				line++;
				printMemoryMap();
			}
		}
		
	
		message.mesg_type = message.pid;	   
		printf("sending message to child %d\n", message.pid);

		if(msgsnd(messageBoxID, &message, sizeof(message), 0) ==  -1){
			perror("oss: ");
		}
	} //end while loop
wait(NULL);

} //end main()
//convertTime
void convertTime(unsigned int simClock[]){
	simClock[1] += simClock[0]/1000000000;
	simClock[0] = simClock[0]%1000000000;
}

void calculateStatistics(){
	float numSecsRan = simClock[1] * 1000000000;
	numSecsRan += simClock[0];
	numMemAccessPerSec = (numSecsRan/(float)numMemoryAccess)/1000000000;
	numPageFaultsPerMemAccess = (float)numPageFaults/(float)numMemoryAccess;

}

void printReport(){
	calculateStatistics();	
	printf("Total Processes Created: %d\n", totalProcessesCreated);
	printf("Number of Segmentation Faults: %d\n", numSegFaults);
	printf("Number of Memory Access Attempts: %d\n", numMemoryAccess);
	printf("Number of Evictions: %d\n", numEvictions);
	printf("Number of Page Faults: %d\n", numPageFaults);
	printf("Number of Memory Accesses Per Second: %f\n", numMemAccessPerSec);
	printf("Number of Page Faults per Memory Access: %f\n", numPageFaultsPerMemAccess);
	printf("Lines on data.log: %d\n", line);
}



//TAKEN FROM BOOK
static int setperiodic(double sec) {
   timer_t timerid;
   struct itimerspec value;
   if (timer_create(CLOCK_REALTIME, NULL, &timerid) == -1)
      return -1;
   value.it_interval.tv_sec = (long)sec;
   value.it_interval.tv_nsec = (sec - value.it_interval.tv_sec)*BILLION;
   if (value.it_interval.tv_nsec >= BILLION) {
      value.it_interval.tv_sec++;
      value.it_interval.tv_nsec -= BILLION;
   }
   value.it_value = value.it_interval;
   return timer_settime(timerid, 0, &value, NULL);
}

void printMemoryMap(){
	int i;
	for (i = 0; i < 256; i++){
		if (memoryBlock[i].referenceBit == 1){
			fprintf(fp, "1");
		} else if (memoryBlock[i].referenceBit == 0){
			fprintf(fp, "0");
		} else {
			fprintf(fp, ".");
		}
	}
	fprintf(fp, "\n");
	fflush(fp);
	line++;
	for (i = 0; i < 256; i++){
		if (memoryBlock[i].dirtyBit == 1){
			fprintf(fp, "D");
		} else if (memoryBlock[i].dirtyBit == 0) {
			fprintf(fp, "U");
		} else {
			fprintf(fp, " .");
		}
		fflush(fp);
	}
	fprintf(fp, "\n");
	fflush(fp);
	line++;
}
int findPid(int pid){
	int i;
	for (i = 0; i < maxProcesses; i++){
		if (pidArray[i] == pid){
			return i;
		}
		
	}
}
		

void handle(int signo){
	if (signo == SIGINT || signo == SIGALRM){
		printf("*********Signal was received************\n");
		terminateSharedResources();
		kill(0, SIGKILL);
		wait(NULL);
		exit(0);
	}
}



void terminateSharedResources(){
		printReport();
		shmctl(shmidSimClock, IPC_RMID, NULL);
		shmctl(shmidProcess, IPC_RMID, NULL);
		msgctl(messageBoxID, IPC_RMID, NULL);
		shmdt(simClock);	
		shmdt(process);
}
//Queue code comes from https://www.geeksforgeeks.org/queue-set-1introduction-and-array-implementation

Queue* createQueue(unsigned capacity)
{
    Queue* queue = (Queue*) malloc(sizeof(Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0; 
    queue->rear = capacity - 1;  // This is important, see the enqueue
    queue->array = (int*) malloc(queue->capacity * sizeof(int));
    return queue;
}
 
int isFull(Queue* queue){
	  return (queue->size == queue->capacity); 
 }
 
int isEmpty(Queue* queue){
	return (queue->size == 0);
 }
  
void enqueue(Queue* queue, int item){
       if (isFull(queue))
		  return;
       queue->rear = (queue->rear + 1)%queue->capacity;
       queue->array[queue->rear] = item;
       queue->size = queue->size + 1;
       printf("%d enqueued to queue\n",item);
}
                                
int dequeue(Queue* queue){
	if (isEmpty(queue))
		return INT_MIN;
        int item = queue->array[queue->front];
        queue->front = (queue->front + 1)%queue->capacity;
        queue->size = queue->size - 1;
        return item;
}
                                                             
int front(Queue* queue){
	if (isEmpty(queue))
	        return INT_MIN;	
	return queue->array[queue->front];
}
int rear(Queue* queue){
        if (isEmpty(queue))
                return INT_MIN;
        return queue->array[queue->rear];
}

//this code came from https://stackoverflow.com/questions/25003961/find-array-index-if-given-value
int FindIndex(int value){
	int i = 0;

	while (i < maxProcesses && bitVector[i] != value) ++i;
	return (i == maxProcesses ? -1 : i );
	}

/*
 KEPT FOR FUTURE USE
//getopt
	char option;
	while ((option = getopt(argc, argv, "s:hl:t:")) != -1){
		switch (option){
			case 's' : maxProcess = atoi(optarg);
				break;
			case 'h': print_usage();
				exit(0);
				break;
			case 'l': 
				strcpy(fileName, optarg);
				break;
			case 't':
				terminateTime = atof(optarg);
				break;
			default : print_usage(); 
				exit(EXIT_FAILURE);
		}
	}
*/
void print_usage(){
	printf("Execution syntax oss -options value\n");
	printf("Options:\n");
	printf("-h: this help message\n");
	printf("-s: specify max limit of processes that can be ran.  Default: 18\n");
}

