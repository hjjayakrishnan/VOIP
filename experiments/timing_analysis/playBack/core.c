#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>
#include <sys/time.h>

static unsigned int rate = 44100;
unsigned int size;
snd_pcm_uframes_t frames = 32;
unsigned int period_time;
char *buffer;


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


static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params){

  unsigned int rrate;
  int err, dir=1, rc;

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

  snd_pcm_hw_params_set_period_size_near(handle,
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

  return 0;
}



static int playBack(snd_pcm_t *handle){

  printf("in playback\n");

  long loops, num;
  int rc;
  double start = 0, stop = 0;
  double sum = 0;
  double avg_exec_time, delay;

  /* 5 seconds in microseconds divided by
   * period time */

  loops = 5000000 / period_time;
  num = loops;
  while (loops > 0) {
    loops--;
    rc = read(0, buffer, size);
    if (rc == 0) {
      fprintf(stderr, "end of file on input\n");
      break;

    } else if (rc != size) {
      fprintf(stderr, "short read: read %d bytes\n", rc);
    }

    start = readTOD();
    rc = snd_pcm_writei(handle, buffer, frames);
    stop = readTOD();
    delay = (double)(stop - start);
    sum += delay;

    if (rc == -EPIPE) {
      /* EPIPE means underrun */
      fprintf(stderr, "underrun occurred\n");
      snd_pcm_prepare(handle);

    } else if (rc < 0) {
      fprintf(stderr,"error from writei: %s\n", snd_strerror(rc));

    }  else if (rc != (int)frames) {
      fprintf(stderr, "short write, write %d frames\n", rc);

    }
  }
  printf("Average execution time :% lf uS\n",(sum/num)*1000000);
  snd_pcm_drain(handle);
  snd_pcm_close(handle);
  free(buffer);

}


int main(){

  int rc;
  const char *device = "hw:1,0";
  snd_pcm_t *handle;
  snd_pcm_hw_params_t *hwparams;
  snd_pcm_hw_params_alloca(&hwparams);
  size = (int)frames * 4; /* 2 bytes/sample, 2 channels */
  buffer = (char *) malloc(size);

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

  playBack(handle);

  return 0;
}
