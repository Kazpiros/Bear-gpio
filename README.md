# Bear-gpio
### Bare-metal implementation of Raspberry pi 4 gpio C library

- Can run UART rx and tx at any speed
- interrupt support
- Full access to GPIO with parallel bitmasking
- arduino-like programming

(for almost baremetal, w/o boot) 
Running on debian os:
``` gcc -o main main.c ```

To boot off SD card
``` make ```


### Why use this? 
- don't.
- I spent a lot of time getting so far to get uart, interrupts, and gpio to work on surface, boot, and (soon to be) driver-interfaced.
- If you like arduinos, or programming with atmel, this lets you do the same thing, but you're binded to the power of the raspberry pi, and youre held victim to its complete and utter lack of public documentation of its systems
  - Yes, i understand "cortex ..." exists, the whole SBC is shrouded in a mystique that only a mad man would try to uncover
