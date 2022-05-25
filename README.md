# Mbed OS 6 Interactive 4 Pin Fan PWM Generation

### Equipment used
- 4 pin PWM Fan (Arctic F8 PWM)
- 12v power supply for fan (current is not particularly high, so batteries in series should be fine also)
- Nucleo F303RE
- ~5k ish resistor to pull up fan tachometer pin to 12v
- 2 resistors to form a voltage divider for the MCU pin that reads the fan tachometer (I used 68k and 20k for a approx 1/4 division). Without these, the tachometer signal is well over 5v with a noise-induced Vp-p of almost 20v. After, around 2.7v with Vp-p under 5v.

### Instructions
- Check the code and change the PwmOut / InterruptIn pins to something that will work for your board
- Build the circuit, remember tach on the fan needs pullup to 12v and then a voltage divider before being read by the MCU
- Connect to the device at 115200 in eg. MobaXterm or Putty and enable "implicit LF in every CR"
- Commands are "duty", "freq", and "output". Enter just the command to see the current value, or with a value after to set the value. Duty and freq take floats in 0 - 1.0, output takes time in ms.
  - Frequency defaults to 25khz and doesn't need to be changed unless you want to see what happens with different values
  - Output sets the interval the current RPM is printed at

### Interference on tachometer pin
I had issues with interference on the tachometer pin. One source was from originally being next to the PWM pin, which of course introduced a bit of 25khz noise. However another source seemed to be from my oscilloscope probe being connected to the same node introducing ~1.1khz noise.
There is a lot of discussion on the internet about this involving various solutions with some combination of capacitors, low pass RC filters, diodes, schmitt triggers, or using a lower noise 12v supply. Using a distant pin and disconnecting sources of interference has been enough for me for now, but better filtering may be needed in some cases.
