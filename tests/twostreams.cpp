/******************************************/
/*
  twostreams.cpp
  by Gary P. Scavone, 2001

  Test executable for audio playback,
  recording, duplex operation, stopping,
  starting, and aborting operations.
  Takes number of channels and sample
  rate as input arguments.  Runs input
  and output through two separate streams.
  Uses blocking functionality.
*/
/******************************************/

#include "RtAudio.h"
#include <iostream.h>
#include <stdio.h>

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
#define TIME 2.0

void usage(void) {
  /* Error function in case of incorrect command-line
     argument specifications
  */
  cout << "\nuseage: twostreams N fs <device>\n";
  cout << "    where N = number of channels,\n";
  cout << "    fs = the sample rate,\n";
  cout << "    and device = the device to use (default = 0).\n\n";
  exit(0);
}

int main(int argc, char *argv[])
{
  int chans, fs, buffer_size, stream1 = 0, stream2 = 0, device = 0;
  long frames, counter = 0, i, j;
  MY_TYPE *buffer1, *buffer2;
  RtAudio *audio;
  FILE *fd;
  double *data = 0;

  // minimal command-line checking
  if (argc != 3 && argc != 4 ) usage();

  chans = (int) atoi(argv[1]);
  fs = (int) atoi(argv[2]);
  if ( argc == 4 )
    device = (int) atoi(argv[3]);

  // Open the realtime output device
  buffer_size = 512;
  try {
    audio = new RtAudio();
  }
  catch (RtError &) {
    exit(EXIT_FAILURE);
  }

  try {
    stream1 = audio->openStream(device, chans, 0, 0,
                                FORMAT, fs, &buffer_size, 8);
    stream2 = audio->openStream(0, 0, device, chans,
                                FORMAT, fs, &buffer_size, 8);
  }
  catch (RtError &) {
    goto cleanup;
  }

  try {
    buffer1 = (MY_TYPE *) audio->getStreamBuffer(stream1);
    buffer2 = (MY_TYPE *) audio->getStreamBuffer(stream2);
  }
  catch (RtError &) {
    goto cleanup;
  }

  frames = (long) (fs * TIME);
  data = (double *) calloc(chans, sizeof(double));

  try {
    audio->startStream(stream1);
  }
  catch (RtError &) {
    goto cleanup;
  }

  cout << "\nStarting sawtooth playback stream for " << TIME << " seconds." << endl;
  while (counter < frames) {
    for (i=0; i<buffer_size; i++) {
      for (j=0; j<chans; j++) {
        buffer1[i*chans+j] = (MY_TYPE) (data[j] * SCALE);
        data[j] += BASE_RATE * (j+1+(j*0.1));
        if (data[j] >= 1.0) data[j] -= 2.0;
      }
    }

    try {
      audio->tickStream(stream1);
    }
    catch (RtError &) {
      goto cleanup;
    }

    counter += buffer_size;
  }

  cout << "\nStopping playback stream." << endl;
  try {
    audio->stopStream(stream1);
  }
  catch (RtError &) {
    goto cleanup;
  }

  fd = fopen("test.raw","wb");

  try {
    audio->startStream(stream2);
  }
  catch (RtError &) {
    goto cleanup;
  }

  counter = 0;
  cout << "\nStarting recording stream for " << TIME << " seconds." << endl;
  while (counter < frames) {

    try {
      audio->tickStream(stream2);
    }
    catch (RtError &) {
      goto cleanup;
    }

    fwrite(buffer2, sizeof(MY_TYPE), chans * buffer_size, fd);
    counter += buffer_size;
  }

  fclose(fd);
  cout << "\nAborting recording." << endl;

  try {
    audio->abortStream(stream2);
    audio->startStream(stream1);
    audio->startStream(stream2);
  }
  catch (RtError &) {
    goto cleanup;
  }

  counter = 0;
  cout << "\nStarting playback and record streams (quasi-duplex) for " << TIME << " seconds." << endl;
  while (counter < frames) {

    try {
      audio->tickStream(stream2);
      memcpy(buffer1, buffer2, sizeof(MY_TYPE) * chans * buffer_size);
      audio->tickStream(stream1);
    }
    catch (RtError &) {
      goto cleanup;
    }

    counter += buffer_size;
  }

  cout << "\nStopping both streams." << endl;
  try {
    audio->stopStream(stream1);
    audio->stopStream(stream2);
  }
  catch (RtError &) {
  }

 cleanup:
  audio->closeStream(stream1);
  audio->closeStream(stream2);
  delete audio;
  if (data) free(data);

  return 0;
}
