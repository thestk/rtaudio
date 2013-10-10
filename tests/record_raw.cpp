/******************************************/
/*
  record_raw.c
  by Gary P. Scavone, 2001

  Records from default input.  Takes
  number of channels and sample rate
  as input arguments. Uses blocking calls.
*/
/******************************************/

#include "RtAudio.h"
#include <iostream.h>
#include <stdio.h>

/*
typedef char  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_SINT8

typedef signed short  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_SINT16

typedef signed long  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_SINT24

typedef signed long  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_SINT32
*/

typedef float  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_FLOAT32

/*
typedef double  MY_TYPE;
#define FORMAT RtAudio::RTAUDIO_FLOAT64
*/

#define TIME   2.0

void usage(void) {
  /* Error function in case of incorrect command-line
     argument specifications
  */
  cout << "\nuseage: record_raw N fs <device>\n";
  cout << "    where N = number of channels,\n";
  cout << "    fs = the sample rate,\n";
  cout << "    and device = the device to use (default = 0).\n\n";
  exit(0);
}

int main(int argc, char *argv[])
{
  int chans, fs, buffer_size, stream, device = 0;
  long frames, counter = 0;
  MY_TYPE *buffer;
  FILE *fd;
  RtAudio *audio;

  // minimal command-line checking
  if (argc != 3 && argc != 4 ) usage();

  chans = (int) atoi(argv[1]);
  fs = (int) atoi(argv[2]);
  if ( argc == 4 )
    device = (int) atoi(argv[3]);

  // Open the realtime output device
  buffer_size = 512;
  try {
    audio = new RtAudio(&stream, 0, 0, device, chans,
                        FORMAT, fs, &buffer_size, 8);
  }
  catch (RtError &) {
    exit(EXIT_FAILURE);
  }

  fd = fopen("test.raw","wb");
  frames = (long) (fs * TIME);

  try {
    buffer = (MY_TYPE *) audio->getStreamBuffer(stream);
    audio->startStream(stream);
  }
  catch (RtError &) {
    goto cleanup;
  }

  cout << "\nRecording for " << TIME << " seconds ... writing file test.raw (buffer size = " << buffer_size << ")." << endl;
  while (counter < frames) {

    try {
      audio->tickStream(stream);
    }
    catch (RtError &) {
      goto cleanup;
    }

    fwrite(buffer, sizeof(MY_TYPE), chans * buffer_size, fd);
    counter += buffer_size;
  }

  try {
    audio->stopStream(stream);
  }
  catch (RtError &) {
  }

 cleanup:
  audio->closeStream(stream);
  delete audio;
  fclose(fd);

  return 0;
}
