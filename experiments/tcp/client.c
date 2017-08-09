/*
Authors: 	HJ Jayakrishna, Richard Noronha, Sahana Sadagopan
University: University of Colorado Boulder
Date: 		8th August 2017
Course: 	Real time Embedded Systems
Professor: Sam Siewart, PhD
*/
/*
Description: 	
This is the part of the project on VOIP between two NVIDIA Jetson boards over TCP connection.
This code is the initialization code and scheduler of the various threads involved at the server side of the system
*/
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#define NUM_THREADS 2

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>


#define SERVER_IP "127.0.0.1"
#define LOCAL_PORT 1234

extern int errno;
extern void broken_pipe_handler();

clockid_t my_clock;
pthread_mutex_t attSem;

// POSIX thread declarations and scheduling attributes
typedef struct
{
    int threadIdx;
} threadParams_t;
pthread_t threads[NUM_THREADS];
threadParams_t threadParams[NUM_THREADS];

//Scheduler declarations
pthread_attr_t rt_sched_attr[NUM_THREADS];
int rt_max_prio, rt_min_prio;
struct sched_param rt_param[NUM_THREADS];
struct sched_param main_param;
pthread_attr_t main_attr;
pid_t mainpid;

/*
Scheduler printing
*/
void print_scheduler(void)
{
   int schedType;

   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
     case SCHED_FIFO:
           printf("Pthread Policy is SCHED_FIFO\n");
           break;
     case SCHED_OTHER:
           printf("Pthread Policy is SCHED_OTHER\n");
       break;
     case SCHED_RR:
           printf("Pthread Policy is SCHED_OTHER\n");
           break;
     default:
       printf("Pthread Policy is UNKNOWN\n");
   }

}


void cleanerFunction()
{
	int rc;
	//Clearing up the system
	rc=pthread_mutex_destroy(&attSem);
	//printf("\n%d\n",rc);
	//if(pthread_mutex_destroy(&attSem)!=0)
	if(rc<0)
		perror("Mutex destroyed");	
	printf("\nDone\n");	
}

void *threadB(void *threadp)
{
	//clockid_t my_clock;
	struct timespec timeNow_Update;
	
	//Mutex critical section
	pthread_mutex_lock(&attSem);
	printf("Update time is:%lds %ldns\n",timeNow_Update.tv_sec,timeNow_Update.tv_nsec);
	
	
	
	pthread_mutex_unlock(&attSem);
	
}

void *threadA(void *threadp)
{
	struct timespec timeNowRead;
	struct timespec timeOut;

	
	{
	clock_gettime(my_clock,&timeNowRead);
	printf("Read Time is:%lds %ldns\n",timeNowRead.tv_sec,timeNowRead.tv_nsec);
	//Set the time clock to 10sec more than the current time to obtain the semaphore in that time
	timeOut.tv_sec=timeNowRead.tv_sec+10;				
	//Mutex critical section
	//pthread_mutex_lock(&attSem);
	if((pthread_mutex_timedlock(&attSem, &timeOut))!=0)
     {
		 //code for if the semaphore is busy
		 clock_gettime(my_clock,&timeNowRead);
         printf("No new data available at %lds %ldns\n",timeNowRead.tv_sec,timeNowRead.tv_nsec);
         //continue;
     }
     else
     {
		 clock_gettime(my_clock,&timeNowRead);
         printf("ThreadA got mutex at %lds %ldns\n",timeNowRead.tv_sec,timeNowRead.tv_nsec);		 
		//WHat to do if the semaphore is available
		pthread_mutex_unlock(&attSem);
		//usleep(40);
	 }
	}
}

int client_sock;
FILE *fp;
struct sockaddr_in client_sockaddr;
    
void broken_pipe_handler()
{

	printf("\nbroken pipe signal received\n");
	cleanerFunction();
	exit(0);

}

void initializeClient()
{
	char c;
    char hostname[64];
    struct hostent *hp;
    struct linger opt;
    int sockarg;
	
    
	//gethostname(hostname, sizeof(hostname));

	if((hp = gethostbyname(SERVER_IP)) == NULL) {
		fprintf(stderr, "%s: unknown host.\n", hostname);
		exit(1);
	}
        else
        {
            printf("Connected to %s\n", SERVER_IP);
        }

	if((client_sock=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("client: socket");
		exit(1);
	}

	client_sockaddr.sin_family = AF_INET;
	printf("Will connect to port %d \n", LOCAL_PORT);
	client_sockaddr.sin_port = htons(LOCAL_PORT);
	bcopy(hp->h_addr, &client_sockaddr.sin_addr, hp->h_length);

    /* discard undelivered data on closed socket */ 
    opt.l_onoff = 1;
    opt.l_linger = 0;

    sockarg = 1;

    setsockopt(client_sock, SOL_SOCKET, SO_LINGER, (char*) &opt, sizeof(opt));
    setsockopt(client_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&sockarg, sizeof(int));

}


void *requestServer()
{
	int num_sets;
	char fileToRead_Ptr[50];
      
    
    if(connect(client_sock, (struct sockaddr*)&client_sockaddr, sizeof(client_sockaddr)) < 0) 
    {
	perror("client: connect");
	exit(1);
    }
    else
    {
        printf("CONNECTED TO REMOTE SERVER\n");
    }


	signal(SIGPIPE, broken_pipe_handler);

	fp = fdopen(client_sock, "r");
	
	char ack='1';
	char buffer[50];
	
	recv(client_sock, (char *)&num_sets, sizeof(int), 0);
	printf("number of sets = %d\n", num_sets);
	send(client_sock, &ack, sizeof(char), 0);
	recv(client_sock, (char *)&buffer, num_sets, 0);
	
	int count=0;
	printf("%s \n", buffer);
	/*while(count<num_sets) 
	{		
		//prints the characters that were received
		count++;
		c = fgetc(fp);			//use this for obtaining individial characters from the file pointer
		
		putchar(c);
	} */
	close(client_sock);

}

int main (int argc, char *argv[])
{
	int rc;
	int i;

	cpu_set_t cpuset;
	mainpid=getpid();

	//Scheduler
	print_scheduler();
	rc=sched_getparam(mainpid, &main_param);
	if (rc) 
   {
       printf("ERROR; sched_setscheduler rc is %d\n", rc);
       perror(NULL);
       exit(-1);
   }
   //Obtain the priorities of the scheduler
	rt_max_prio = sched_get_priority_max(SCHED_FIFO);
	rt_min_prio = sched_get_priority_min(SCHED_FIFO);

	main_param.sched_priority=rt_max_prio;
	rc=sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
	if(rc < 0) perror("main_param");
	print_scheduler();

	/*
	pthread_attr_getscope(&main_attr, &scope);

	if(scope == PTHREAD_SCOPE_SYSTEM)
	printf("PTHREAD SCOPE SYSTEM\n");
	else if (scope == PTHREAD_SCOPE_PROCESS)
	printf("PTHREAD SCOPE PROCESS\n");
	else
	printf("PTHREAD SCOPE UNKNOWN\n");

	printf("rt_max_prio=%d\n", rt_max_prio);
	printf("rt_min_prio=%d\n", rt_min_prio);
	*/
	for(i=0; i < NUM_THREADS; i++)
	{
		rc=pthread_attr_init(&rt_sched_attr[i]);
		rc=pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
		rc=pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
		//rc=pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &cpuset);

		rt_param[i].sched_priority=rt_max_prio-i-1;
		pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);

		threadParams[i].threadIdx=i;
	}
/*********************************************************************************/
   //Mutex creation
	pthread_mutex_init(&attSem,NULL);
	initializeClient();
/*********************************************************************************/

	//Thread creation
	pthread_create(&threads[0],   // pointer to thread descriptor
					  (void *)0,     // use default attributes
					  requestServer, // thread function entry point
					  (void *)&(threadParams[0]) // parameters to pass in		//Cant pass nothing so just pass a number
					 );

	/*pthread_create(&threads[1],   // pointer to thread descriptor
					  (void *)0,     // use default attributes
					 threadB, // thread function entry point
					  (void *)&(threadParams[0]) // parameters to pass in		//Cant pass nothing so just pass a number
					 );*/
/*********************************************************************************/
    pthread_join(threads[0], NULL);
	//pthread_join(threads[1], NULL);
/*********************************************************************************/
	cleanerFunction();
}
