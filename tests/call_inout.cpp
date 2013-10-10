/******************************************/
/*
  call_inout.c
  by Gary P. Scavone, 2001

  Records from default input and passes it
  through to the output.  Takes number of
  channels and sample rate as input arguments.
  Uses callback functionality.
*/
/******************************************/

#include "RtAudio.h"
#include <iostream.h>

/*
typedef signed long  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_SINT24

typedef char  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_SINT8

typedef signed short  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_SINT16

typedef signed long  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_SINT32

typedef float  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_FLOAT32
*/

typedef double  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_FLOAT64

void usage(void) {
  /* Error function in case of incorrect command-line
     argument specifications
  */
  cout << "\nuseage: call_inout N fs\n";
  cout << "    where N = number of channels,\n";
  cout << "    and fs = the sample rate.\n\n";
  exit(0);
}

int inout(char *buffer, int buffer_size, void *)
{
  // Surprise!!  We do nothing to pass the data through.
  return 0;
}

int main(int argc, char *argv[])
{
  int device, stream, chans, fs;
  RtAudio *audio;
  char input;

  // minimal command-line checking
  if (argc != 3) usage();

  chans = (int) atoi(argv[1]);
  fs = (int) atoi(argv[2]);

  // Open the realtime output device
  int buffer_size = 512;
  device = 0; // default device
  try {
    audio = new RtAudio(&stream, device, chans, device, chans,
                        FORMAT, fs, &buffer_size, 8);
  }
  catch (RtError &) {
    exit(EXIT_FAILURE);
  }

  try {
    audio->setStreamCallback(stream, &inout, NULL);
    audio->startStream(stream);
  }
  catch (RtError &) {
    goto cleanup;
  }

  cout << "\nRunning ... press <enter> to quit.\n";
  cin.get(input);

  try {
    audio->stopStream(stream);
  }
  catch (RtError &) {
  }

 cleanup:
  audio->closeStream(stream);
  delete audio;

  return 0;
}
