#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
//#include <bcm_host.h>qesd
#define BLOCK_SIZE 4096
#define GPIO_BASE 0xfe200000//0x7C200000
#define UART_MINI_BASE 0xfe215000 // may be different
#define UART0_BASE  0xfe201000
//note to kaz [index] <- i * word.len (i*4)
volatile unsigned *gpio;
volatile unsigned *uart;

#define POINTS_PER_PACK 12
#define LIDAR_HEADER 0x54
static const uint8_t CrcTable[256] ={ 
    0x00, 0x4d, 0x9a, 0xd7, 0x79, 0x34, 0xe3, 0xae,
    0xf2, 0xbf, 0x68, 0x25, 0x8b, 0xc6, 0x11, 0x5c,
    0xa9, 0xe4, 0x33, 0x7e, 0xd0, 0x9d, 0x4a, 0x07,
    0x5b, 0x16, 0xc1, 0x8c, 0x22, 0x6f, 0xb8, 0xf5,
    0x1f, 0x52, 0x85, 0xc8, 0x66, 0x2b, 0xfc, 0xb1,
    0xed, 0xa0, 0x77, 0x3a, 0x94, 0xd9, 0x0e, 0x43,
    0xb6, 0xfb, 0x2c, 0x61, 0xcf, 0x82, 0x55, 0x18, 
    0x44, 0x09, 0xde, 0x93, 0x3d, 0x70, 0xa7, 0xea, 
    0x3e, 0x73, 0xa4, 0xe9, 0x47, 0x0a, 0xdd, 0x90, 
    0xcc, 0x81, 0x56, 0x1b, 0xb5, 0xf8, 0x2f, 0x62, 
    0x97, 0xda, 0x0d, 0x40, 0xee, 0xa3, 0x74, 0x39, 
    0x65, 0x28, 0xff, 0xb2, 0x1c, 0x51, 0x86, 0xcb, 
    0x21, 0x6c, 0xbb, 0xf6, 0x58, 0x15, 0xc2, 0x8f, 
    0xd3, 0x9e, 0x49, 0x04, 0xaa, 0xe7, 0x30, 0x7d, 
    0x88, 0xc5, 0x12, 0x5f, 0xf1, 0xbc, 0x6b, 0x26, 
    0x7a, 0x37, 0xe0, 0xad, 0x03, 0x4e, 0x99, 0xd4, 
    0x7c, 0x31, 0xe6, 0xab, 0x05, 0x48, 0x9f, 0xd2, 
    0x8e, 0xc3, 0x14, 0x59, 0xf7, 0xba, 0x6d, 0x20, 
    0xd5, 0x98, 0x4f, 0x02, 0xac, 0xe1, 0x36, 0x7b, 
    0x27, 0x6a, 0xbd, 0xf0, 0x5e, 0x13, 0xc4, 0x89, 
    0x63, 0x2e, 0xf9, 0xb4, 0x1a, 0x57, 0x80, 0xcd, 
    0x91, 0xdc, 0x0b, 0x46, 0xe8, 0xa5, 0x72, 0x3f, 
    0xca, 0x87, 0x50, 0x1d, 0xb3, 0xfe, 0x29, 0x64, 
    0x38, 0x75, 0xa2, 0xef, 0x41, 0x0c, 0xdb, 0x96, 
    0x42, 0x0f, 0xd8, 0x95, 0x3b, 0x76, 0xa1, 0xec, 
    0xb0, 0xfd, 0x2a, 0x67, 0xc9, 0x84, 0x53, 0x1e, 
    0xeb, 0xa6, 0x71, 0x3c, 0x92, 0xdf, 0x08, 0x45, 
    0x19, 0x54, 0x83, 0xce, 0x60, 0x2d, 0xfa, 0xb7, 
    0x5d, 0x10, 0xc7, 0x8a, 0x24, 0x69, 0xbe, 0xf3, 
    0xaf, 0xe2, 0x35, 0x78, 0xd6, 0x9b, 0x4c, 0x01, 
    0xf4, 0xb9, 0x6e, 0x23, 0x8d, 0xc0, 0x17, 0x5a, 
    0x06, 0x4b, 0x9c, 0xd1, 0x7f, 0x32, 0xe5, 0xa8
};



typedef struct __attribute__ ((packed)){ //do i need __att
  uint16_t distance;
  uint8_t intensity;
} LidarPoints;

typedef struct __attribute__ ((packed)){
  uint8_t header;
  uint8_t ver_len;
  uint8_t speed;
  uint16_t start_angle;
  LidarPoints point[POINTS_PER_PACK];
  uint16_t end_angle;
  uint16_t timestamp;
  uint16_t crc8;
}LidarFrame;

uint8_t CalCRC8(uint8_t *p, uint8_t len){
  uint8_t crc = 0;
  for(int i = 0; i < len; i++){
    crc = CrcTable[(crc ^ *p++) & 0xFF];
  }
  return crc;
}

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
  int baud_reg =  (250000000 / (rate * 8)) - 1; // implement

  uart[0x4c/4] |= 0x80; //dlab set (start baud reg modification)
  uart[0x68/4] = 0xCB6; // 415~~ almost 9600 baud
  uart[0x4c/4] |= 0x1; // moved here just to give time to update baud
  uart[0x4c/4] &= ~(0x80); //dlab clear uart ready
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
    while (!(uart[0x64 / 4] & 0x1)); // stat reg
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

int map_memory(volatile unsigned **addr, int mem_fd, off_t base, size_t size) {
    *addr = (volatile unsigned *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, base);
    if (*addr == MAP_FAILED) {
        perror("mmap");
        return -1;
    }
    return 0;
}

void unmap_memory(volatile unsigned *addr, size_t size) {
    if (addr != MAP_FAILED) {
        munmap((void *)addr, size);
    }
}

int setup_peripherals() {
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("open");
        return -1;
    }

    if (map_memory(&gpio, mem_fd, GPIO_BASE, 4096) != 0) {
        close(mem_fd);
        return -1;
    }

    if (map_memory(&uart, mem_fd, UART_MINI_BASE, 4096) != 0) {
        unmap_memory(gpio, 4096);
        close(mem_fd);
        return -1;
    }

    close(mem_fd);
    return 0;
}

void cleanup_peripherals() {
    unmap_memory(gpio, 4096);
    unmap_memory(uart, 4096);
}

void lidar_mUART_packer(){
  //while((uart[0x64/4] & (0xF << 16)) < 8); // # of bits in fifo mu_stat 16-19
  //do lsr reg bit 0 (data ready "active high")
  //AUX_MU_IIR_REG  bit 1 will clear rx fifo
  int header_start = 0; // boolean incase theres a databyte with same header.
  LidarFrame Frame;
  LidarPoints Cloud[POINTS_PER_PACK];
  uint8_t fifo_buffer;
  while((uart[0x40/4] & 0xFF) != LIDAR_HEADER);
}
