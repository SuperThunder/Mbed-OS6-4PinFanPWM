#include "yamshell.h"

YamShell::YamShell(PinName serialTX, PinName serialRX, uint32_t baud, bool preserveLine): _bf(serialTX, serialRX, baud)
{
    //Calls to BufferedSerial are blocking for now
    //TODO try to set to nonblocking (may need to set up mutex so simultaneous write operations (including input echo operations) don't conflict)
    _bf.set_blocking(true);
    _input_thread.start(callback(this, &YamShell::_input_loop));
    _preserve_line = preserveLine;
}


//intermediate layer to BufferedSerial.write to implement input line preserving feature
//TODO may want some special methods newline, backspace, putc, etc to write N number of backspaces / newlines or a single character 
//  - and optionally without trying to preserve the current line at the bottom
void YamShell::write(const void *buf, std::size_t len)
{
    //TODO may need mutex to coordinate with input thread char echo code
    //preserve line functionality:
    //      if the line buffer already has text, then reprint it on a newline after getting a print command
    //      this way the active content will be preserved at the bottom
    if(_preserve_line && _lineind > 0)
    {
        //go to a new line
        _bf.write(&("\n"), 1);

        //put new output
        _bf.write(buf, len);

        //down a line
        _bf.write(&("\n"), 1);

        //put old input back
        _bf.write(_linebuf, _lineind);
    }
    else 
    {
        _bf.write(buf, len);
    }
}


void YamShell::print(const char s[])
{
    int len = strlen(s);
    this->write(s, len);
}

void YamShell::println(const char s[])
{
    this->print(s);
    this->print("\n");
}

//variadic function so printf can be used with buffered serial (alternatively, you can get the FILE* handle for the BufferedSerial with fdopen and use fprintf)
void YamShell::printf(const char fmt[], ...)
{
    //sized to RX buffer size in case entire line needs to be written
    char buffer[MBED_CONF_DRIVERS_UART_SERIAL_RXBUF_SIZE] = {0};
    uint32_t vsn_retval = 0;

    va_list args;
    va_start(args, fmt);
    
    //non negative and less than specified buffer size: entire string written to buffer
    vsn_retval = vsnprintf(buffer,MBED_CONF_DRIVERS_UART_SERIAL_RXBUF_SIZE,fmt,args);
    
    va_end(args);
    
    if(vsn_retval > 0 && vsn_retval < MBED_CONF_DRIVERS_UART_SERIAL_RXBUF_SIZE)
    {
        this->write(buffer, vsn_retval);
    }
    else if(vsn_retval >= MBED_CONF_DRIVERS_UART_SERIAL_RXBUF_SIZE)
    {
        error("E: vsnprintf return value exceeded buffer size! (%d)\r\n", vsn_retval);
    }
    else
    {
        error("E: vsnprintf negative value (%d)\r\n", vsn_retval);
    }
}

void YamShell::registerCommand(std::string command_name, _CommandCallback command_function)
{
    //check current number of callbacks
    if(_command_callback_count >= _commands.size())
    {
        this->println("E: Max number of registered commands already reached!");
        return;
    }

    _CallbackTuple com_tuple = std::make_tuple(std::string(command_name), command_function);
    _commands[_command_callback_count++] = com_tuple;
    this->printf("Registering command: %s\n", command_name.c_str());
}


void YamShell::_input_line_handler(const char* il)
{
    //do nothing if nothing in line
    if(il[0] == '\0')
    {
        return;
    }

    int argc = 0;
    char* argv[ARG_MAX] = {0};
    
    char token_buffer[LINE_BUFFER_SIZE];
    const char* sep = " \t\n";
    char* token;
    char* save_pos;

    //this->printf("Got line: %s\n", il);
    
    //Copy the command line
    memcpy(token_buffer, il, LINE_BUFFER_SIZE);

    //tokenize
    //First use: scan the token buffer, save our position with the save_pos pointer
    token = strtok_r(token_buffer, sep, &save_pos);
    argv[argc++] = token;
    while(token != NULL)
    {
        //this->printf("--Tok %d: %s--\n", argc-1, token);
        //After first use: return to our saved position
        token = strtok_r(save_pos, sep, &save_pos);
        argv[argc++] = token;

        if(argc > MAX_COMMAND_COUNT)
        {
            this->println("E: Max number of commands exceeded!");
            return;
        }
    }
    argc--; //argc value increment overshoots by 1, so decrement after getting all tokens

    //show all tokens found
    // for(int i = 0; i < argc; i++)
    // {
    //     this->printf("--Tok %d: %s--\n", i, argv[i]);
    // }
    
    //get command name
    //this->printf("--Command: %s--\n", argv[0]);
    std::string input_command(argv[0]);

    //check if it is registered
    //Using lambda with std::find_if: https://stackoverflow.com/questions/42933943/how-to-use-lambda-for-stdfind-if
    _CallbackTuple* ct_search = std::find_if(_commands.begin(), _commands.end(), [&input_command](const _CallbackTuple& ct){return std::get<0>(ct) == input_command;});
    if(ct_search != _commands.end())
    {
        //call if it is
        _CommandCallback cc = std::get<1>(*ct_search);
        cc.call(argc, argv);
    }
    else
    {
        this->printf("E: Unknown command:  %s\n", argv[0]);
    }

}

//Check for input until newline, then process it
void YamShell::_input_loop()
{
    char serialbuf[LINE_BUFFER_SIZE]{0}; 

    int serialcount = 0;
    this->print("Input thread\n\n");
    while(true)
    {
        //TODO: Don't go over buffer size

        serialcount = _bf.read(serialbuf, sizeof(serialbuf));
        //go through all the chars we just received
        // echo them back as appropriate
        // and also fill up a line buffer that we will send to a handler function when we hit a newline
        for(int i = 0; i < serialcount; i++)
        {
            //handle newline behaviour -> print back newline, call lineBuffer handler function
            if(serialbuf[i] == '\r' || serialbuf[i] == '\n')
            {
                    //handle CR LF or LF CR cases by skipping the second char of the sequence
                    if( serialbuf[i+1] == '\n' || serialbuf[i+1] == '\r')
                    {
                        i++;
                    }
                    _bf.write("\n", 1);
                    _linebuf[_lineind++] = '\0'; //don't include the newline in the line buffer, use its space for the null character

                    //Set line index to 0 so print commands in the input line handler don't try to preserve the line buffer contents 
                    //(the user has pressed enter, so command is submitted and doesn't need to be preserved for further editing)
                    //OK since _input_line_handler is not async, we don't get further input from serial until we are done here
                    _lineind = 0;
                    //Handle the input (parses the command name and calls relevant handler function)
                    _input_line_handler(_linebuf);

            }
            //handle backspace behaviour - print back backspace, then a space to clear the character, then another backspace
            //for the lineBuffer, we just move the index back one character
            //NOTE: backspace doesn't work in Mbed Studio serial output. Use something like MobaXterm or Putty
            else if(serialbuf[i] == '\b')
            {
                //don't back up if we are at the start of the line
                if(_lineind != 0)
                {
                    //this->print("\b \b");
                    _bf.write("\b \b", 3);
                    _linebuf[_lineind--] = '\0'; //set the most recent char back to \0 and move the line buffer back a character
                }
            }
            // All text chars, print back and record in buffer
            // Space (0x20) is start of printable range, ~ (0x7E) is end
            else if (serialbuf[i] >= ' ' && serialbuf[i] <= '~')
            {
                _bf.write(&(serialbuf[i]), 1);
                _linebuf[_lineind++] = serialbuf[i];
            }
            //all others chars, ignore for now
            else {
                //do nothing
            }
        }

        ThisThread::sleep_for(10ms);
    }
}