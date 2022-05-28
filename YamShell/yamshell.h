/*
YamShell (Yet Another Mbed Shell) is a serial based CLI for Mbed OS 6.
It is designed to be easily used in quick and small projects.
If you need a proper shell for serious purposes look at NaturalTinyShell.
*/

#ifndef YAMSHELL_H
#define YAMSHELL_H

#include "mbed.h"
#include <array>
#include <tuple>
#include <string>

constexpr uint32_t MAX_COMMAND_COUNT = 32;
constexpr uint32_t LINE_BUFFER_SIZE = 256;
constexpr uint32_t ARG_MAX = 8; //max number of arguments allowed

//TODO:
//Command registration with callback (to vector of string in heap maybe)

//Argc/Argv generation:
//Make a copy of the line buffer (possibly not strictly needed here, but so that there is not contention)
//Figure out argc and argv using strtok_r or strsep
//Call commands with argc and argv (argv points to the start of each token in the copied line buffer)

//Preserves input line even while output printing
//Provides easy access to printf, println, write(char*) functionsr

class YamShell
{
public:
    YamShell(PinName serialTX, PinName serialRX, uint32_t baud);
    void print(const char* s);
    void println(const char* s);
    void printf(const char fmt[], ...);

    void register_command(std::string command_name, Callback<void(int argc, char** argv)> command_function);

    void serial_setbaud(int baud){_bf.set_baud(baud);};
    BufferedSerial* getBufferedSerial(){return &_bf;};

private:
    BufferedSerial _bf;
    Thread _input_thread;

    //Array of callbacks to call for a given command string
    //TODO dynamic registration isn't great. There is probably some way to use C++ features so that callback handlers can all be registered at compile time.
    //          The C way would probably be a preprocessor directive that would feed into a big assembled if-else block or static array to loop through
    std::array<std::tuple<const std::string, Callback<void(int argc, char** argv)>>, MAX_COMMAND_COUNT> _commands{};

    void _input_loop();
    void _input_line_handler(const char* il);

    
};


#endif