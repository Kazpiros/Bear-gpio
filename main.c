#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include "kBCM2711.h"

void read_out_peripherals();

int main()
{
  
  if (setup_peripherals() != 0) {
        return -1; // Handle error
  }
  read_out_peripherals();

  char buffer[1024]; // so i dont have to read

  output_GPIO(5);
  input_GPIO(6, 1); //maybe add #define PUP 1
  
  mini_uart_enable(); 
  printf("here"); 
  mini_baud(9600);
  printf("here2"); 

  while(1)
  {
    printf("looping.. %d \n", read_GPIO(6));
    fflush(stdout);
    char c = uart_read_byte();
    printf("byte: %x\n",c);
    
    c = uart_read_byte();
    printf("byte: %x\n",c);
    //if(read_GPIO(6))
    //  printf("success!");
    write_GPIO(5,1);
    sleep(1);
    write_GPIO(5,0);
    sleep(1);
    
    printf("AUX_MU_LCR_REG: %x\n", uart[0x4C/4]);  // Check LCR_REG value
    printf("AUX_MU_LSR_REG: %x\n", uart[0x54/4]);  // Check LSR_REG value (Line Status Register)
    printf("AUX_MU_IO_REG: %x\n", uart[0x40/4]);  // Check IO_REG value (Input/Output Register)
    
  }
  cleanup_peripherals();
  return 0;
}

void read_out_peripherals(){
  for(int i = 0; i < 8; i++){
    printf("Mapped addr (gpios): %08x \n", gpio[i]);
  }
  //printf("\n");
  for(int i = 0; i < 0x64/4; i++){
    printf("Mapped addr (uart): %08x @ %x\n", uart[i], &uart[i]);
  }
}
