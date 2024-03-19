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
