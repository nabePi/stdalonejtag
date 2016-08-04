#define DEBUG

#ifdef DEBUG
#include <avr/pgmspace.h>
#include <stdio.h>
#define dprintf(format, args...) printf_P(PSTR(format), ## args)
#else
#define dprintf(format, args...) 
#endif
