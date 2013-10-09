/************************************************************************/
/*! \class CommandLine
    \brief Command-line opition parser.

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

#ifndef STDOPT_H
#define STDOPT_H

#include <vector>
#include <string>
#include <sstream>
#include <exception>

namespace stdopt
{

	class CommandLineException: public std::exception {
	public: 
		CommandLineException(const std::string &error)
		{
			s = error;
		}
		const char*what() { return s.c_str(); }
	private:
		std::string s;
	};

	class CommandLine
	{
	public:
		CommandLine();
		virtual ~CommandLine();

		void ProcessCommandLine(int argc, char**argv);
		void ProcessCommandLine(const std::vector<std::string>& cmdline);

		template <class TVAL> void AddOption(const char*name,TVAL*pResult, TVAL defaultValue)
		{
			this->optionHandlers.push_back(new COptionHandler<TVAL>(name,pResult));
            *pResult = defaultValue;

		}
		template <class TVAL> void AddOption(const char*name,TVAL*pResult)
        {
			this->optionHandlers.push_back(new COptionHandler<TVAL>(name,pResult));
        }
		const std::vector<std::string> &GetArguments() { return args; }
    template <class T> void GetArgument(size_t arg, T*pVal)
    {
      if (arg >= args.size()) { 
        std::stringstream os;
        os << "Argument " << (arg+1) << " not provided.";
        throw CommandLineException(os.str());
      }
      std::stringstream is(args[arg]);
      T value;
      is >> value;
      if (!is.fail() && is.eof())
      {
        *pVal = value;
      } else {
        std::stringstream os;
        os << "Argument " << (arg+1) << " was not in the correct format.";
        throw CommandLineException(os.str());
      }
    }
    void GetArgument(size_t arg, std::string*pVal)
    {
      if (arg >= args.size()) { 
        std::stringstream os;
        os << "Argument " << (arg+1) << " not provided.";
        throw CommandLineException(os.str());
      }
      *pVal = args[arg];
    }


	private:

		class COptionHandlerBase {
		public:
			COptionHandlerBase(const std::string & name) { this->name = name;}
			virtual ~COptionHandlerBase() { };
			const std::string &getName() const { return name; }
			virtual bool HasArgument()const  = 0; 
			virtual void Process(const char* value) const = 0;
		private:
			std::string name;
		};
		template <class T> class COptionHandler: public COptionHandlerBase {
		public: 
			COptionHandler(const std::string &name,T *result, T defaultValue = T())
				: COptionHandlerBase(name)
			{
				_pResult = result;
				*_pResult = defaultValue;

			}
			virtual bool HasArgument() const;

			virtual void Process(const char *strValue) const {
				std::stringstream is(strValue);
				T value;
        is >> value;
				if (!is.fail() && is.eof())
				{
					*_pResult = value;
				} else {
					std::stringstream os;
					os << "Invalid value provided for option '" << getName() << "'.";
					throw CommandLineException(os.str().c_str());
				}
			}

		private:
			std::string strName;
			T*_pResult;
		};
		const CommandLine::COptionHandlerBase*GetOptionHandler(const std::string &name) const;

		std::vector<std::string > args;
		std::vector<COptionHandlerBase*> optionHandlers;
	};

	// Argument specializations for bool.
	template <class T> bool CommandLine::COptionHandler<T>::HasArgument()const {
		return true; 
	};
	inline bool CommandLine::COptionHandler<bool>::HasArgument() const {
		return false;
	}
	inline void CommandLine::COptionHandler<bool>::Process(const char*strValue) const {
    if (strValue == NULL)
    {
      *_pResult = true;
      return;
    }
		switch (*strValue)
		{
		case '\0':
		case '+':
			*_pResult = true;
			break;
		case '-':
			*_pResult = false;
			break;
		default:
			throw CommandLineException("Please specify '+' or '-' for boolean options.");
		}
	}
	// specializations for std::string.
	inline void  CommandLine::COptionHandler<std::string >::Process(const char*strValue)const  {
		*_pResult  = strValue;
	}

	// specializations for std::vector<std::string>
	inline void  CommandLine::COptionHandler<std::vector<std::string> >::Process(const char*strValue)const  {
		_pResult->push_back(strValue); 
	}
};



#endif
