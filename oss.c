#include "oss.h" //contains all other includes necessary, macro definations, and function prototypes




//Global Variables

char fileName[10] = "data.log";
FILE * fp;
pid_t childPid; //pid id for child processes
unsigned int *simClock; // simulated system clock  simClock [0] = seconds, simClock[1] = nanoseconds
int memoryBlock[256];
Process *process;


int FindIndex(int value);
int bitVector[18];
int pidArray[maxProcesses];

//prototypes
static int setperiodic(double sec);
void terminateSharedResources();
void convertTime(unsigned int simClock[]);
void print_usage();

//record keeping variables
int numeMemAccessPerSec;
int numPageFaultsPerMemAccess;
int averageMemAccessSpeed;
int numSegFaults;
int throughput;

int main (int argc, char *argv[]) {
	//keys
	messageQueueKey = 59569;
	keySimClock = 59566;
	keyProcess = 59567;

	int maxProcess = 18; //default
	int totalProcessesCreated = 0; //keeps track of all processes created	
	int processLimit = 100; //max number of processes allowed by assignment parameters
	double terminateTime = 3; //used by setperiodic to terminate program
	unsigned int newProcessTime[2] = {0,0};
	Queue* blockedQueue = createQueue(maxProcesses);
	bool messageReceived;

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

	int line;

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
	int index;

	int counter = 0;
	while(counter < 20){ //setting max loops during development
		printf("starting\n");
		//check to see if its time for a new process to start
		if(line >= 10000){
			terminateSharedResources;
			kill(getpid(), SIGINT);
		}	
		counter++;
		if ((simClock[1] == newProcessTime[1] && simClock[0] >= newProcessTime[0]) || simClock[1] >= newProcessTime[1]){
			bool spawnNewProcess = false;
			//find available location in the resouceTable
			for (index = 0; index < maxProcess; index++){
				if (pidArray[index] == 0){
					spawnNewProcess = true;
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

			}

			newProcessTime[0] = (rand() % 500000000) + 1000000 + simClock[0];
			newProcessTime[1] = simClock[1];
			convertTime(simClock);
	
		}

		
			simClock[0] += 5555555555;
			convertTime(simClock);
		
			
//				msgrcv(messageBoxID, &message, sizeof(message), 5, IPC_NOWAIT);	
	


//				if(msgsnd(messageBoxID, &message, sizeof(message), 0) ==  -1){
//						perror("oss attempted to send message after safety check of a blocked resource");
//				}
}
wait(NULL);
} //end main()
//convertTime
void convertTime(unsigned int simClock[]){
	simClock[1] += simClock[0]/1000000000;
	simClock[0] = simClock[0]%1000000000;
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

