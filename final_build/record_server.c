/**************************************************************************
* Authors: 	HJ Jayakrishnan, Richard Noronha, Sahana Sadagopan
* University:   University of Colorado Boulder
* Date: 	8th August 2017
* Course: 	Real time Embedded Systems
* Professor:    Sam Siewart, PhD


* Description: This file is the server which records the audio and sends it
              across to the client.

* Instructions: 1. Change SERVER_IP to the IP of your server PC.
*               2. Run with sudo.
*               3. Ensure a LAN is available with the client and server
                    in same subnet.
**************************************************************************/
#define _GNU_SOURCE
#define ALSA_PCM_NEW_HW_PARAMS_API
#define SERVER_IP "10.0.0.192"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>

#define NUM_THREADS 2
#define RINGBUFFER_ROWS 256
#define RINGBUFFER_COLUMNS 128
#define NUM_FRAMES 32

/* TCP connection and interrupt handling */
extern void int_handler();
extern void broken_pipe_handler();
static int server_sock, client_sock;
static int fromlen;
static FILE *fp;
static struct sockaddr_in server_sockaddr, client_sockaddr;

/* global variables for record */
unsigned int val=44100;
int dir;
int size;
unsigned int period_time;
snd_pcm_uframes_t frames = NUM_FRAMES;
snd_pcm_t *handle;
snd_pcm_hw_params_t *params;
char record_array[RINGBUFFER_ROWS][RINGBUFFER_COLUMNS];// Ring buffer

/* POSIX thread declarations and scheduling attributes */
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
sem_t tcpSem, rcdSem;
pthread_mutex_t mutexsem;

/**************************************************************************
*   Function - int_handler
*   Parameters - none
*   Returns - void
*   Purpose - Clean up in the event of a Ctrl+C
/**************************************************************************/
void int_handler()
{
  char ch;
  printf("Enter y to close sockets or n to keep open:");
  scanf("%c", &ch);

  if(ch == 'y')
  {
    printf("\nsockets are being closed\n");
    //pthread_join(threads[0], NULL);
    close(client_sock);
    close(server_sock);
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
  }
  exit(0);

}

/**************************************************************************
*   Function - broken_pipe_handler
*   Parameters - none
*   Returns - void
*   Purpose - Clean up in the event of a broken pipe
/**************************************************************************/

void broken_pipe_handler()
{
  fprintf(stderr,"Broken pipe handler");
  pthread_join(threads[0], NULL);
  pthread_join(threads[1], NULL);
  close(client_sock);
  close(server_sock);
  snd_pcm_drain(handle);
  snd_pcm_close(handle);
  exit(0);
}

/**************************************************************************
*   Function - print_scheduler
*   Parameters - none
*   Returns - void
*   Purpose - Prints the current Linux scheduler
/**************************************************************************/
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
/**************************************************************************
*   Function - readTOD
*   Parameters - none
*   Returns - Current Time in seconds
*   Purpose - Calculating time delays
*   TO-DO - Move to POSIX Real Time clock
/**************************************************************************/
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
/**************************************************************************
*   Function - set_hwparams
*   Parameters - pcm handle and the hardware Parameters
*   Returns - error code in the event of a failure, else 0
*   Purpose - Initialize hardware parameters for ALSA
/**************************************************************************/
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
    printf("Rate %iHz not available for record: %s\n", val, snd_strerror(err));
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

/**************************************************************************
*   Function - readbuf
*   Parameters - pcm handle, pointer to the audio buffer where audio is stored, length
*   Returns - error code/ bytes transferred by DMA to host
*   Purpose - Tranfer audio from sound card to host and ensures NUM_FRAMES*4 bytes
/**************************************************************************/

long readbuf(snd_pcm_t *handle, char *buf, long len)
{
  long r;
  int frame_bytes = 4;
  do {
      r = snd_pcm_readi(handle, buf, len);
      if (r > 0) {
        buf += r * frame_bytes;
        len -= r;
      }
      // fprintf(stderr,"r = %li, len = %li\n", r, len);
  if(r<0)
    fprintf(stderr,"error: %s\n", snd_strerror(r));
  } while (r >= 1 && len > 0);
return r;
}

/**************************************************************************
*   Function - record
*   Parameters - pcm handle
*   Returns - error in case of failed DMA read
*   Purpose - Listens on the TCP socket and starts reading audio from sound card
/**************************************************************************/

static int record(snd_pcm_t *handle){

  int rc,i;
  int fd;
  int initial_rows_dma=0;
  int initial_rows_tcp=0;
  int count = 0;

  double start = 0, stop = 0;
  double sum = 0;
  double avg_exec_time, delay, avg_exec_time_s;


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

  while(1){
    // start= readTOD();
    // fprintf(stderr,"\n waiting at recording thread\n");
    // pthread_mutex_lock(&mutexsem);
    sem_wait(&rcdSem);
    // fprintf(stderr,"\n got rcdSem and now in recording\n");
    for(initial_rows_dma=0;initial_rows_dma<RINGBUFFER_ROWS; initial_rows_dma++)
      {
        if (readbuf(handle, (char *)record_array[initial_rows_dma], frames)<0){
          fprintf(stderr, "read error \n");
          break;
        }
        // rc = write(1, (char *)record_array[initial_rows_dma], size);
      }
    //  stop = readTOD();
    //  delay = (double)(stop - start);

    //  fprintf(stderr,"Record delay in ms%lf\n",delay*1000);
    //  sum += delay;
    //  pthread_mutex_unlock(&mutexsem);
    sem_post(&tcpSem);
  }
  // fprintf(stderr,"Record delay in ms: %lf\n",(sum/count)*1000);
  snd_pcm_drain(handle);
  snd_pcm_close(handle);
  close(client_sock);
}

/**************************************************************************
*   Function - sendThread
*   Parameters - thread ID
*   Returns - void
*   Purpose - Sends the audio packet to the client
/**************************************************************************/

void *sendThread(void *threadp)
{
  int initial_rows_tcp = 0;
  int count = 0;

  double start = 0, stop = 0;
  double sum = 0;
  double avg_exec_time, delay, avg_exec_time_s;

  while(1){

    // fprintf(stderr,"\n waiting at sendTHread\n");
    sem_wait(&tcpSem);
    // fprintf(stderr,"\n Got tcpSem and now sending audio\n");
    // start= readTOD();
    // pthread_mutex_lock(&mutexsem);
    for(initial_rows_tcp=0;initial_rows_tcp<RINGBUFFER_ROWS; initial_rows_tcp++)
    {
      // rc = write(1, (char *)record_array[initial_rows_tcp], size);
      send(client_sock, (&record_array[initial_rows_tcp][RINGBUFFER_COLUMNS]), RINGBUFFER_COLUMNS, 0);
    }
    sem_post(&rcdSem);
    // pthread_mutex_unlock(&mutexsem);
    // stop = readTOD();
    // delay = (double)(stop - start);
    // fprintf(stderr,"%lf, ", delay*1000);
    // sum += delay;
  }
  // fprintf(stderr,"Send delay in ms : %lf\n",(sum/count)*1000);
}

/**************************************************************************
*   Function - recordThread
*   Parameters - thread ID
*   Returns - void
*   Purpose - Opens PCM device and initializes PCM parameters
/**************************************************************************/

void *recordThread(void *threadp)
{
  int rc;
  snd_pcm_hw_params_alloca(&params);
  size = frames * 4; /* 2 bytes/sample, 2 channels */
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

/**************************************************************************
*   Function - initializeServer
*   Parameters - none
*   Returns - void
*   Purpose - Initialize TCP server
/**************************************************************************/

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

/**************************************************************************
*   Function - main
*   Parameters - none
*   Returns - void
*   Purpose - Initialize threads, semaphores and cores
/**************************************************************************/

int main (int argc, char *argv[])
{

  sem_init(&rcdSem,0,0);
  sem_init(&tcpSem,0,1);
  int rc;
  int i;
	
  /* Setting core affinity */
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(1, &cpuset);
  mainpid=getpid();

  /* Scheduler */
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
     pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &cpuset);
     threadParams[i].threadIdx=i;
   }
/*********************************************************************************/
  initializeServer();
/*********************************************************************************/

	//Thread creation
  pthread_create(&threads[0],   // pointer to thread descriptor
		 &rt_sched_attr[0],     // use default attributes
		 sendThread, // thread function entry point
		 (void *)&(threadParams[0]) // parameters to pass in		//Cant pass nothing so just pass a number
		);

  pthread_create(&threads[1],   // pointer to thread descriptor
		 &rt_sched_attr[1],     // use default attributes
		 recordThread, // thread function entry point
		 (void *)&(threadParams[1]) // parameters to pass in		//Cant pass nothing so just pass a number
		);

//sequence//

/*********************************************************************************/
  pthread_join(threads[0], NULL);
  pthread_join(threads[1],NULL);
/*********************************************************************************/
  printf("\nDone\n");
}
