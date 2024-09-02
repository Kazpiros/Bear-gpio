#include <stdio.h>
//#include "kBCM2711.c"
//#include <bcm_host.h>
#define BLOCK_SIZE 4096
#define GPIO_BASE 0xfe200000//0x7C200000

//note to kaz [index] <- i * word.len (i*4)
volatile unsigned *gpio;

void output_GPIO(int pin);

void input_GPIO(int pin, int pull);

void write_GPIO(int pin, int value);

int read_GPIO(int pin);

void initialize();