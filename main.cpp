/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"
#include "tach.h"
#include "yamshell.h"

#define PWM_PIN PB_3
#define TACH_PIN PA_0
#define DEFAULT_PWM_FREQUENCY 25000.0f
#define DEFAULT_PWM_DUTY 0.5f

//define YAMSHELL_NO_EVENT_THREAD to disable of EventQueue + a thread for it to run in
#define YAMSHELL_NO_EVENT_THREAD

Thread output_thread;

Tach fan_tach(TACH_PIN);
YamShell ys(USBTX, USBRX, 115200);
PwmOut fan_pwm(PWM_PIN); //Arduino D3 pin

//Global config struct so that input line handler can print back the current value
//OK for a small program like this but a neater organization would be if there was an "PWMApp" directory with pwmapp.h / pwmapp.cpp with this struct,
//      the output tick function and the input line handler. The main function here would set up and connects classes to each other that operate on their own with
//      state neatly kept only in the context of functions to which it is relevant.
struct {
    float pwm_frequency = DEFAULT_PWM_FREQUENCY;
    float pwm_duty = DEFAULT_PWM_DUTY;
    chrono::milliseconds output_update_ms = chrono::milliseconds(2000);
} config;

//Output the tach reading at a specific frequency
//This thread could be replaced by a Ticker function that sends the printf call to an event queue. 
//      - Equivalent except for one less task stack of memory use, if the event queue already exists
void output_loop()
{
    while(true)
    {
        ys.printf("Tach reading (RPM): %d\n", fan_tach.getRPM());
        ThisThread::sleep_for(config.output_update_ms);
    }

}

void command_duty(int argc, char** argv)
{
    //just command name
    if(argc == 1)
    {
        ys.printf("PWM duty: %f\n", config.pwm_duty);
    }
    //if value provided too
    else if(argc == 2)
    {
        float tmp_f;
        int status = sscanf(argv[1], "%f", &tmp_f);
        //set duty
        config.pwm_duty = tmp_f;
        fan_pwm.write(tmp_f);
    }
}

void command_frequency(int argc, char** argv)
{
    //just command name
    if(argc == 1)
    {
        ys.printf("PWM frequency: %f\n", config.pwm_frequency);
    }
    //if value provided too
    else if(argc == 2)
    {
        float tmp_f;
        int status = sscanf(argv[1], "%f", &tmp_f);
        //set duty
        config.pwm_frequency = tmp_f;
        fan_pwm.period(1.0 / tmp_f);
    }
}

void command_output(int argc, char** argv)
{
    //just command name
    if(argc == 1)
    {
        ys.printf("Output frequency (ms): %d\n", config.output_update_ms);
    }
    //if value provided too
    else if(argc == 2)
    {
        int tmp_d;
        int status = sscanf(argv[1], "%d", &tmp_d);
        //set duty
        config.output_update_ms = chrono::milliseconds(tmp_d*1000);
    }
}



int main()
{
    // Initialise the digital pin LED1 as an output
    DigitalOut led(LED1);
    led = 1;

    fan_pwm.write(config.pwm_duty);
    fan_pwm.period(1.0/config.pwm_frequency);

    ys.print("\nStarting threads\n");
    ThisThread::sleep_for(1s);
    ys.registerCommand("duty", &command_duty);
    ys.registerCommand("freq", &command_frequency);
    ys.registerCommand("output", &command_output);

    ys.getEventQueue()->call(printf, "\ntest\n");

    ThisThread::sleep_for(1s); //small delay for tach to get first RPM reading
    output_thread.start(output_loop);


    while (true) {
        ThisThread::sleep_for(3s);

        led = !led;
    }
}

