/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"

#define PWM_PIN PB_3
#define TACH_PIN PA_0


Thread input_thread;
Thread pwm_thread;
//Thread tach_thread;
Ticker tach_ticker;
Thread output_thread;
BufferedSerial bf(USBTX, USBRX, 115200);
//FILE* bf_pc_out;
PwmOut fan_pwm(PWM_PIN); //Arduino D3 pin
InterruptIn tach_in(TACH_PIN); //Arduino D5 pin

//TODO replace global variables with RTOS primitive between threads (probably queues of length 1)
//Config struct written by input thread and read by pwm / tach / output threads
struct {
    float pwm_duty = 0.5;
    float pwm_frequency = 25000;
    chrono::milliseconds tach_update_ms = 1000ms;
    chrono::milliseconds output_update_ms = chrono::milliseconds(2000);
} config;

//TODO avoid global tach variable by having a Tachometer class whose instantiated object carry the state, will need to do the proper callback to the member function
//Then tach thread can calculate RPM From frequency and send to output thread
//incremented by tachometer ISR
volatile int tach_count = 0;
int tach_rpm = -1;

void bf_print(BufferedSerial* bf, char s[])
{
    int len = strlen(s);
    bf->write(s, len);
}

void bf_println(BufferedSerial* bf, char s[])
{
    int len = strlen(s);
    bf->write(s, len);
    bf->write(&("\n"), 1);
}

//variadic function so printf can be used with buffered serial (alternatively, you can get the )
void bf_printf(BufferedSerial* bf, const char fmt[], ...)
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
        bf->write(buffer, vsn_retval);
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

//set PWM frequency to specified value
void pwm_loop()
{
    bf_printf(&bf, "Starting PWM loop\n");
    //float pwm_duty= 0.5;
    //float pwm_period = (1.0/25000.0); //25khz
    fan_pwm.period(1/config.pwm_frequency);
    //TODO only set period / duty when it actually changes
    while(true)
    {
        fan_pwm.period(1/config.pwm_frequency);
        fan_pwm.write(config.pwm_duty);

        ThisThread::sleep_for(250ms);
    }

}

//Handle interrupt from tachometer
void tach_isr()
{
    tach_count++;
}

//check the tach variable at the update frequency, calculate the RPM and set var, reset tach variable
// void tach_loop()
// {
//     bf_printf(&bf, "Starting tach loop\n");
//     while(true)
//     {
//         //times 60 seconds per minute, divided by 2 pulses per rotation
//         tach_rpm  = tach_count * 60 / 2;
//         bf_printf(&bf, "Tach count: %d\n", tach_count);

//         //TODO use a proper RTOS way of controlling access to ISR volatile count variable
//         tach_count = 0; //ok since write to an integer is atomic, but still not great

//         //TODO tach loop should trigger at a fixed frequency (Ticker?)
//         //sleep slightly faster than update frequency to guarantee most recent value
//         ThisThread::sleep_for(config.output_update_ms * 9 / 10);
//     }
// }

//Timer functions should be totally nonblocking
//Avoid memory alloc or printf
void tach_rpm_tick()
{
    //times 60 seconds per minute, divided by 2 pulses per rotation
    tach_rpm  = tach_count * 60 / 2;

    //TODO use a proper RTOS way of controlling access to ISR volatile count variable
    tach_count = 0; //ok since write to an integer is atomic, but still not great
}

void input_line_handler(char* il)
{
    int status, addr = -1; //temporary vars used when parsing the command
    float tmp_f = 0.0f;
    int tmp_d = 0;
    char command[16] = {0};
    
    //Value 1: PWM duty or frequency
    char value1 [32] = {0};

    int scan_count = sscanf(il, "%s %s ", command, value1);

    //TODO: buffer overflow possible, use strtok or regex instead?
    //fprintf(bf_pc_out, "\n--lineBuffer command: %s--\n", command);
    //fprintf(bf_pc_out, "--lineBuffer value1: %s--\n", value1);\
    //fprintf(bf_pc_out, "--lineBuffer scan count: %d--\n", scan_count);
    bf_printf(&bf, "\n--lineBuffer command: %s--\n", command);
    bf_printf(&bf, "--lineBuffer value1: %s--\n", value1);\
    bf_printf(&bf, "--lineBuffer scan count: %d--\n", scan_count);

    if(scan_count == 1)
    {
        if(strcmp("duty", command) == 0)
        {
            //print duty
            bf_printf(&bf, "PWM Duty: %f\n", config.pwm_duty);
        }
        else if(strcmp("freq", command) == 0)
        {
            //print frequency
            bf_printf(&bf, "PWM Frequency: %f\n", config.pwm_frequency);
        }
        else if(strcmp("output", command) == 0)
        {
            //print output frequency
            bf_printf(&bf, "Output frequency (ms): %d\n", config.output_update_ms);
        }
        //todo unknown command bit here
    }
    else if(scan_count == 2)
    {
        if(strcmp("duty", command) == 0)
        {
            status = sscanf(value1, "%f", &tmp_f);
            //set duty
            config.pwm_duty = tmp_f;
        }
        else if(strcmp("freq", command) == 0)
        {
            status = sscanf(value1, "%f", &tmp_f);
            //set frequency
            config.pwm_frequency = tmp_f;
        }
        else if(strcmp("output", command) == 0)
        {
            status = sscanf(value1, "%d", &tmp_d);
            //set output frequency
            config.output_update_ms = chrono::milliseconds(tmp_d);
        }
        //todo unknown command bit here
    }
    else
    {
        //fprintf(bf_pc_out, "E: Invalid input\n");
        bf_print(&bf, "E: Invalid input\n");
    }
}

//Check for input until newline, then process it
void input_loop()
{
    char serialbuf[255]{0};
    char linebuf[256]{0};
    int lineind = 0;
    int serialcount = 0;
    bf.write("Input thread\n\n", 13);
    while(true)
    {
        serialcount = bf.read(serialbuf, sizeof(serialbuf));
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
                    bf_print(&bf, "\n");
                    //bf.write((void*)'\n', 2); //print back the newline
                    //fputc('\n', bf_pc_out);
                    //fprintf(pc, "\n");

                    linebuf[lineind++] = '\0'; //don't include the newline in the line buffer

                    //Handle the input
                    input_line_handler(linebuf);

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
                    bf_print(&bf, "\b \b");
                    //bf.write("\b \b", 4); //print backspace space backspace to move back a character and clear the character on the terminal
                    //fprintf(bf_pc_out, "\b \b");
                    linebuf[lineind--] = '\0'; //set the most recent char back to \0 and move the line buffer back a character
                }
            }
            // All text chars, print back and record in buffer
            // Space (0x20) is start of printable range, ~ (0x7E) is end
            else if (serialbuf[i] >= ' ' && serialbuf[i] <= '~')
            {
                //bf.write((char*)(serialbuf[i]), 2);
                //char tmpc[2]{0};
                //tmpc[0] = serialbuf[i];

                //char c = serialbuf[i];
                bf.write(&(serialbuf[i]), 1);

                //bf.sync();

                //fputc(serialbuf[i], bf_pc_out);
                linebuf[lineind++] = serialbuf[i];
            }
            //all others chars, ignore for now
            //Arrow keys, home, end, tab, DEL, etc
            else {
                //do nothing
            }
        }

        ThisThread::sleep_for(10ms);
    }
}

//Output the tach reading at a specific frequency
void output_loop()
{
    bf_printf(&bf, "Starting output loop\n");
    while(true)
    {
        //TODO Proper RTOS way of getting the current tach RPM from the tach loop
        bf_printf(&bf, "Tach reading (RPM): %d\n", tach_rpm);
        ThisThread::sleep_for(config.output_update_ms);
    }

}

int main()
{
    // Initialise the digital pin LED1 as an output
    DigitalOut led(LED1);
    led = 1;

    tach_in.rise(&tach_isr);

    bf.set_blocking(true);

    //bf_pc_out = fdopen(&bf, "w");
    bf_print(&bf, "\nStarting threads\n");
    //fprintf(bf_pc_out,  "\nStarting threads\n");

    input_thread.start(input_loop);
    pwm_thread.start(pwm_loop);
    //tach_thread.start(tach_loop);
    tach_in.mode(PullDown);
    tach_ticker.attach(&tach_rpm_tick, config.tach_update_ms);
    output_thread.start(output_loop);

    while (true) {
        ThisThread::sleep_for(10s);
    }
}

