/************************************************************************/
/*! \brief Interactively Test RtAudio parameters.

    RtAudio is a command-line utility that allows users to enumerate 
    installed devices, and to test input, output and duplex operation 
    of RtAudio devices with various buffer and buffer-size 
    configurations.

    Copyright (c) 2005 Robin Davies.

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation files
    (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge,
    publish, distribute, sublicense, and/or sell copies of the Software,
    and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/************************************************************************/

#include "RtAudio.h"
#include "FileWvOut.h"

#include <cmath>
#include <iostream>
#include <stdopt.h>

using namespace std;
using namespace stdopt;

#ifdef _WIN32
// Correct windows.h standards violation.
#undef min
#undef max
#endif

#define DSOUND 1

RtAudio::RtAudioApi rtApi = RtAudio::WINDOWS_DS;

void DisplayHelp(std::ostream &os)
{
  os 
    << "rtaudiotest - Test rtaudio devices." << endl
    << endl
    << "Syntax:" << endl
    << "   rtaudiotest [options]* enum" << endl
    << "                              - Display installed devices." << endl
    << "   rtaudiotest [options]* inputtest <devicenum> [<filename>]" << endl
    << "                              - Capture audio to a .wav file." << endl
    << "   rtaudiotest [options]* outputtest <devicenum>" << endl
    << "                              - Generate a test signal on the device.." << endl
    << "   rtaudiotest [options]* duplextest <inputDevicenum> <outputdevicenum>" << endl
    << "                              - Echo input to output." << endl

    << "Options:" << endl
    << "   -h -?        Display this message." << endl
    << "   -dsound      Use DirectX drivers." << endl
    << "   -asio        Use ASIO drivers." << endl
    << "   -buffers N   Use N buffers." << endl
    << "   -size N      Use buffers of size N." << endl
    << "   -srate N     Use a sample-rate of N (defaults to 44100)." << endl
    << "   -channels N  Use N channels (defaults to 2)." << endl
    << "   -seconds N   Run the test for N seconds (default 5)." << endl
    << "Description: " << endl
    << "  RtAudio is a command-line utility that allows users to enumerate " << endl
    << "  installed devices, and to test input, output and duplex operation " << endl
    << "  of RtAudio devices with various buffer and buffer-size " << endl
    << "  configurations." << endl
    << "Examples:"  << endl
    << "      rtaudio -asio enum" << endl
    << "      rtaudio -dsound -buffers 4 -size 128 -seconds 3 inputtest 0 test.wav" << endl
    ;

}

void EnumerateDevices(RtAudio::RtAudioApi api)
{
  RtAudio rt(api);
  for (int i = 1; i <= rt.getDeviceCount(); ++i)
  {
    RtAudioDeviceInfo info = rt.getDeviceInfo(i);
    cout << "Device " << i << ": " << info.name << endl;
  }
}

struct TestConfiguration 
{
  long srate;
  int channels;
  int bufferSize;
  int buffers;
  int seconds;
};


bool DisplayStats(RtAudio::RtAudioApi api)
{
#ifdef __WINDOWS_DS__
  // Display latency results for Windows DSound drivers.
  if (api == RtAudio::WINDOWS_DS)
  {
    RtApiDs::RtDsStatistics s = RtApiDs::getDsStatistics();

    cout << "   Latency: " << s.latency*1000.0 << "ms" << endl;
    if (s.inputFrameSize)
    {
      cout << "   Read overruns: " << s.numberOfReadOverruns << endl;
    }
    if (s.outputFrameSize)
    {
      cout << "   Write underruns: " << s.numberOfWriteUnderruns << endl;
    }

    if (s.inputFrameSize)
    {
      cout << "   Read lead time in sample frames (device): " << s.readDeviceSafeLeadBytes/ s.inputFrameSize << endl;
    }
    if (s.outputFrameSize)
    {
      cout << "   Write lead time in sample frames (device): " << s.writeDeviceSafeLeadBytes / s.outputFrameSize << endl;
      cout << "   Write lead time in sample frames (buffer): " << s.writeDeviceBufferLeadBytes / s.outputFrameSize << endl;

    }

  }
#endif
  return true;
}

void InputTest( RtAudio::RtAudioApi api, 
                int inputDevice,
                const std::string &fileName,
                TestConfiguration &configuration )
{
  RtAudio rt(api);

  int bufferSize = configuration.bufferSize;

  RtAudioDeviceInfo info = rt.getDeviceInfo(inputDevice);
  cout << "Reading from device " << inputDevice << " (" << info.name << ")\n";

  rt.openStream(0,0,inputDevice,configuration.channels, RTAUDIO_SINT16, configuration.srate,&bufferSize,configuration.buffers);
  if (bufferSize != configuration.bufferSize)
    {
      cout << "The buffer size was changed to " << bufferSize << " by the device." << endl;
      configuration.bufferSize = bufferSize;

    }

  int nTicks = (int)ceil((configuration.srate* configuration.seconds)*1.0/configuration.bufferSize);

  if (fileName.length() == 0) 
    {
      // just run the stream.
      rt.startStream();
      for (int i = 0; i < nTicks; ++i)
        {
          rt.tickStream();
        }
      rt.stopStream();
    } else 
    {
      if (configuration.seconds > 10) {
        throw CommandLineException("Capture of more than 10 seconds of data is not supported.");
      }
      std::vector<short> data;
      // we could be smarter, but the point here is to capture data without interfering with the stream.
      // File writes while ticking the stream is not cool. 
      data.resize(nTicks*configuration.bufferSize*configuration.channels); // potentially very big. That's why we restrict capture to 10 seconds.
      short *pData = &data[0];

      rt.startStream();
      int i;
      for (i = 0; i < nTicks; ++i)
        {
          rt.tickStream();
          short *streamBuffer = (short*)rt.getStreamBuffer();
          for (int samples = 0; samples < configuration.bufferSize; ++samples)
            {
              for (int channel = 0; channel < configuration.channels; ++channel)
                {
                  *pData ++ = *streamBuffer++;
                }
            }
        }
      rt.stopStream();
      remove(fileName.c_str());
      FileWvOut wvOut;
      wvOut.openFile( fileName.c_str(), configuration.channels, FileWrite::FILE_WAV );

      StkFrames frame(1,configuration.channels,false);
      pData = &data[0];

      for (i = 0; i < nTicks; ++i) {
        for (int samples = 0; samples < configuration.bufferSize; ++samples) {
          for (int channel = 0; channel < configuration.channels; ++channel) {
            frame[channel] = (float)( *pData++*( 1.0 / 32768.0 ) );
          }
          wvOut.tickFrame(frame);
        }
      }
      wvOut.closeFile();
    }
  rt.closeStream();

  if (DisplayStats(api)) {
    cout << "Test succeeded." << endl;
  }
}

void OutputTest( RtAudio::RtAudioApi api, 
                 int outputDevice,
                 TestConfiguration &configuration )
{
  RtAudio rt(api);
  int bufferSize = configuration.bufferSize;

  RtAudioDeviceInfo info = rt.getDeviceInfo(outputDevice);
  cout << "Writing to " << info.name << "...\n";

  rt.openStream(outputDevice,configuration.channels, 0,0, RTAUDIO_SINT16, configuration.srate,&bufferSize,configuration.buffers);
  if (bufferSize != configuration.bufferSize) {
    cout << "The buffer size was changed to " << bufferSize << " by the device." << endl;
    configuration.bufferSize = bufferSize;
  }

  rt.startStream();
  short *pBuffer = (short*)rt.getStreamBuffer();
  int nTicks = (int)ceil((configuration.srate* configuration.seconds)*1.0/configuration.bufferSize);

  double phase = 0;
  double deltaPhase = 880.0/configuration.srate;
  for (int i = 0; i < nTicks; ++i) {
    short *p = pBuffer;
    for (int samp = 0; samp < configuration.bufferSize; ++samp) {
      short val = (short)(sin(phase)*(32768/4)); // sin()*0.25 magnitude. Audible, but not damaging to ears or speakers.
      phase += deltaPhase;

      for (int chan = 0; chan < configuration.channels; ++chan) {
        *p++ = val;
      }
    }
    rt.tickStream();
  }
  rt.stopStream();
  rt.closeStream();

  if ( DisplayStats(api) ) {
    cout << "Test succeeded." << endl;
  }
}

void DuplexTest( RtAudio::RtAudioApi api, 
                 int inputDevice,
                 int outputDevice,
                 TestConfiguration &configuration )
{
  RtAudio rt(api);
  int bufferSize = configuration.bufferSize;

  RtAudioDeviceInfo info = rt.getDeviceInfo(inputDevice);
  cout << "Reading from  " << info.name << ", " << endl;
  info = rt.getDeviceInfo(outputDevice);
  cout << "Writing to  " << info.name << "..." << endl;

  rt.openStream(outputDevice,configuration.channels, inputDevice,configuration.channels, RTAUDIO_SINT16, configuration.srate,&bufferSize,configuration.buffers);
  if (bufferSize != configuration.bufferSize)
    {
      cout << "The buffer size was changed to " << bufferSize << " by the device." << endl;
      configuration.bufferSize = bufferSize;
    }

  rt.startStream();
  short *pBuffer = (short*)rt.getStreamBuffer();
  int nTicks = (int)ceil((configuration.srate* configuration.seconds)*1.0/configuration.bufferSize);

  for (int i = 0; i < nTicks; ++i) {
    rt.tickStream();
  }
  rt.stopStream();
  rt.closeStream();

  if ( DisplayStats(api) ) {
    cout << "Test succeeded." << endl;
  }
}

int main(int argc, char **argv)
{
  try
    {
      CommandLine commandLine;

      TestConfiguration configuration;
      bool displayHelp;
      bool useDsound;
      bool useAsio;

      commandLine.AddOption("h",&displayHelp);
      commandLine.AddOption("?",&displayHelp);
      commandLine.AddOption("dsound",&useDsound);
      commandLine.AddOption("asio",&useAsio);
      commandLine.AddOption("srate",&configuration.srate,44100L);
      commandLine.AddOption("channels",&configuration.channels,2);
      commandLine.AddOption("seconds",&configuration.seconds,5);
      commandLine.AddOption("buffers",&configuration.buffers,2);
      commandLine.AddOption("size",&configuration.bufferSize,128);

      commandLine.ProcessCommandLine(argc,argv);

      if (displayHelp || commandLine.GetArguments().size() == 0)
        {
          DisplayHelp(cout);
          return 0;
        }
      if (useDsound) 
        {
          rtApi = RtAudio::WINDOWS_DS;
        } else if (useAsio)
        {
          rtApi = RtAudio::WINDOWS_ASIO;
        } else {
        throw CommandLineException("Please specify an API to use: '-dsound', or '-asio'");
      }

      std::string testName;
      commandLine.GetArgument(0,&testName);
      if (testName == "enum")
        {
          EnumerateDevices(rtApi);
        } else if (testName == "inputtest")
        {
          int inputDevice;
          std::string fileName;
          commandLine.GetArgument(1,&inputDevice);
          if (commandLine.GetArguments().size() >= 2)
            {
              commandLine.GetArgument(2,&fileName);
            }
          InputTest(rtApi,inputDevice,fileName,configuration);
        } else if (testName == "outputtest")
        {
          int inputDevice;
          commandLine.GetArgument(1,&inputDevice);
          OutputTest(rtApi,inputDevice,configuration);
        } else if (testName == "duplextest")
        {
          int inputDevice;
          int outputDevice;
          commandLine.GetArgument(1,&inputDevice);
          commandLine.GetArgument(2,&outputDevice);
          DuplexTest(rtApi,inputDevice,outputDevice,configuration);
        } else {
        throw CommandLineException("Not a valid test name.");
      }

    } catch (CommandLineException &e)
    {
      cerr << e.what() << endl << endl;
      cerr << "Run 'rtaudiotest -h' to see the commandline syntax." << endl;
      return 3;
    } catch (RtError &e)
    {
      cerr << e.getMessage() << endl;
      return 3;

    } catch (std::exception &e)
    {
      cerr << "Error: " << e.what() << endl;
      return 3;

    }
  return 0;
}
