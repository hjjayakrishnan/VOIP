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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>


#include <alsa/asoundlib.h>

#define NUM_THREADS 2
#define ALSA_PCM_NEW_HW_PARAMS_API
#define SERVER_IP "10.0.0.143"
#define LOCAL_PORT 1234

clockid_t my_clock;
pthread_mutex_t attSem;

extern int errno;
extern void broken_pipe_handler();

// ALSA playback parameters

static unsigned int rate = 44100;
unsigned int size;
snd_pcm_uframes_t frames = 32;
unsigned int period_time;
char *buffer_s128_1, *buffer_s128_2, *buffer_s32;
snd_pcm_t *handle;

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



void int_handler()
{
  printf("\n Freeing buffers and closing pcm handles");
  snd_pcm_drain(handle);
  snd_pcm_close(handle);
  free(buffer_s32);
  free(buffer_s128_1);
  free(buffer_s128_2);

	exit(0);

}
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

static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params){

  unsigned int rrate;
  int err, dir=1, rc;
  snd_pcm_uframes_t size_;
  /* Allocate hardware parameters  */

  /* Fill it in with default values. */
  err = snd_pcm_hw_params_any(handle, params);
  if (err < 0) {
    printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
    return err;
  }
  /* Set the desired hardware parameters. */

  /* Interleaved mode */
  err = snd_pcm_hw_params_set_access(handle, params,
                      SND_PCM_ACCESS_RW_INTERLEAVED);
  if (err < 0) {
    printf("Access type not available for playback: %s\n", snd_strerror(err));
    return err;
  }

  /* Signed 16-bit little-endian format */
  err = snd_pcm_hw_params_set_format(handle, params,
                              SND_PCM_FORMAT_S16_LE);

  if (err < 0) {
   printf("Sample format not available for playback: %s\n", snd_strerror(err));
   return err;
  }

  /* Two channels (stereo) */
  err = snd_pcm_hw_params_set_channels(handle, params, 2);

  if (err < 0) {
    printf("Channels count 2 not available for playbacks: %s\n", snd_strerror(err));
    return err;
  }

  /* 44100 bits/second sampling rate (CD quality) */
  rrate = rate;
  err = snd_pcm_hw_params_set_rate_near(handle, params,
                                  &rate, &dir);
  if (err < 0) {
    printf("Rate %iHz not available for playback: %s\n", rate, snd_strerror(err));
    return err;
  }
  if (rrate != rate) {
    printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
    return -EINVAL;
  }

  /* Set period size to 32 frames. */

  err = snd_pcm_hw_params_set_period_size_near(handle,
                              params, &frames, &dir);
  if (err < 0) {
    printf("Unable to set period size for playback: %s\n", snd_strerror(err));
    return err;
  }

  /* Write the parameters to the driver */
  rc = snd_pcm_hw_params(handle, params);
  if (rc < 0) {
    fprintf(stderr,
            "unable to set hw parameters: %s\n",
            snd_strerror(rc));
    exit(1);
  }

  /* We want to loop for 5 seconds */
  snd_pcm_hw_params_get_period_time(params,
                                    &period_time, 0);
  printf("period time:%d uS\n", period_time);

  err = snd_pcm_hw_params_get_buffer_size(params, &size_);
  if (err < 0) {
          printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
          return err;
  }
  printf("buffer size:%d frames\n", size_);

  return 0;
}

static int playBack(snd_pcm_t *handle){
  /* TCP */

  int num_sets = size;
  int p;
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

  long loops, num;
  int rc;
  double start = 0, stop = 0;
  double sum = 0;
  double avg_exec_time, delay;

  recv(client_sock, (char *)&num_sets, sizeof(int), 0);
  recv(client_sock, (char *)buffer_s128_1, num_sets, 0);
  fprintf(stderr,"\nin playback : %d", strlen(buffer_s128_1));

  while (1) {

    rc = snd_pcm_writei(handle, buffer_s128_1, frames);

    if (rc == -EPIPE) {
      /* EPIPE means underrun */
      fprintf(stderr, "underrun occurred\n");
      snd_pcm_prepare(handle);

    } else if (rc < 0) {
      fprintf(stderr,"error from writei: %s\n", snd_strerror(rc));

    }  else if (rc != (int)frames) {
      fprintf(stderr, "short write, write %d frames\n", rc);

    }


    recv(client_sock, (char *)&num_sets, sizeof(int), 0);
    fprintf(stderr,"\nnum sets : %d", num_sets);
    recv(client_sock, (char *)buffer_s128_1, num_sets, 0);
    fprintf(stderr,"\nloop : %d", strlen(buffer_s128_1));

  }

  snd_pcm_drain(handle);
  snd_pcm_close(handle);
  free(buffer_s128_1);
  free(buffer_s128_2);
  free(buffer_s32);

}

void *threadA(void *threadp)
{

  playBack(handle);

}

int main (int argc, char *argv[])
{
	int rc;
	int i;

  /* ALSA parameters */
  const char *device = "hw:1,0";
  snd_pcm_hw_params_t *hwparams;
  snd_pcm_hw_params_alloca(&hwparams);
  size = (int)frames * 4; /* 2 bytes/sample, 2 channels */
  //size =32;

  buffer_s128_1 = (char *) malloc(size);
  buffer_s128_2 = (char *) malloc(size);
  buffer_s32 = (char *) malloc(size/4);

  printf("Playback device : %s \n", device);

  /* Open PCM device for playback. */
  rc = snd_pcm_open(&handle, device,SND_PCM_STREAM_PLAYBACK, 0);
  if (rc < 0) {
    fprintf(stderr,"unable to open pcm device: %s\n", snd_strerror(rc));
    exit(1);
  }

  if ((rc = set_hwparams(handle, hwparams)) < 0) {
      printf("Setting of hwparams failed: %s\n",snd_strerror(rc));
      exit(EXIT_FAILURE);
  }

  /*end of ALSA parameters */


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
		//rc=pthread_attr_setaffinity_np(&rt_sched_attr[i],/*
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

    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <signal.h>
    #include <string.h>


    #include <alsa/asoundlib.h>

    #define NUM_THREADS 2
    #define ALSA_PCM_NEW_HW_PARAMS_API
    #define SERVER_IP "10.0.0.143"
    #define LOCAL_PORT 1234

    clockid_t my_clock;
    pthread_mutex_t attSem;

    extern int errno;
    extern void broken_pipe_handler();

    // ALSA playback parameters

    static unsigned int rate = 44100;
    unsigned int size;
    snd_pcm_uframes_t frames = 32;
    unsigned int period_time;
    char *buffer_s128_1, *buffer_s128_2, *buffer_s32;
    snd_pcm_t *handle;

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

    //TCP globals
    int client_sock;
    FILE *fp;
    struct sockaddr_in client_sockaddr;

    /*
    * SIGINT handler
    */

    void int_handler()
    {
      printf("\n Freeing buffers and closing pcm handles");
      snd_pcm_drain(handle);
      snd_pcm_close(handle);
      free(buffer_s32);
      free(buffer_s128_1);
      free(buffer_s128_2);

    	exit(0);

    }

    /*
    * clean up in case of a SIGPIPE
    */

    void broken_pipe_handler()
    {

    	printf("\nbroken pipe signal received\n");
    	cleanerFunction();
      snd_pcm_drain(handle);
      snd_pcm_close(handle);
      free(buffer_s32);
      free(buffer_s128_1);
      free(buffer_s128_2);
    	exit(0);

    }

    /*
    * Calculate Time delays
    */

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
    *  Scheduler printing
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

    /*
    * Destroy mutex and perform cleanup
    */

    void cleanerFunction()
    {
    	int rc;
    	//Clearing up the system
    	rc=pthread_mutex_destroy(&attSem);
    	if(rc<0)
    		perror("Mutex destroyed");
    	printf("\nDone\n");
    }

    /*
    * Intialize TCP client parameters
    */

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

    /*
    * set hardware parameters for ALSA playback
    */

    static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params){

      unsigned int rrate;
      int err, dir=1, rc;
      snd_pcm_uframes_t size_;
      /* Allocate hardware parameters  */

      /* Fill it in with default values. */
      err = snd_pcm_hw_params_any(handle, params);
      if (err < 0) {
        printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
        return err;
      }
      /* Set the desired hardware parameters. */

      /* Interleaved mode */
      err = snd_pcm_hw_params_set_access(handle, params,
                          SND_PCM_ACCESS_RW_INTERLEAVED);
      if (err < 0) {
        printf("Access type not available for playback: %s\n", snd_strerror(err));
        return err;
      }

      /* Signed 16-bit little-endian format */
      err = snd_pcm_hw_params_set_format(handle, params,
                                  SND_PCM_FORMAT_S16_LE);

      if (err < 0) {
       printf("Sample format not available for playback: %s\n", snd_strerror(err));
       return err;
      }

      /* Two channels (stereo) */
      err = snd_pcm_hw_params_set_channels(handle, params, 2);

      if (err < 0) {
        printf("Channels count 2 not available for playbacks: %s\n", snd_strerror(err));
        return err;
      }

      /* 44100 bits/second sampling rate (CD quality) */
      rrate = rate;
      err = snd_pcm_hw_params_set_rate_near(handle, params,
                                      &rate, &dir);
      if (err < 0) {
        printf("Rate %iHz not available for playback: %s\n", rate, snd_strerror(err));
        return err;
      }
      if (rrate != rate) {
        printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
        return -EINVAL;
      }

      /* Set period size to 32 frames. */

      err = snd_pcm_hw_params_set_period_size_near(handle,
                                  params, &frames, &dir);
      if (err < 0) {
        printf("Unable to set period size for playback: %s\n", snd_strerror(err));
        return err;
      }

      /* Write the parameters to the driver */
      rc = snd_pcm_hw_params(handle, params);
      if (rc < 0) {
        fprintf(stderr,
                "unable to set hw parameters: %s\n",
                snd_strerror(rc));
        exit(1);
      }

      /* get period time */
      snd_pcm_hw_params_get_period_time(params,
                                        &period_time, 0);
      printf("period time:%d uS\n", period_time);

      err = snd_pcm_hw_params_get_buffer_size(params, &size_);
      if (err < 0) {
              printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
              return err;
      }
      printf("buffer size:%d frames\n", size_);

      return 0;
    }

    /*
    *  Receive TCP packet and send to sound card for playback
    */

    static int playBack(snd_pcm_t *handle){
      /* TCP */

      int num_sets = size;
      int p;
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

      long loops, num;
      int rc;
      double start = 0, stop = 0;
      double sum = 0;
      double avg_exec_time, delay;

      recv(client_sock, (char *)&num_sets, sizeof(int), 0);
      recv(client_sock, (char *)buffer_s128_1, num_sets, 0);
      fprintf(stderr,"\nin playback : %d", strlen(buffer_s128_1));

      while (1) {

        rc = snd_pcm_writei(handle, buffer_s128_1, frames);

        if (rc == -EPIPE) {
          /* EPIPE means underrun */
          fprintf(stderr, "underrun occurred\n");
          snd_pcm_prepare(handle);

        } else if (rc < 0) {
          fprintf(stderr,"error from writei: %s\n", snd_strerror(rc));

        }  else if (rc != (int)frames) {
          fprintf(stderr, "short write, write %d frames\n", rc);

        }


        recv(client_sock, (char *)&num_sets, sizeof(int), 0);
        fprintf(stderr,"\nnum sets : %d", num_sets);
        recv(client_sock, (char *)buffer_s128_1, num_sets, 0);
        fprintf(stderr,"\nloop : %d", strlen(buffer_s128_1));

      }

      snd_pcm_drain(handle);
      snd_pcm_close(handle);
      free(buffer_s128_1);
      free(buffer_s128_2);
      free(buffer_s32);

    }

    /*
    * thread for playback
    */

    void *playBackthread(void *threadp)
    {

      playBack(handle);

    }

    /*
    * Main
    */

    int main (int argc, char *argv[])
    {
    	int rc;
    	int i;

      /* ALSA parameters */
      const char *device = "hw:1,0";
      snd_pcm_hw_params_t *hwparams;
      snd_pcm_hw_params_alloca(&hwparams);
      size = (int)frames * 4; /* 2 bytes/sample, 2 channels */
      //size =32;

      buffer_s128_1 = (char *) malloc(size);
      buffer_s128_2 = (char *) malloc(size);
      buffer_s32 = (char *) malloc(size/4);

      printf("Playback device : %s \n", device);

      /* Open PCM device for playback. */
      rc = snd_pcm_open(&handle, device,SND_PCM_STREAM_PLAYBACK, 0);
      if (rc < 0) {
        fprintf(stderr,"unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
      }

      if ((rc = set_hwparams(handle, hwparams)) < 0) {
          printf("Setting of hwparams failed: %s\n",snd_strerror(rc));
          exit(EXIT_FAILURE);
      }

      /*end of ALSA parameters */


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
      signal(SIGINT, int_handler);
    /*********************************************************************************/

    	//Thread creation
    	pthread_create(&threads[0],   // pointer to thread descriptor
    					  (void *)0,     // use default attributes
    					  playBackthread, // thread function entry point
    					  (void *)&(threadParams[0]) // parameters to pass in		//Cant pass nothing so just pass a number
    					 );

    	// pthread_create(&threads[1],   // pointer to thread descriptor
    	// 				  (void *)0,     // use default attributes
    	// 				 threadB, // thread function entry point
    	// 				  (void *)&(threadParams[0]) // parameters to pass in		//Cant pass nothing so just pass a number
    	// 				 );
    /*********************************************************************************/
        pthread_join(threads[0], NULL);
    	// pthread_join(threads[1], NULL);
    /*********************************************************************************/
    	cleanerFunction();
    }
 sizeof(cpu_set_t), &cpuset);

		rt_param[i].sched_priority=rt_max_prio-i-1;
		pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);

		threadParams[i].threadIdx=i;
	}
/*********************************************************************************/
   //Mutex creation
	pthread_mutex_init(&attSem,NULL);
  initializeClient();
  signal(SIGINT, int_handler);
/*********************************************************************************/

	//Thread creation
	pthread_create(&threads[0],   // pointer to thread descriptor
					  (void *)0,     // use default attributes
					  threadA, // thread function entry point
					  (void *)&(threadParams[0]) // parameters to pass in		//Cant pass nothing so just pass a number
					 );

	// pthread_create(&threads[1],   // pointer to thread descriptor
	// 				  (void *)0,     // use default attributes
	// 				 threadB, // thread function entry point
	// 				  (void *)&(threadParams[0]) // parameters to pass in		//Cant pass nothing so just pass a number
	// 				 );
/*********************************************************************************/
    pthread_join(threads[0], NULL);
	// pthread_join(threads[1], NULL);
/*********************************************************************************/
	cleanerFunction();
}
