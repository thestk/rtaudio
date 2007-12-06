/******************************************/
/*
  play_saw.c
  by Gary P. Scavone, 2001

  Play sawtooth waveforms of distinct frequency.
  Takes number of channels and sample rate as
  input arguments.  Uses blocking functionality.
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
  cout << "\nuseage: play_saw N fs\n";
  cout << "    where N = number of channels,\n";
  cout << "    and fs = the sample rate.\n\n";
  exit(0);
}

int main(int argc, char *argv[])
{
  int chans, fs, device, buffer_size, stream;
  long frames, counter = 0, i, j;
  MY_TYPE *buffer;
  RtAudio *audio;
  double *data;

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
  catch (RtAudioError &m) {
    m.printMessage();
    exit(EXIT_FAILURE);
  }

  frames = (long) (fs * TIME);
  data = (double *) calloc(chans, sizeof(double));

  try {
    buffer = (MY_TYPE *) audio->getStreamBuffer(stream);
    audio->startStream(stream);
  }
  catch (RtAudioError &m) {
    m.printMessage();
    goto cleanup;
  }

  cout << "\nPlaying for " << TIME << " seconds." << endl;
  while (counter < frames) {
    for (i=0; i<buffer_size; i++) {
      for (j=0; j<chans; j++) {
        buffer[i*chans+j] = (MY_TYPE) (data[j] * SCALE);
        data[j] += BASE_RATE * (j+1+(j*0.1));
        if (data[j] >= 1.0) data[j] -= 2.0;
      }
    }

    try {
      //cout << "frames until no block = " << audio->streamWillBlock(stream) << endl;
      audio->tickStream(stream);
    }
    catch (RtAudioError &m) {
      m.printMessage();
      goto cleanup;
    }

    counter += buffer_size;
  }

  try {
    audio->stopStream(stream);
  }
  catch (RtAudioError &m) {
    m.printMessage();
  }

 cleanup:
  audio->closeStream(stream);
  delete audio;
  if (data) free(data);

  return 0;
}
