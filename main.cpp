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

//command logic to reimplement
//     if(strcmp("duty", command) == 0)
//     {
//         //print duty
//         this->printf("PWM Duty: %f\n", config.pwm_duty);
//     }
//     else if(strcmp("freq", command) == 0)
//     {
//         //print frequency
//         this->printf("PWM Frequency: %f\n", config.pwm_frequency);
//     }
//     else if(strcmp("output", command) == 0)
//     {
//         //print output frequency
//         this->printf("Output frequency (ms): %d\n", config.output_update_ms);
//     }
//     //todo unknown command bit here
// }
// else if(scan_count == 2)
// {
//     if(strcmp("duty", command) == 0)
//     {
//         status = sscanf(value1, "%f", &tmp_f);
//         //set duty
//         config.pwm_frequency = tmp_f;
//         fan_pwm.write(tmp_f);
//     }
//     else if(strcmp("freq", command) == 0)
//     {
//         status = sscanf(value1, "%f", &tmp_f);
//         //set frequency
//         config.pwm_duty = tmp_f;
//         fan_pwm.period(1/tmp_f);
//     }
//     else if(strcmp("output", command) == 0)
//     {
//         status = sscanf(value1, "%d", &tmp_d);
//         //set output frequency
//         config.output_update_ms = chrono::milliseconds(tmp_d);
//         //output_ticker.detach();
//         //output_ticker.attach(&output_ti)
//     }
//     //todo unknown command bit here
// }

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

void command_duty(int argv, char** argvv)
{

}

void command_frequency()
{

}

void command_output()
{
    
}

int main()
{
    // Initialise the digital pin LED1 as an output
    DigitalOut led(LED1);
    led = 1;

    ys.print("\nStarting threads\n");
    //ys.register_command("duty", &command_duty);

    output_thread.start(output_loop);

    ThisThread::sleep_for(1s); //small delay for tach to get first RPM reading
    //output_ticker.attach(&output_tick, config.output_update_ms);

    while (true) {
        ThisThread::sleep_for(10s);
        led = !led;
    }
}

