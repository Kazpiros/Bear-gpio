#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
//#include <bcm_host.h>qesd
#define BLOCK_SIZE 4096
#define GPIO_BASE 0xfe200000//0x7C200000
#define UART_MINI_BASE 0xfe215000 // may be different
#define UART0_BASE  0x7e201000
//note to kaz [index] <- i * word.len (i*4)
volatile unsigned *gpio;
volatile unsigned *uart;
char *uart_mem;

void output_GPIO(int pin) // gpfsel - 0x00 to 0x14 offset (by 4's) // ren, fen, !! implement!
{
  int reg = pin / 10;
  int shift = (pin % 10) * 3;
  gpio[reg] &= ~( 7 << shift ); // sets clear 
  gpio[reg] |=  ( 1 << shift ); // sets to 001 to configure as output
}

void input_GPIO(int pin, int pull)
{
    if (pin > 57 || (pull != 1 && pull != 2))  // Ensure valid pin and pull-up/pull-down values
        return;

    int regsel = pin / 10;          
    int shiftsel = (pin % 10) * 3;  
    gpio[regsel] &= ~(7 << shiftsel);
    
    int regpull = pin / 32 + 37;    
    int shiftpull = (pin % 16) * 2; 
    gpio[regpull] &= ~(3 << shiftpull); // Clear
    gpio[regpull] |= (pull << shiftpull); // Set pull
  
    usleep(100); 
}

void write_GPIO(int pin, int value)
{  
  int GPSET = (pin / 32) ? 8 : 7;
  int GPCLR = (pin / 32) ? 11 : 10; 

  if(value)
    gpio[GPSET] = 1 << pin;  // GPSET0 register
  else
    gpio[GPCLR] = 1 << pin; // GPCLR0 register 
}

// mini UART implementation
int mini_baud(int rate){ // output Baudrate set
  
   // 8 bit mode ; gpt said 0x3.. try that later
  uart[0x60/4] &= ~(0xFF);
  uart[0x60/4] |= 0x1; // 0x1 reciever enable
  int baud_reg =  (250000000 / (rate * 8)) - 1; // check

  uart[0x4c/4] |= 0x80; //dlab set (start baud reg modification)
  uart[0x68/4] = 0xCB6; // 415~~ almost 9600 baud
  uart[0x4c/4] |= 0x1; // moved here just to give time to update baud
  uart[0x4c/4] &= ~(0x80); //dlab clear uart ready

  //uart[0x68/4] = baud_reg;
  /*
  printf("bits @ uart_baud: %x\n", uart[0x68/4]);
  printf("bits @ ctrl_reg: %x\n", uart[0x60/4]);
  printf("dlab !!!!!!!! ----> %d \n", uart[0x4c/4] & (1 << 7));
  printf("status (3/2 over run, 3/1 ready) %x\n", (uart[0x54/4] & 0x3));
  */
  __sync_synchronize(); //flush mem barrier


  //DLAB access (bit 7) AUX_MU_LCR_REG (also set bit 0 to 1 for 8-bit mode)
  //If set the first two Mini UART registers give access to the Baudrate register. During operation this bit must be cleared.
}

void mini_uart_enable(){

  gpio[0x04/4] &= ~(0x3FFFFFFF);
  gpio[0x04/4] |= (0x12000); // set fsel1 to alt5 (MINI UART -> RXD1 & TXD1)
  __sync_synchronize();
  uart[0x04/4] &= ~0x7;
  uart[0x04/4] |= 0b001;// enable mini_uart
  __sync_synchronize();
  printf("bits @ gpio fsel: %x\n", gpio[0x04/4]);
  printf("bits @ uart en: %x\n", uart[0x04/4]);
}

int uart_read_byte(){
  //while((uart[0x54] & 0x1) != 1); // no data? no bitches!
  if(uart[0x4c/4] & (1 << 7)){
    return -1;
  }
  
  printf("STATS: \n");
  printf("# of bits in RXD1: %x\n", (uart[0x64/4] & (0xF << 16))); //16 to 19
  printf("idle?: %d\n", (uart[0x64/4] & (1 << 2))==1); //b2 reciever disabled checker
  printf("clear?: %d\n", (uart[0x64/4] & (1 << 1)) == 1); //b1
  printf("symb avail?: %d\n", (uart[0x64/4] & (1)) == 1); //b0
  
  return (uart[0x40/4] & 0xFF); //AUX_MU_IO_REG
}
void loopback_test(){
  //mucntl bit 1 trnasmitter enable
  uart[0x60/4] |= 0b10;
  char test_data[] = "Hello, UART!";
  int length = sizeof(test_data) - 1;
  char received_data[length];
  for(int i = 0; i < length; i++){
    uart[0x40/4] |= (0xFF << 7);
    usleep(208);
    //while (!(uart[0x64 / 4] & 0x1)); // stat reg
    received_data[i] = uart[0x40 / 4] & 0xFF;
  }
  printf("!!!! %c\n", test_data);
}

void write_buffer(char *buff, int size) {
    // Example: Write buffer to a file
    FILE *file = fopen("uart_output.txt", "a");
    if (file) {
        //fwrite(buff, 1, size, file);
        //putc(buff[0], file);
        fclose(file);
    }
}

void uart_read_output(char *buff, int size){
  int i = 0;
  while(i < 125){
    int c = uart_read_byte();
    buff[i++] = (char) c;
    if(i >= size){ // just my buffer size
      //printf("here?");
      //printf(" %x \n", c);
      write_buffer(buff, size);
      i = 0;
    }
  }
}
//write in 2 banks 32 bits (57 total) bit inputs (limit bank 2 to 32:57)
void write_GPIO_masked(unsigned int bank0, unsigned int bank1)
{
  unsigned int clear_bank0 = ~bank0;
  unsigned int clear_bank1 = ~bank1;
  gpio[7] = bank0;
  gpio[8] = bank1;
  gpio[10] &= clear_bank0;
  gpio[11] &= clear_bank1;
}

int read_GPIO(int pin)
{
    if (pin > 57) return 0;  // Invalid pin number

    int reg = pin / 32 + 13; // GPLEV registers start from index 13
    int shift = pin % 32;    // Determine the bit position within the register

    // Check if the pin is high or low
    return (gpio[reg] & (1 << shift)) ? 1 : 0;
}

int port_asn(volatile unsigned** BASE_){
  int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
  if(mem_fd < 0)
  {
    perror("open");
    return -1;
  }
  //sets the starting (virtualy mapped) address for GPIOs
  *BASE_ = (volatile unsigned*) mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, GPIO_BASE);
  
  if(*BASE_ == MAP_FAILED)
  {
    perror("mmap");
    close(mem_fd);
    return -1;
  }
}
int main()
{
  /*
  if (port_asn(&gpio) != 0) {
        return -1; // Handle error
    }
  */
  char buffer[1024]; // so i dont have to read

  int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
  if(mem_fd < 0)
  {
    perror("open");
    return -1;
  }
  if ((unsigned long)uart_mem % 4096)
     uart_mem += 4096 - ((unsigned long)uart_mem % 4096);
  printf("Adjusted UART base address: %08x\n", &uart_mem);
  //sets the starting (virtualy mapped) address for GPIOs
  gpio = (volatile unsigned*) mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, GPIO_BASE);
  uart = (volatile unsigned*) mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, mem_fd, UART_MINI_BASE);
  
  if(gpio == MAP_FAILED)
  {
    perror("mmap");
    close(mem_fd);
    return -1;
  }
  if(uart == MAP_FAILED)
  {
    perror("mmap");
    printf("failed uart map\n");
    close(mem_fd);
    return -1;
  }

  for(int i = 0; i < 8; i++){
    printf("Mapped addr (gpios): %08x \n", gpio[i]);
  }
  //printf("\n");
  for(int i = 0; i < 0x64/4; i++){
    printf("Mapped addr (uart): %08x @ %x\n", uart[i], &uart[i]);
  }
  //fflush(stdout);
  output_GPIO(5);
  input_GPIO(6, 1); //maybe add #define PUP 1
  
  mini_uart_enable(); 
  printf("here"); 
  mini_baud(9600);
  printf("here2"); 
  loopback_test();
  //uart[0x04] = 10;
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
  munmap((void*) gpio, 4096);
  munmap((void*) uart, 4096);
  close(mem_fd);

  return 0;
}
