Info:
______________________
I mainly wrote this program to learn about the devices wireless capabilities, and become more familiar with C++ programming, and the Smart Response XE in general.

Current issues I am trying to figure out is making an option for device to be woken from sleep if a new message comes in. I have no issue having a new message waking device from sleep, but the woken device has issues transmitting after that. I need to learn more about the devices registers and interrupts to make this work.

Also messages can get garbled if two devices try to send at same time, or one device sends messages too fast.


Setup:
----------------------
This program for the Smart Response XE requires Larry Bank's support library which can be downloaded here:

https://github.com/bitbank2/SmartResponseXE

After downloading library zip file, add it as a zip library to your sketch.

For the sleep to work, in SmartResponseXE.cpp, in the SRXESleep() function,
you must comment out this line:

TRXPR = 1 << SLPTR; // send transceiver to sleep

Commands:
---------------------
Change username:  _n

Sleep device:     bye

Usage:
--------------------
When the devices are facing each other, and under 25 feet away,
the message transmission seems to be pretty consistant.

But due to the low power of the radio, and the PCB antenna, messages often
will not go through depending on if there is anything inbetween or even which
direction the device is facing.
