/**
 * virtmem.c 
 */

#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TLB_SIZE 16
#define PAGES 1024
#define PAGE_MASK 1023/* TODO */

#define PAGE_SIZE 1024
#define OFFSET_BITS 10
#define OFFSET_MASK 1023/* TODO */

//We added page_frames as it is not same with pages anymore
#define PAGE_FRAMES 256

#define MEMORY_SIZE PAGE_FRAMES * PAGE_SIZE //updated the memory size accordingly

// Max number of characters per line of input file to read.
#define BUFFER_SIZE 10

struct tlbentry {
  unsigned int logical;
  unsigned int physical;
};

// TLB is kept track of as a circular array, with the oldest element being overwritten once the TLB is full.
struct tlbentry tlb[TLB_SIZE];
// number of inserts into TLB that have been completed. Use as tlbindex % TLB_SIZE for the index of the next TLB line to use.
int tlbindex = 0;

// pagetable[logical_page] is the physical page number for logical page. Value is -1 if that logical page isn't yet in the table.
int pagetable[PAGES];

signed char main_memory[MEMORY_SIZE];

// Pointer to memory mapped backing file
signed char *backing;

int max(int a, int b)
{
  if (a > b)
    return a;
  return b;
}

/* Returns the physical address from TLB or -1 if not present. */
int search_tlb(unsigned int logical_page) {
    /* TODO */
   //goes over the elements of the TLB and if it finds the desired page in TLB, return the physical frame of it
  // otherwise returns 0-1indicating the desired logical page is not in TLB
  int x = max((tlbindex - TLB_SIZE), 0);
  for (int i = x; i < tlbindex; i++) {
    struct tlbentry *current = &tlb[i % TLB_SIZE];
    
    if (current->logical == logical_page) {
      return current->physical;
    }
    x = max((tlbindex - TLB_SIZE), 0);
  }
  return -1;  
}

/* Adds the specified mapping to the TLB, replacing the oldest mapping (FIFO replacement). */
void add_to_tlb(unsigned int logical, unsigned int physical) {
    /* TODO */
  //creates a tlbentry, adds it to tlb by
  //setting the appropriate tlb entry's attributes (logical and physical) to the given ones
  //also increments the tlbindex as one more insert into TLB is completed.
  struct tlbentry *current = &tlb[tlbindex % TLB_SIZE];
  tlbindex++;
  current->logical = logical;
  current->physical = physical;
    
}
//inputs function that takes the inputs from command line and sets the page replacement policy
void inputs(int argc, char **argv, int *p) {
  int opt = getopt(argc, argv, "p:");
    while (opt != -1) {
        switch (opt) {
        case 'p':
            *p = atoi(optarg);
            printf("Page Replacement Policy Number:%d\n",*p);
            return;
        default:
            fprintf(stderr, "Usage: %s [-t nsecs] [-n] name\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
        
    }
}

int main(int argc, const char *argv[])
{
//This part is commented out as the argc will be greater than 3 when specifying -p
  /*if (argc != 3) {
    fprintf(stderr, "Usage ./virtmem backingstore input\n");
    exit(1);
  }*/
  
  //Taking inputs for policy
  int p = 0;
  inputs(argc, argv, &p);
  //setting the counting table values to all 0
  int counting[PAGES] = {0};
  
  const char *backing_filename = argv[1]; 
  int backing_fd = open(backing_filename, O_RDONLY);
  backing = mmap(0, MEMORY_SIZE, PROT_READ, MAP_PRIVATE, backing_fd, 0); 
  
  const char *input_filename = argv[2];
  FILE *input_fp = fopen(input_filename, "r");
  
  // Fill page table entries with -1 for initially empty table.
  int i;
  for (i = 0; i < PAGES; i++) {
    pagetable[i] = -1;
  }
  
  // Character buffer for reading lines of input file.
  char buffer[BUFFER_SIZE];
  
  // Data we need to keep track of to compute stats at end.
  int total_addresses = 0;
  int tlb_hits = 0;
  int page_faults = 0;
  
  // Number of the next unallocated physical page in main memory
  unsigned int free_page = 0;
  
  while (fgets(buffer, BUFFER_SIZE, input_fp) != NULL) {
    total_addresses++;
    int logical_address = atoi(buffer);

    /* TODO 
    / Calculate the page offset and logical page number from logical_address */
    //set offset to logical address & OFFSET_MASK, so that it will represent the next offset
   //set logical_page to (logical_address >> OFFSET_BITS) & PAGE_MASK, so that it will represent the next logical_page
    int offset = logical_address & OFFSET_MASK;
    int logical_page = (logical_address >> OFFSET_BITS) & PAGE_MASK;
    ///////
    
    //if the page replacement policy is LRU, we used total addresses as a counting number in counting table
    if(p == 1) { 
      counting[logical_page] = total_addresses;
    }
    
    int physical_page = search_tlb(logical_page);
    // TLB hit
    if (physical_page != -1) {
      tlb_hits++;
      // TLB miss
    } else {
      physical_page = pagetable[logical_page];
      
      // Page fault
      if (physical_page == -1) {
          /* TODO */
          //First, increase the page_faults number by 1
          page_faults++;
          if(p==0) {
          //FIFO
          //If FIFO, then, set physical_page to next unallocated physical page and 
          //increment the free_page so that it will represent the next unallocated physical page
          physical_page = free_page;
	  free_page = (free_page + 1) % PAGE_FRAMES;
	   
	   //LRU
	   } else if (p==1) {
	   //until the memory fills up for the first time, places pages in the empty page frames just like in FIFO
		  if(page_faults <= PAGE_FRAMES) {
		   	physical_page = page_faults - 1;
		  } else {
		//once it is filled up, select the victim page by choosing the least recently accessed one. (Do this by checking the counting table)
		//at the end update the counting table
			  int i;
			  int minIndex = 0;
			  int minValue = 0;
			  for(i = 0; i < PAGES; i++) {
			    if(pagetable[i] != -1) {
			      if(minValue == 0 || minValue > counting[i]) {
				minIndex = i;
				minValue = counting[i];
				
			      }
			    }
			  }
			physical_page = pagetable[minIndex]; 
		     }
          }
          //copy the page from physical to logical_page
         memcpy(main_memory + physical_page * PAGE_SIZE, backing + logical_page * PAGE_SIZE, PAGE_SIZE);
	 for(int i = 0; i < PAGES; i++){
	 	if(pagetable[i] == physical_page){
	 		pagetable[i] = -1;
	 	}
	 }
	 //update the page table accordingly
	 pagetable[logical_page] = physical_page;       
      }
      add_to_tlb(logical_page, physical_page);
    }
    
    int physical_address = (physical_page << OFFSET_BITS) | offset;
    signed char value = main_memory[physical_page * PAGE_SIZE + offset];
    
    printf("Virtual address: %d Physical address: %d Value: %d\n", logical_address, physical_address, value);
  }
  
  printf("Number of Translated Addresses = %d\n", total_addresses);
  printf("Page Faults = %d\n", page_faults);
  printf("Page Fault Rate = %.3f\n", page_faults / (1. * total_addresses));
  printf("TLB Hits = %d\n", tlb_hits);
  printf("TLB Hit Rate = %.3f\n", tlb_hits / (1. * total_addresses));
  
  return 0;
}
