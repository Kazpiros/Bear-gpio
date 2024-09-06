#ifndef auxIRQ_H
#define auxIRQ_H

#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include "kBCM2711.h"
#include <time.h>


#define GICD_BASE 0x4c0041000
#define GICC_BASE 0x4c0042000 //to change

#define IRQ_ID (29+96)

#define WRITE_SYSREG(value, reg) asm volatile ("msr " #reg ", %0" :: "r"(value))

typedef volatile uint32_t reg32;

volatile unsigned *GICD = (volatile unsigned *) GICD_BASE;
volatile unsigned *GICC = (volatile unsigned *) GICC_BASE;
// "spi" id = 96 + VC_IRQ_ID    ie: aux_vc -> 96 + 29 = 125
// interrupts are identified using ID numbers (INTIDs).
// spi interrupt type - shared peripheral interrupts @ (INTID 32-1019)
//When GICD_CTLR.DS==0, Secure access: allows gicc to have bit 0 set as grp 0 (and gp1 at bit 1) , if not secure, only grp 1 @ bit 0 
void gcid_set_prio(int priority, int id){ // priority 0 (highest) to 255 lowest
    int n = (id / 4);
    int offset = (0x400 + 4*n);
    int bOffset = (id % 4) * 8;
    //GICD[offset/4] |= (0xFF << bOffset); // can FF be anything or does it need to be FF?
    GICD[offset/4] = GICD[offset/4] & ~(0xFF << bOffset) | (priority << bOffset);//redo above like this perhaps? ->& ~(0xFF << shift)) | (priority << shift);
    
}

void gicd_en_irq(int id){
    int offset = (0x100 + (id/32) * 4);
    GICD[offset/4] |= 1 << (id % 32);//0x0100-0x017C @ GICD_ISENABLER<n>
}

void gicc_ack_irq(int id){
    //GICC_IAR - 0x000C
    int ack_sig;
    ack_sig = GICC[0xC/4]; //ack'd
    if(ack_sig != id)
        printf("irq acks do not match\n");
    GICC[0x10/4] |= ack_sig; // finish ack
}

void gicc_en(void){
    GICC[0] |= 9; // enable grp 0 & FIQEn
}
void gicd_en(void){
    GICD[0] |= 1;
    GICD[0] &= ~(1<<6); //disable security !! make sure its disabled
    //add cpu targetter
}

//0x4c0040000 <- GIC Base address
void aux_IRQSetup(){
    
    IRQ[0x220/4] |= 0xFFFFFFFF;//clear enable - IRQ0_CLR_EN_0 0x220
    IRQ[0x210/4] |= 0xFFFFFFFF;//IRQ0_SET_EN_0  0x210
    uart[0x48/4] |= 0x6; //clear fifos
    uart[0x44/4] |= 0x3; //set interupts
}

void enable_interrupt_controller(){
    IRQ[0x210/4] |= (1 << 29); //irq0 set enable @ aux bit
}

extern void interrupt_handler(void);

void interrupt_handler() {

    printf("interrupted.\n");

    // Optionally, clear the interrupt in the GIC to prevent it from re-triggering
    // Example: Clear the interrupt in the appropriate GIC register if necessary
}

/* Trash vvv */
void init_irq(void){
    asm volatile ("msr DAIFClr, #0x2");
}

void *vector_table[] __attribute__((section(".vectors"))) = {
    [125] = interrupt_handler, // IRQ number for UART or other peripheral
};


void setup_vbar() {
    WRITE_SYSREG((unsigned long)vector_table, vbar_el1);
}
void uart_read_stream(char *buffer, LidarFrame **Lidar_Data){
    //*buffer = malloc (10 * sizeof (*buffer));
    char temp = ' ';
    int i = 0;
    int header_check = 1; //change to zero after check
    while(1){
        clock_t begin = clock();
        char uByte = reverse_byte(uart_read_byte());
        
        uart[0x48/4] = 0x0;  // bit clear
        uart[0x48/4] |= 0xFF;  // bit clear        
        clock_t end = clock();
        double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        //printf("time: %f\n", time_spent);

        if(uByte == LIDAR_HEADER){ //Lidar_header
                i=0; // reset to first frame
                //printf("header found!\n");
                header_check = 1;
                //printf("\n");
        }
        if((IRQ[0x200/4] & (1 << 29)) && (header_check == 1))// if detects non zero value @ interrupt line
        {    
            uart[0x48/4] |= 0x6; // clear fifo
            //buffer[i++] = uByte; // i++ before
            
        }
        if(temp != uByte)
            printf("%x", uByte);
        temp = uByte;
        //if(i == 47)
        //    break;
    }
    // since no padding on the struct, can directly assign values into the frame
    //*Lidar_Data = (LidarFrame *)buffer;
    //free(buffer);
}
void parse_buffer(char *buffer){
        printf("%x", buffer);
}


// now compiling with  arm-linux-gnueabihf-gcc
#endif
