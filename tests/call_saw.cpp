/******************************************/
/*
  call_saw.c
  by Gary P. Scavone, 2001

  Play sawtooth waveforms of distinct frequency.
  Takes number of channels and sample rate as
  input arguments.  Use callback functionality.
*/
/******************************************/

#include "RtAudio.h"
#include <iostream.h>

/*
typedef signed long  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_SINT24
#define SCALE  2147483647.0

typedef char  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_SINT8
#define SCALE  127.0

typedef signed short  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_SINT16
#define SCALE  32767.0

typedef signed long  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_SINT32
#define SCALE  2147483647.0
*/
typedef float  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_FLOAT32
#define SCALE  1.0

/*
typedef double  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_FLOAT64
#define SCALE  1.0
*/

#define BASE_RATE 0.005
#define TIME   1.0

void usage(void) {
  /* Error function in case of incorrect command-line
     argument specifications
  */
  cout << "\nuseage: call_saw N fs\n";
  cout << "    where N = number of channels,\n";
  cout << "    and fs = the sample rate.\n\n";
  exit(0);
}

int chans;

int saw(char *buffer, int buffer_size, void *data)
{
  int i, j;
  extern int chans;
  MY_TYPE *my_buffer = (MY_TYPE *) buffer;
  double *my_data = (double *) data;

  for (i=0; i<buffer_size; i++) {
    for (j=0; j<chans; j++) {
      *my_buffer++ = (MY_TYPE) (my_data[j] * SCALE);
      my_data[j] += BASE_RATE * (j+1+(j*0.1));
      if (my_data[j] >= 1.0) my_data[j] -= 2.0;
    }
  }

  return 0;
}

int main(int argc, char *argv[])
{
  int device, stream, buffer_size, fs;
  RtAudio *audio;
  double *data;
  char input;

  // minimal command-line checking
  if (argc != 3) usage();

  chans = (int) atoi(argv[1]);
  fs = (int) atoi(argv[2]);

  // Open the realtime output device
  buffer_size = 256;
  device = 0; // default device
  try {
    audio = new RtAudio(&stream, device, chans, 0, 0,
                        FORMAT, fs, &buffer_size, 4);
  }
  catch (RtError &) {
    exit(EXIT_FAILURE);
  }

  data = (double *) calloc(chans, sizeof(double));

  try {
    audio->setStreamCallback(stream, &saw, (void *)data);
    audio->startStream(stream);
  }
  catch (RtError &) {
    goto cleanup;
  }

  cout << "\nPlaying ... press <enter> to quit.\n";
  cin.get(input);

  // Stop the stream.
  try {
    audio->stopStream(stream);
  }
  catch (RtError &) {
  }

 cleanup:
  audio->closeStream(stream);
  delete audio;
  if (data) free(data);

  return 0;
}
