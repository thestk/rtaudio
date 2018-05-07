/******************************************/
/*
  apinames.cpp
  by Jean Pierre Cimalando, 2018.

  This program tests parts of RtAudio related
  to API names, the conversion from name to API
  and vice-versa.
*/
/******************************************/

#include "RtAudio.h"
#include <cstdlib>
#include <cctype>
#include <iostream>

int main() {
    std::vector<RtAudio::Api> apis;
    RtAudio::getCompiledApi( apis );

    // ensure the known APIs return valid names
    std::cout << "API names by identifier:\n";
    for ( size_t i = 0; i < apis.size() ; ++i ) {
        const std::string &name = RtAudio::getCompiledApiName(apis[i]);
        if (name.empty()) {
            std::cerr << "Invalid name for API " << (int)apis[i] << "\n";
            exit(1);
        }
        std::cout << "* " << (int)apis[i] << ": '" << name << "'\n";
    }

    // ensure unknown APIs return the empty string
    {
        const std::string &name = RtAudio::getCompiledApiName((RtAudio::Api)-1);
        if (!name.empty()) {
            std::cerr << "Bad string for invalid API\n";
            exit(1);
        }
    }

    // try getting API identifier by case-insensitive name
    std::cout << "API identifiers by name:\n";
    for ( size_t i = 0; i < apis.size() ; ++i ) {
        std::string name = RtAudio::getCompiledApiName(apis[i]);
        if ( RtAudio::getCompiledApiByName(name) != apis[i] ) {
            std::cerr << "Bad identifier for API '" << name << "'\n";
            exit( 1 );
        }
        std::cout << "* '" << name << "': " << (int)apis[i] << "\n";
        for ( size_t j = 0; j < name.size(); ++j )
            name[j] = (j & 1) ? toupper(name[j]) : tolower(name[j]);
        if ( RtAudio::getCompiledApiByName(name) != apis[i] ) {
            std::cerr << "Bad identifier for API '" << name << "'\n";
            exit( 1 );
        }
        std::cout << "* '" << name << "': " << (int)apis[i] << "\n";
    }

    // try getting an API identifier by unknown name
    {
        RtAudio::Api api;
        api = RtAudio::getCompiledApiByName("ALSO");
        if ( api != RtAudio::UNSPECIFIED ) {
            std::cerr << "Bad identifier for unknown API name\n";
            exit( 1 );
        }
        api = RtAudio::getCompiledApiByName("");
        if ( api != RtAudio::UNSPECIFIED ) {
            std::cerr << "Bad identifier for unknown API name\n";
            exit( 1 );
        }
    }

    return 0;
}
