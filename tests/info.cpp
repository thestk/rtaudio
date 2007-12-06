/******************************************/
/*
  info.cpp
  by Gary P. Scavone, 2001

  Prints audio system/device info.
*/
/******************************************/

#include "RtAudio.h"
#include <iostream.h>

int main(int argc, char *argv[])
{
  RtAudio *audio;
  RtAudio::RTAUDIO_DEVICE my_info;
  try {
    audio = new RtAudio();
  }
  catch (RtAudioError &m) {
    m.printMessage();
    exit(EXIT_FAILURE);
  }

  int devices = audio->getDeviceCount();
  cout << "\nFound " << devices << " devices ...\n";

  for (int i=0; i<devices; i++) {
    try {
      audio->getDeviceInfo(i, &my_info);
    }
    catch (RtAudioError &m) {
      m.printMessage();
      break;
    }

    cout << "\nname = " << my_info.name << '\n';
    if (my_info.probed == true)
      cout << "probe successful\n";
    else
      cout << "probe unsuccessful\n";
    cout << "maxOutputChans = " << my_info.maxOutputChannels << '\n';
    cout << "minOutputChans = " << my_info.minOutputChannels << '\n';
    cout << "maxInputChans = " << my_info.maxInputChannels << '\n';
    cout << "minInputChans = " << my_info.minInputChannels << '\n';
    cout << "maxDuplexChans = " << my_info.maxDuplexChannels << '\n';
    cout << "minDuplexChans = " << my_info.minDuplexChannels << '\n';
    if (my_info.hasDuplexSupport)
      cout << "duplex support = true\n";
    else
      cout << "duplex support = false\n";
    cout << "format = " << my_info.nativeFormats << '\n';
    if (my_info.nSampleRates == -1)
      cout << "min_srate = " << my_info.sampleRates[0] << ", max_srate = " << my_info.sampleRates[1] << '\n';
    else {
      cout << "sample rates = ";
      for (int j=0; j<my_info.nSampleRates; j++)
        cout << my_info.sampleRates[j] << " ";
      cout << endl;
    }
  }
  cout << endl;

  delete audio;
  return 0;
}
