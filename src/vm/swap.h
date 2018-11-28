#ifndef SWAP_TABLE_H
#define SWAP_TABLE_H


struct bitmap* swap_table; 

void swap_read(void *fault_addr); 
void swap_write(struct frame* f); 



// swap write 
/* 

    when user pool is full, 
    takes kvm,uaddr from frame table and writes it to swap 
    uses kvm to find which page of memory to write to swap 

    sets the swap bit in the uaddr to TRUE 
    and makes sure that the next time you want the thing you just stored, 
        you can just get it from looking at uaddr 
       
       
       
       
*/

#endif
