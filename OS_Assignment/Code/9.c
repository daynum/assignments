/*
Swapnil Kasliwal -2018B4A30311H
Dinank Vashishth -2018B5A71055H
Ayush Singh -2018B4A30924H
Vansh Madan - 2018B4A70779H
Viral Tiwari - 2018B4AA0795H
Divyanshu Singh - 2018B4AA0891H
Rohit Kushwah - 2018B5A71062H
Rijul Dhingra - 2018B4A80807H
*/


#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct queue
{
	int id;
	struct queue* next;
};

void enqueue(struct queue** head, int id)
{
	struct queue* p = (struct queue*)malloc(sizeof(struct queue));
	p->id = id;
	p->next = NULL;
	
	if (*head == NULL)
	{
		*head = p;
		return;
	}
	
	struct queue* p2 = *head;
	while (p2->next != NULL)
	{
		p2 = p2->next;
	}
	
	p2->next = p;
}

void dequeue(struct queue** head)
{
	struct queue* p = (*head)->next;
	free(*head);
	*head = p;
}

void printQueue(struct queue* head)
{
	while (head != NULL)
	{
		printf("%d, ", head->id);
		head = head->next;
	}
}



pid_t ID[3];

int child1ToMaster[2];  //pipe for sending message from C1 to Master
int child2ToMaster[2];	 //pipe for sending message from C2 to Master
int child3ToMaster[2];  //pipe for sending message from C3 to Master

int n1, n2, n3;

bool rr = false;


bool* allow;			// Used to allow the task thread to continue or pause execution
double* turn_around_time;			// Stores turnaround time of all the processes
double arrival_time[3];			// Stores arrival time of all the processes
double* waiting_time;



unsigned long long SIGNAL_CHECK_FREQ = 1000L; // 0.001 ms
unsigned long long TIME_QUANTA = 10000L;	// 0.01 ms
unsigned long long FIRSTCOMEFIRSTSERVE_INTERVAL = 10000L;	// 0.01 ms



// Shared memory locations

static int* flags[3];		// task threads are awakened and paused using these
static bool* finished[3];	// these flags tell the scheduler that the task have completed 
static double* finish_time[3];		// finished time of all the processes is stored in it
static double* execution_time[3];		// xecution time of all the processes is stored in it
static double* begin;

pthread_cond_t* cond;
pthread_mutex_t* mutex;

struct timeval tv;
double end;

// It recieves and listens to the pipes for the messages from tasks/ process that are completed.
void* reciever (void* arg)
{
	int i = *((int*)arg);
	char readBuffer[80];
	if(i==0)
	close(child1ToMaster[1]);
	else if(i==1)
	close(child2ToMaster[1]);
	else if(i==2)
	close(child3ToMaster[1]);
	
	while (!(*finished[i]))
	{	
		if(i==0)
		{
		int nbytes = read(child1ToMaster[0], readBuffer, sizeof(readBuffer));
		}
		else if(i==1)
		{
		int nbytes = read(child2ToMaster[0], readBuffer, sizeof(readBuffer));
		}
		else if(i==2)
		{
		int nbytes = read(child3ToMaster[0], readBuffer, sizeof(readBuffer));
		}
		
		if (i == 0)
			printf("Result of C1 : %s\n", readBuffer);
		else if (i == 2)
			printf("Result of C3 : %s\n", readBuffer);
		
		*(finished[i]) = true;
		
		// Calculating the turn around time
		turn_around_time[i] = *(finish_time[i]) - arrival_time[i];
		
		if (i == 1)
			break;
			
		printf("C%d stops at %fs\n", (i + 1), *(finish_time[i]));
		fflush(stdout);
	}
}

void* C2 (void* arg)
{
	while (allow[1] == NULL)
		pthread_cond_wait(&cond[1], &mutex[1]);
	
	// closing the read end of the pipe
	close(child2ToMaster[0]);
	
	int n2 = *((int*)arg);
	struct timeval tv_c2;
	int p = n2;
	FILE* fp;
	char* line = NULL;
	size_t len = 0;
	ssize_t read;
	
	for(int i=0; i<5; i++)
	n2++;
	
	n2 = p;
	fp = fopen("./data.txt", "r");
	
	int i=0;
	while(i < n2 && fp != NULL)
	{
		while (!allow[1])
			pthread_cond_wait(&cond[1], &mutex[1]);
			
		read = getline(&line, &len, fp);
		
		if (read == -1)
			break;
			
		printf("%s", line);
		fflush(stdout);
		i++;
	}
	
	fclose(fp);
	if (line)
		free(line);
	
	// getting the cpu execution time for the task
	
	struct timespec currTime;
	clockid_t threadClockId;
	pthread_getcpuclockid(pthread_self(), &threadClockId);
	clock_gettime(threadClockId, &currTime);
	
	*(execution_time[1]) = currTime.tv_sec + currTime.tv_nsec / 10e9;
	
	// getting the finish time of the process
	
	gettimeofday(&tv_c2, NULL);
	double end = tv_c2.tv_sec + tv_c2.tv_usec / 10e6;
	
	*(finish_time[1]) = end - *begin;
		
	printf("C2 stops at %fs\n", *(finish_time[1]));
	fflush(stdout);
	
	char fin[] = "Done Printing";
	write(child2ToMaster[1], fin, (strlen(fin) + 1));
}

void* C1 (void* arg)
{
	while (!allow[0])
		pthread_cond_wait(&cond[0], &mutex[0]);
	
	// closing the read end of the pipe
	close(child1ToMaster[0]);
	
	int n1 = *((int*)arg);
	unsigned long long sum = 0;
	srand(time(0));
	struct timeval tv_c1;
	int i=0;
	while (i < n1)
	{
		while (!allow[0])
			pthread_cond_wait(&cond[0], &mutex[0]);
		
		unsigned long long num = (unsigned long long)(rand() % 1000001);
		sum += num;
		
		i++;
	}
	
	// getting the cpu execution time for the task
	
	struct timespec currTime;
	clockid_t threadClockId;
	pthread_getcpuclockid(pthread_self(), &threadClockId);	
	clock_gettime(threadClockId, &currTime);
	
	*(execution_time[0]) = currTime.tv_sec + currTime.tv_nsec / 10e9;
	
	// getting the finish time of the process
	
	gettimeofday(&tv_c1, NULL);
	double end = tv_c1.tv_sec + tv_c1.tv_usec / 10e6;
	
	*(finish_time[0]) = end - *begin;
	
	char fin[12];
	sprintf(fin, "%llu", sum);
	write(child1ToMaster[1], fin, (strlen(fin) + 1));
}

void* C3 (void* arg)
{
	while (!allow[2])
		pthread_cond_wait(&cond[2], &mutex[2]);
	
	// closing the read end of the pipe
	close(child3ToMaster[0]);
	
	int n3 = *((int*)arg);
	unsigned long long sum = 0;
	struct timeval tv_c3;
	
	FILE* fp;
	char* line = NULL;
	size_t len = 0;
	ssize_t read;
	
	fp = fopen("./data.txt", "r");
	int i=0;
	while (i < n3)
	{
		while (!allow[2])
			pthread_cond_wait(&cond[2], &mutex[2]);
		
		read = getline(&line, &len, fp);
		
		if (read == -1)
			break;
			
		unsigned long long num = (unsigned long long )atoi(line);
		sum += num;
		
		i++;
	}
	
	fclose(fp);
	if (line)
		free(line);
	
	// getting the cpu execution time for the task
	
	struct timespec currTime;
	clockid_t threadClockId;
	pthread_getcpuclockid(pthread_self(), &threadClockId);
	clock_gettime(threadClockId, &currTime);
	
	*(execution_time[2]) = currTime.tv_sec + currTime.tv_nsec / 10e9;
	
	// getting the finish time of the process
	
	gettimeofday(&tv_c3, NULL);
	double end = tv_c3.tv_sec + tv_c3.tv_usec / 10e6;
	
	*(finish_time[2]) = end - *begin;
	
	char fin[12];
	sprintf(fin, "%llu", sum);
	write(child3ToMaster[1], fin, (strlen(fin) + 1));
}



int main(void)
{
	begin = mmap(NULL, sizeof *begin, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	
	// Input values of n1, n2, n3
	
	printf("Enter the values of n1, n2 and n3: ");
	scanf("%d%*c%d%*c%d%*c", &n1, &n2, &n3);
	
	int p;
	
	// Choose scheduler
	
	printf("Enter 0 for FIRSTCOMEFIRSTSERVE and 1 for Round Robin : ");
	scanf("%d%*c", &p);
	
	rr = (bool)p;
	
	if (rr)
	{
		printf("Enter the Time Quanta in nanoseconds : ");
		scanf("%llu%*c", &TIME_QUANTA);
	}
	
	
	allow = (bool*)malloc(sizeof(bool) * 3);
	waiting_time = (double*)malloc(sizeof(double) * 3);
	turn_around_time = (double*)malloc(sizeof(double) * 3);
	arrival_time[0] = arrival_time[1] = arrival_time[2] = 0;
	
	cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t) * 3);
	mutex = (pthread_mutex_t*)malloc(sizeof(pthread_cond_t) * 3);
	
	for(int i=0; i<5; i++)
	{
	int scheduling;
	}
	int i=0;
	while (i < 3)
	{	
		if(i==0)
		pipe(child1ToMaster);
		if(i==1)
		pipe(child2ToMaster);
		if(i==2)
		pipe(child3ToMaster);
		
		
		mutex[i] = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
		cond[i] = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
		
		
		finished[i] = false;
		allow[i] = false;
		
		
		flags[i] = mmap(NULL, sizeof *flags[i], PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		*(flags[i]) = 0;
		
		finished[i] = mmap(NULL, sizeof *finished[i], PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		*(finished[i]) = 0;
		
		execution_time[i] = mmap(NULL, sizeof *execution_time[i], PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		*(execution_time[i]) = 0;
		
		finish_time[i] = mmap(NULL, sizeof *finish_time[i], PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		*(finish_time[i]) = 0;
		
		i++;
	}
	
	struct queue* q = NULL;
	
	// Create the first child process C1

	if ((ID[0] = fork()) == 0)
	{
		// Created task thread of C1
		pthread_t t1;		
		pthread_create(&t1, NULL, &C1, (void*)(&n1));
		
		// Inside monitor thread of C1
		
		int init_flag = 0;
		
		struct timespec tim1, tim2;
		tim1.tv_sec = 0;
		tim1.tv_nsec = SIGNAL_CHECK_FREQ;
		
		while (!(*(finished[0])))
		{
			// Check if the shared memory space is toggled
			// *flags[i] = 1 for awake task and 0 for pause task
			// We check for toggle to send a signal
			while (init_flag == *(flags[0]))
			{
				nanosleep(&tim1, &tim2);
			}
			init_flag = *(flags[0]);
			
			if (*(flags[0]))
			{
				allow[0] = true;
				pthread_cond_signal(&cond[0]);
			}
			else
				allow[0] = false;
		}
	}
	else
	{
		enqueue(&q, 0);
		
		// Create the second child process C2
		if ((ID[1] = fork()) == 0)
		{
			// Created task thread of C2
			
			pthread_t t2;			
			pthread_create(&t2, NULL, &C2, (void*)(&n2));
			
			// Inside monitor thread of C2
			
			int init_flag = 0;
			
			struct timespec tim1, tim2;
			tim1.tv_sec = 0;
			tim1.tv_nsec = SIGNAL_CHECK_FREQ;
			
			while (!(*(finished[1])))
			{
				// Check if the shared memory space is toggled
				// *flags[i] = 1 for awake task and 0 for pause task
				// We check for toggle to send a signal
				while (init_flag == *(flags[1]))
				{
					nanosleep(&tim1, &tim2);
				}
				init_flag = *(flags[1]);
				
				if (*(flags[1]))
				{
					allow[1] = true;
					pthread_cond_signal(&cond[1]);
				}
				else
					allow[1] = false;
			}
		}
		else
		{
			enqueue(&q, 1);
			
			// Create the third child process C3
			if ((ID[2] = fork()) == 0)
			{
				// Created task thread of C3
				
				pthread_t t3;				
				pthread_create(&t3, NULL, &C3, (void*)(&n3));
				
				int init_flag = 0;
				
				// Inside monitor thread of C3
				
				struct timespec tim1, tim2;
				tim1.tv_sec = 0;
				tim1.tv_nsec = SIGNAL_CHECK_FREQ;
				
				while (!(*(finished[2])))
				{
					// Check if the shared memory space is toggled
					// *flags[i] = 1 for awake task and 0 for pause task
					// We check for toggle to send a signal					
					while (init_flag == *(flags[2]))
					{
						nanosleep(&tim1, &tim2);
					}
					init_flag = *(flags[2]);
					
					if (*(flags[2]))
					{
						allow[2] = true;
						pthread_cond_signal(&cond[2]);
					}
					else
						allow[2] = false;
				}
			}
			else
			{
				enqueue(&q, 2);
				
				// Create 3 threads to listen to the pipes
				// Each thread receives the completed message from the children
				
				pthread_t listen_c1, listen_c2, listen_c3;
				int i1 = 0, i2 = 1, i3 = 2;
				
				pthread_create(&listen_c1, NULL, &reciever, (void*)(&i1));
				pthread_create(&listen_c2, NULL, &reciever, (void*)(&i2));
				pthread_create(&listen_c3, NULL, &reciever, (void*)(&i3));
				
				struct timespec tim1, tim2;
				tim1.tv_sec = 0;
				
				// begin stores the arrival time
				// Since all processes have been created and queued,
				// all of them have the same Arrival Time
				
				gettimeofday(&tv, NULL);	
				*begin = tv.tv_sec + tv.tv_usec / 10e6;
				
				// Implementation of Round Robin
				if (rr)
				{
					tim1.tv_nsec = TIME_QUANTA;
					while (q != NULL)
					{
						int i = q->id;
						
						gettimeofday(&tv, NULL);
						end = tv.tv_sec + tv.tv_usec / 10e6;
							
						if (!(*(finished[i])))
						{
							printf("C%d starts at %fs\n", (i + 1), (end - *begin));
							fflush(stdout);
						}
						
						*(flags[i]) = 1;
						
						nanosleep(&tim1, &tim2);
						
						*(flags[i]) = 0;
						
						if (*(finished[i]))
							dequeue(&q);
						else
						{
							dequeue(&q);
							enqueue(&q, i);
						}
					}
				}
				
				// Implementation of FIRSTCOMEFIRSTSERVE
				else
				{
					tim1.tv_nsec = FIRSTCOMEFIRSTSERVE_INTERVAL;
					while (q != NULL)
					{
						int i = q->id;
					
						gettimeofday(&tv, NULL);
						end = tv.tv_sec + tv.tv_usec / 10e6;
						
						if (!(*(finished[i])))
						{
							printf("C%d starts at %fs\n", (i + 1), (end - *begin));
							fflush(stdout);
						}
						
						*(flags[i]) = 1;
						
						while (!(*(finished[i])))
						{
							nanosleep(&tim1, &tim2);
						}
						
						*(flags[i]) = 0;
						
						dequeue(&q);
					}
				}
				
				
				for (int i = 0; i < 3; i++)
					waiting_time[i] = turn_around_time[i] - *(execution_time[i]);
					
				printf("\n");
				
				int i = 0; 
				while (i < 3)
				{
					printf("turn_around_time of C%d = %fs\n", (i + 1), turn_around_time[i]);
					printf("execution_time of C%d = %fs\n", (i + 1), *(execution_time[i]));
					printf("waiting_time of C%d = %fs\n", (i + 1), waiting_time[i]);
					printf("\n");
					
					i++;
				}
				
				
			}
		}
	}
	return 0;
}



