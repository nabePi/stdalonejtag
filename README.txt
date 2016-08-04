A few notes:

- The baud rate the AVR speaks is 38400, no parity, 8 databits, 1 stopbit
- If you need to convert an svf to an xsvf, use the binaries here:
 http://code.google.com/p/the-bus-pirate/downloads/detail?name=BPv3.XSVFplayer.v1.1.zip&can=2&q=
The best way to invoke it it:
./svf2xsvf502 -fpga -rlen 1024 -useXSDR  -d -i file.svf -o file.xsvf
- If you want to take the risk, you can overclock your AVR to the max. See
the OVERCLOCK_DANGEROUSLY define in overclock.c for more info.
- If you program your ATTiny85, take care to set the fuses correctly. They
should be: lfuse 0xF1, hfuse 0xDD, efuse 0xFF.
- When uploading via xmodem, the first sector usually takes a few seconds 
to be accepted. This is because the firmware only erases the flash after one
sector has been received successfully. Erasing the flash takes a few seconds.
- If your xsvf doesn't run, try connecting it to the serial port and press
'l' in the xmodem mode. It should show what happened the last few times the
chip tried parsing/uploading the xsvf.

