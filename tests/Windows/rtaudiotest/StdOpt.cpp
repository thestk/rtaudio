/************************************************************************/
/*! \class CommandLine
    \brief Command-line option parser.

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

#include ".\stdopt.h"

using namespace stdopt;

CommandLine::CommandLine()
{
}

void CommandLine::ProcessCommandLine(int argc, char**argv)
{
	std::vector<std::string> cmdline;
	for (int i = 0; i < argc; ++i) {
		cmdline.push_back(argv[i]);
	}
	ProcessCommandLine(cmdline);
}

const CommandLine::COptionHandlerBase*CommandLine::GetOptionHandler(const std::string &name) const
{
	// Return excact matches only.
	for (size_t i = 0; i < optionHandlers.size(); ++i)
	{
		if (optionHandlers[i]->getName() == name) {
			return (optionHandlers[i]);
		}
	}
	return NULL;
}

void CommandLine::ProcessCommandLine(const std::vector<std::string>& cmdline)
{
	for (size_t i = 1; i < cmdline.size(); ++i)
    {
      if (cmdline[i].length() != 0 && cmdline[i][0] == L'/' || (cmdline[i][0] == '-')) {
        std::string arg = cmdline[i].substr(1);
        const COptionHandlerBase *pHandler = GetOptionHandler(arg);
        if (pHandler == NULL) {
          throw CommandLineException(std::string("Unknown option: ") + arg);
        }
        if (pHandler->HasArgument())
          {
            std::string strArg;
            if (i+1 < cmdline.size()) {
              ++i;
              strArg = cmdline[i];
            }
            pHandler->Process(strArg.c_str());
          } else {
          pHandler->Process(NULL);
        }
      } else {
        args.push_back(cmdline[i]);
      }
    }
}

CommandLine::~CommandLine(void)
{
	for (size_t i = 0; i < optionHandlers.size(); ++i)
	{
		delete optionHandlers[i];
	}
	optionHandlers.resize(0);
}
