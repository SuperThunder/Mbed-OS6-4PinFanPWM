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


class YamShell
{
public:
    typedef Callback<void(int argc, char** argv)> _CommandCallback;

    YamShell(PinName serialTX, PinName serialRX, uint32_t baud, bool preserveLine = true);
    void write(const void *buf, std::size_t len);
    void print(const char* s);
    void println(const char* s);
    void printf(const char fmt[], ...);

    void register_command(std::string command_name, _CommandCallback command_function);

    void serial_setbaud(int baud){_bf.set_baud(baud);};
    void serial_setpreserveline(bool pl){_preserve_line = pl;};
    BufferedSerial* getBufferedSerial(){return &_bf;};

private:
    BufferedSerial _bf;
    Thread _input_thread;

    //Array of callbacks to call for a given command string
    //TODO dynamic registration isn't great. There is probably some way to use C++ features so that callback handlers can all be registered at compile time.
    //          The std::string for command name could probably easily be char*, is anything gained by being a string?
    //          The C way would probably be a preprocessor directive that would feed into a big assembled if-else block or static array to loop through
    typedef std::tuple<std::string, _CommandCallback> _CallbackTuple;
    std::array<_CallbackTuple, MAX_COMMAND_COUNT> _commands{};
    uint32_t _command_callback_count = 0;

    bool _preserve_line;
    char _linebuf[LINE_BUFFER_SIZE]{0};
    int _lineind = 0;

    void _input_loop();
    void _input_line_handler(const char* il);

};


#endif