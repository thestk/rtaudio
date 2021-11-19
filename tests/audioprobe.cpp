/******************************************/
/*
  audioprobe.cpp
  by Gary P. Scavone, 2001

  Probe audio system and prints device info.
*/
/******************************************/

#include "RtAudio.h"
#include <iostream>
#include <map>

std::vector< RtAudio::Api > listApis()
{
  std::vector< RtAudio::Api > apis;
  RtAudio :: getCompiledApi( apis );

  std::cout << "\nCompiled APIs:\n";
  for ( size_t i=0; i<apis.size(); i++ )
    std::cout << i << ". " << RtAudio::getApiDisplayName(apis[i])
              << " (" << RtAudio::getApiName(apis[i]) << ")" << std::endl;

  return apis;
}

void listDevices(RtAudio::Api api)
{
  RtAudio audio(api);
  RtAudio::DeviceInfo info;

  std::cout << "\nAPI: " << RtAudio::getApiDisplayName(audio.getCurrentApi()) << std::endl;

  unsigned int devices = audio.getDeviceCount();
  std::cout << "\nFound " << devices << " device(s) ...\n";

  for (unsigned int i=0; i<devices; i++) {
    info = audio.getDeviceInfo(i);

    std::cout << "\nDevice Name = " << info.name << '\n';
    std::cout << "Device ID = " << i << '\n';
    if ( info.probed == false )
      std::cout << "Probe Status = UNsuccessful\n";
    else {
      std::cout << "Probe Status = Successful\n";
      std::cout << "Output Channels = " << info.outputChannels << '\n';
      std::cout << "Input Channels = " << info.inputChannels << '\n';
      std::cout << "Duplex Channels = " << info.duplexChannels << '\n';
      if ( info.isDefaultOutput ) std::cout << "This is the default output device.\n";
      else std::cout << "This is NOT the default output device.\n";
      if ( info.isDefaultInput ) std::cout << "This is the default input device.\n";
      else std::cout << "This is NOT the default input device.\n";
      if ( info.nativeFormats == 0 )
        std::cout << "No natively supported data formats(?)!";
      else {
        std::cout << "Natively supported data formats:\n";
        if ( info.nativeFormats & RTAUDIO_SINT8 )
          std::cout << "  8-bit int\n";
        if ( info.nativeFormats & RTAUDIO_SINT16 )
          std::cout << "  16-bit int\n";
        if ( info.nativeFormats & RTAUDIO_SINT24 )
          std::cout << "  24-bit int\n";
        if ( info.nativeFormats & RTAUDIO_SINT32 )
          std::cout << "  32-bit int\n";
        if ( info.nativeFormats & RTAUDIO_FLOAT32 )
          std::cout << "  32-bit float\n";
        if ( info.nativeFormats & RTAUDIO_FLOAT64 )
          std::cout << "  64-bit float\n";
      }
      if ( info.sampleRates.size() < 1 )
        std::cout << "No supported sample rates found!";
      else {
        std::cout << "Supported sample rates = ";
        for (unsigned int j=0; j<info.sampleRates.size(); j++)
          std::cout << info.sampleRates[j] << " ";
      }
      std::cout << std::endl;
      if ( info.preferredSampleRate == 0 )
        std::cout << "No preferred sample rate found!" << std::endl;
      else
        std::cout << "Preferred sample rate = " << info.preferredSampleRate << std::endl;
    }
  }
}

int main(int argc, char *argv[])
{
  std::cout << "\nRtAudio Version " << RtAudio::getVersion() << std::endl;

  std::vector< RtAudio::Api > apis = listApis();

  for ( size_t api=0; api < apis.size(); api++ )
  {
    errno = 0;
    char *s;
    if (argc < 2
        || apis[api] == RtAudio::getCompiledApiByName(argv[1])
        || (api == std::strtoul(argv[1], &s, 10) && argv[1] != s && !errno))
      listDevices(apis[api]);
  }
  std::cout << std::endl;

  return 0;
}
