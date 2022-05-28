#include "yamshell.h"

YamShell::YamShell(PinName serialTX, PinName serialRX, uint32_t baud): _bf(serialTX, serialRX, baud)
{
    //Calls to BufferedSerial are blocking for now
    //TODO try to set to nonblocking (may need to set up mutex so simultaneous write operations (including input echo operations) don't conflict)
    _bf.set_blocking(true);
    _input_thread.start(callback(this, &YamShell::_input_loop));
}

void YamShell::print(const char s[])
{
    int len = strlen(s);
    _bf.write(s, len);
}

void YamShell::println(const char s[])
{
    int len = strlen(s);
    _bf.write(s, len);
    _bf.write(&("\n"), 1);
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
        _bf.write(buffer, vsn_retval);
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

void YamShell::_input_line_handler(const char* il)
{
    //int status, addr = -1; //temporary vars used when parsing the command
    // float tmp_f = 0.0f;
    // int tmp_d = 0;
    //char command[16] = {0};
    
    //Value 1: PWM duty or frequency
    //char value1 [32] = {0};

    //int scan_count = sscanf(il, "%s %s ", command, value1);

    // this->printf("\n--lineBuffer command: %s--\n", command);
    // this->printf("--lineBuffer value1: %s--\n", value1);\
    // this->printf("--lineBuffer scan count: %d--\n", scan_count);

    int argc = 0;
    char* argv[ARG_MAX] = {0};
    
    char token_buffer[LINE_BUFFER_SIZE];
    const char* sep = " \t\n\0";
    char* token;
    char* save_pos;

    this->printf("Got line: %s\n", il);
    
    //TODO copy line to new buffer, tokenize, get command, check if we have command, if we do form argc and argv and call handler
    
    //Copy the command line
    memcpy(token_buffer, il, LINE_BUFFER_SIZE);

    //tokenize
    //First use: scan the token buffer, save our position with the save_pos pointer
    token = strtok_r(token_buffer, sep, &save_pos);
    while(token != NULL)
    {

        argv[argc] = token;
        this->printf("--Tok %d: %s--\n", argc, token);
        argc++;
        if(argc > MAX_COMMAND_COUNT)
        {
            this->println("E: Max number of commands exceeded!");
            return;
        }
        //After first use: return to our saved position
        token = strtok_r(save_pos, sep, &save_pos);
    }

    for(int i = 0; i < argc; i++)
    {
        this->printf("--Tok %d: %s--\n", i, argv[i]);
    }
    
    //get command name
    //this->printf("--Command: %s", *argv[0]);

    //check if it is registered

    //call if it is

//     if(scan_count == 1)
//     {
        
//     else
//     {
//         this->print("E: Invalid input\n");
//     }
}

//Check for input until newline, then process it
void YamShell::_input_loop()
{
    char serialbuf[LINE_BUFFER_SIZE]{0}; 
    char linebuf[LINE_BUFFER_SIZE]{0};
    int lineind = 0;
    int serialcount = 0;
    _bf.write("Input thread\n\n", 13);
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
                    this->print("\n");
                    linebuf[lineind++] = '\0'; //don't include the newline in the line buffer, use its space for the null character

                    //Handle the input (parses the command name and calls relevant handler function)
                    _input_line_handler(linebuf);

                    lineind = 0;
            }
            //handle backspace behaviour - print back backspace, then a space to clear the character, then another backspace
            //for the lineBuffer, we just move the index back one character
            //NOTE: backspace doesn't work in Mbed Studio serial output. Use something like MobaXterm or Putty
            else if(serialbuf[i] == '\b')
            {
                //don't back up if we are at the start of the line
                if(lineind != 0)
                {
                    this->print("\b \b");
                    linebuf[lineind--] = '\0'; //set the most recent char back to \0 and move the line buffer back a character
                }
            }
            // All text chars, print back and record in buffer
            // Space (0x20) is start of printable range, ~ (0x7E) is end
            else if (serialbuf[i] >= ' ' && serialbuf[i] <= '~')
            {
                _bf.write(&(serialbuf[i]), 1);
                linebuf[lineind++] = serialbuf[i];
            }
            //all others chars, ignore for now
            else {
                //do nothing
            }
        }

        ThisThread::sleep_for(10ms);
    }
}