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
#include <semaphore.h>
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
#define NUM_THREADS 2
#include <sys/time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>


#define SERVER_IP "10.0.0.143"

extern int errno;
extern void int_handler();
extern void broken_pipe_handler();
static int server_sock, client_sock;
static int fromlen, i, j, num_sets;
static char c;
static FILE *fp;
static struct sockaddr_in server_sockaddr, client_sockaddr;

//Record global variables
char *buffer;
unsigned int val=44100;
int dir;
int rc;
int size;
int loopcomp=1;
snd_pcm_uframes_t frames=32;
unsigned int period_time;
snd_pcm_t *handle;
snd_pcm_hw_params_t *params;
int size1=32;
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
sem_t sem_t1,sem_t2;
/*
* SIGINT handler
*/
void int_handler()
{
  char ch;
  printf("Enter y to close sockets or n to keep open:");
  scanf("%c", &ch);
  
  if(ch == 'y') 
  {
    printf("\nsockets are being closed\n");
    pthread_join(threads[0], NULL);
    close(client_sock);
    close(server_sock);

    fclose(serverFilePtr);
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);

  }
  exit(0);

}

void broken_pipe_handler()
{
  printf("Broken pipe handler");
  pthread_join(threads[0], NULL);
  close(client_sock);
  close(server_sock);

  fclose(serverFilePtr);
  snd_pcm_drain(handle);
  snd_pcm_close(handle);
  free(buffer);

  
  exit(0);

}

/*
Threading functions
*/
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
//Read time of day
double readTOD(void)
{
    struct timeval tv;
    double ft=0.0;
    if ( gettimeofday(&tv, NULL) != 0 )
    {
        perror("readTOD");
        return 0.0;
    }
    else
    {
        ft = ((double)(((double)tv.tv_sec) + (((double)tv.tv_usec)/1000000)));
    }
    return ft;
}
/*
*/
static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params)
{
  int dir=1;
  int rc;
  int err;
  /* Fill it in with default values. */
   err =snd_pcm_hw_params_any(handle, params);
   if (err < 0) {
    printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
    return err;
  }
  /* Set the desired hardware parameters. */
  /* Interleaved mode */
  err=snd_pcm_hw_params_set_access(handle, params,SND_PCM_ACCESS_RW_INTERLEAVED);
  if (err < 0) {
    printf("Access type not available for record: %s\n", snd_strerror(err));
    return err;
  }
  /* Signed 16-bit little-endian format */
  err=snd_pcm_hw_params_set_format(handle, params,SND_PCM_FORMAT_S16_LE);
  if (err < 0) {
   printf("Sample format not available for record: %s\n", snd_strerror(err));
   return err;
  }
  /* Two channels (stereo) */
  err=snd_pcm_hw_params_set_channels(handle, params, 2);
  if (err < 0) {
    printf("Channels count 2 not available for record: %s\n", snd_strerror(err));
    return err;
  }
  /* 44100 bits/second sampling rate (CD quality) */
  val = 44100;
  err=snd_pcm_hw_params_set_rate_near(handle, params,&val, &dir);
  if (err < 0) {
    printf("Rate %iHz not available for record: %s\n", rate, snd_strerror(err));
    return err;
  }
  /* Set period size to 32 frames. */
  err=snd_pcm_hw_params_set_period_size_near(handle,params, &frames, &dir);
  if (err < 0) {
    printf("Unable to set period size for record: %s\n", snd_strerror(err));
    exit(1);
  }
  /* Write the parameters to the driver */
  rc = snd_pcm_hw_params(handle, params);
  if (rc < 0) {
    fprintf(stderr,
            "unable to set hw parameters: %s\n",
            snd_strerror(rc));
    exit(1);
  }
  /* Use a buffer large enough to hold one period */
  snd_pcm_hw_params_get_period_size(params,&frames, &dir);
 
  /* We want to loop for 5 seconds */
  snd_pcm_hw_params_get_period_time(params,&period_time, 0);

  return 0;
}

void waitForClientToConnect()
{
	for(;;) {
    /* Listen on the socket */
    if(listen(server_sock, 5) < 0) {
      perror("server: listen");
      exit(1);
    }
    else
      {printf("Listening to port 1234\n");
      }
    /* Accept connections */
    if((client_sock=accept(server_sock,(struct sockaddr *)&client_sockaddr,&fromlen)) < 0) 
       {
      perror("server: accept");
      exit(1);
      }
    else
      {
        printf("Connection made on port 1234\n");
        break;
      }      
    } 
	fp = fdopen(client_sock, "r"); 
}

static int record(snd_pcm_t *handle)
{
	FILE *serverFilePtr;
	long loops, num;
	int rc;
	double start = 0, stop = 0;
	double sum = 0;
	double avg_exec_time, delay;
	loops = 5000000 / period_time;
	num=loops;
	serverFilePtr=fopen("record.raw","w");
	rc = snd_pcm_readi(handle, buffer, frames);
	
	waitForClientToConnect();
    int lengthOfString=0;
    
  while (1) 
  {
    rc = snd_pcm_readi(handle, buffer, frames);
    if (rc == -EPIPE) {
    /* EPIPE means overrun */
    fprintf(stderr, "overrun occurred\n");
    snd_pcm_prepare(handle);
    } else if (rc < 0) {
      fprintf(stderr,
             "error from read: %s\n",
              snd_strerror(rc));
    } else if (rc != (int)frames) {
      fprintf(stderr, "short read, read %d frames\n", rc);
    }
    //rc = write(1, buffer, size);
    fwrite(buffer,1,size,serverFilePtr);
    lengthOfString=strlen(buffer); 
    fprintf(stderr,"\n buffer length: %d \n", lengthOfString);
    send(client_sock, (char *)(&lengthOfString), sizeof(int), 0);   //This sends to the server the number of bytes that the client has and wishes to transfer.  
    send(client_sock, (char *)(buffer), lengthOfString, 0);
  } 
  fclose(serverFilePtr);
  snd_pcm_drain(handle);
  snd_pcm_close(handle);
  free(buffer);
  close(client_sock);
}

void soundCardSetup()
{
	snd_pcm_hw_params_alloca(&params);
	struct timespec timeNowRead;
	struct timespec timeOut;
	clock_gettime(my_clock,&timeNowRead);
	printf("Read Time is:%lds %ldns\n",timeNowRead.tv_sec,timeNowRead.tv_nsec);
	timeOut.tv_sec=timeNowRead.tv_sec+10;       
	size = frames * 4; /* 2 bytes/sample, 2 channels */
	fprintf(stderr,"recordThread runs");
	rc = snd_pcm_open(&handle, "default",
					SND_PCM_STREAM_CAPTURE, 0);
	if (rc < 0) {
	fprintf(stderr,
			"unable to open pcm device: %s\n",
			snd_strerror(rc));
	exit(1);
	}

	if ((rc = set_hwparams(handle, params)) < 0) {
	  printf("Setting of hwparams failed: %s\n",snd_strerror(rc));
	  exit(EXIT_FAILURE);
	}
	record(handle);
}

void *recordThread(void *threadp)
{
  
 
}

void initializeServer()
{
  
  char hostname[64];
  struct hostent *hp;
  struct linger opt;
  int sockarg;
  if((hp = (struct hostent*) gethostbyname(SERVER_IP)) == NULL) {
    fprintf(stderr, "%s: host unknown.\n", hostname);
    exit(1);
  }

  if((server_sock=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("server: socket");
    exit(1);
  }

  bzero((char*) &server_sockaddr, sizeof(server_sockaddr));
  server_sockaddr.sin_family = AF_INET;
  server_sockaddr.sin_port = htons(1234);
  bcopy (hp->h_addr, &server_sockaddr.sin_addr, hp->h_length);

  /* Bind address to the socket */
  if(bind(server_sock, (struct sockaddr *) &server_sockaddr,
            sizeof(server_sockaddr)) < 0) 
    {
    perror("server: bind");
    exit(1);
  }

    /* turn on zero linger time so that undelivered data is discarded when
       socket is closed
     */
    opt.l_onoff = 1;
    opt.l_linger = 0;

    sockarg = 1;
 
    setsockopt(server_sock, SOL_SOCKET, SO_LINGER, (char*) &opt, sizeof(opt));
    setsockopt(client_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&sockarg, sizeof(int));
  signal(SIGINT, int_handler);
  signal(SIGPIPE, broken_pipe_handler);

  //serve_clients();

}


int main (int argc, char *argv[])
{
	int rc;
	int i;
	cpu_set_t cpuset;
	mainpid=getpid();
  buffer = (char *) malloc(size);

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

	for(i=0; i < NUM_THREADS; i++)
	{
		rc=pthread_attr_init(&rt_sched_attr[i]);
		rc=pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
		rc=pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
		rt_param[i].sched_priority=rt_max_prio-i-1;
		pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);
		threadParams[i].threadIdx=i;
	}
/*********************************************************************************/
  initializeServer();
  soundCardSetup();
/*********************************************************************************/

	//Thread creation
	pthread_create(&threads[0],   // pointer to thread descriptor
					  (void *)0,     // use default attributes
					  recordThread, // thread function entry point
					  (void *)&(threadParams[0]) // parameters to pass in		//Cant pass nothing so just pass a number
					 );
        
#if 0
	pthread_create(&threads[1],   // pointer to thread descriptor
					  (void *)0,     // use default attributes
					 serve_clients, // thread function entry point
					  (void *)&(threadParams[0]) // parameters to pass in		//Cant pass nothing so just pass a number
					 );
#endif
//sequence//
  
/*********************************************************************************/
    pthread_join(threads[0], NULL);
	fprintf(stderr,"Back to main");
/*********************************************************************************/
  printf("\nDone\n");
}
