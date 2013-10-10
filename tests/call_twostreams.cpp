/******************************************/
/*
  twostreams.cpp
  by Gary P. Scavone, 2001

  Text executable using two streams with
  callbacks.
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

typedef float  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_FLOAT32
#define SCALE  1.0
*/

typedef double  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_FLOAT64
#define SCALE  1.0

void usage(void) {
  /* Error function in case of incorrect command-line
     argument specifications
  */
  cout << "\nuseage: call_twostreams N fs\n";
  cout << "    where N = number of channels,\n";
  cout << "    and fs = the sample rate.\n\n";
  exit(0);
}

int chans;

int in(char *buffer, int buffer_size, void *data)
{
  extern int chans;
  MY_TYPE *my_buffer = (MY_TYPE *) buffer;
  MY_TYPE *my_data = (MY_TYPE *) data;
  long buffer_bytes = buffer_size * chans * sizeof(MY_TYPE);

  memcpy(my_data, my_buffer, buffer_bytes);

  return 0;
}

int out(char *buffer, int buffer_size, void *data)
{
  extern int chans;
  MY_TYPE *my_buffer = (MY_TYPE *) buffer;
  MY_TYPE *my_data = (MY_TYPE *) data;
  long buffer_bytes = buffer_size * chans * sizeof(MY_TYPE);

  memcpy(my_buffer, my_data, buffer_bytes);

  return 0;
}

int main(int argc, char *argv[])
{
  int device, buffer_size, stream1, stream2, fs;
  MY_TYPE *data = 0;
  RtAudio *audio;
  char input;

  // minimal command-line checking
  if (argc != 3) usage();

  chans = (int) atoi(argv[1]);
  fs = (int) atoi(argv[2]);

  // Open the realtime output device
  buffer_size = 512;
  device = 0; // default device
  try {
    audio = new RtAudio();
  }
  catch (RtError &) {
    exit(EXIT_FAILURE);
  }

  try {
    stream1 = audio->openStream(0, 0, device, chans,
                                FORMAT, fs, &buffer_size, 8);
    stream2 = audio->openStream(device, chans, 0, 0,
                                FORMAT, fs, &buffer_size, 8);
  }
  catch (RtError &) {
    goto cleanup;
  }

  data = (MY_TYPE *) calloc(chans*buffer_size, sizeof(MY_TYPE));
  try {
    audio->setStreamCallback(stream1, &in, (void *)data);
    audio->setStreamCallback(stream2, &out, (void *)data);
    audio->startStream(stream1);
    audio->startStream(stream2);
  }
  catch (RtError &) {
    goto cleanup;
  }

  cout << "\nRunning two streams (quasi-duplex) ... press <enter> to quit.\n";
  cin.get(input);

  cout << "\nStopping both streams.\n";
  try {
    audio->stopStream(stream1);
    audio->stopStream(stream2);
  }
  catch (RtError &) {
    goto cleanup;
  }

  cout << "\nPress <enter> to restart streams:\n";
  cin.get(input);

  try {
    audio->startStream(stream1);
    audio->startStream(stream2);
  }
  catch (RtError &) {
    goto cleanup;
  }

  cout << "\nRunning two streams (quasi-duplex) ... press <enter> to quit.\n";
  cin.get(input);

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
