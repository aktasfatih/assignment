#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <unistd.h>
#include <signal.h>
#include "memlayout.h"

// Storing the current memory access type
static sig_atomic_t current_access, last_access;

// Storing where to return after the signal handlers
static sigjmp_buf jumpbuf;

// For making sure the process controlling this doesn't get affected.
static struct sigaction oldsignalhandle;
static struct sigaction oldsignalbushandle;

// Signal handler for SIGSEGV
static void handler(int signum) {
	siglongjmp(jumpbuf, 1);
}

// Memory scan function
int get_mem_layout (struct memregion *regions, unsigned int size){

	// Saving the old signal
  	sigaction(SIGSEGV, NULL, &oldsignalhandle);
	sigaction(SIGBUS, NULL, &oldsignalbushandle);

	// Assigning new signal handler
  	signal(SIGSEGV, handler);
	signal(SIGBUS, handler);

  	unsigned int *currentAddress = 0; // This is the current address we are in for the search.
  	int readValue; // This is the value in the currentAddress	
	int pageSize = getpagesize(); // Page size of a memory space
	int listIndex = 0; // The current index in regions
	int regionCount = 0;
	int isItFirst = 1; // Active in currentAddress = 0x0
	
	int i;
	int needTimes = ((0xFFFFFFFF - pageSize + 1) / pageSize) + 1; // This is how many times we scan with the pagesize

	for( i = 0; i < needTimes; i++ ){
		// printf("%d, %x\n", i, currentAddress);
		current_access = MEM_NO;
        if ( sigsetjmp(jumpbuf, 1 ) == 0 ){
            // Reading operation
            readValue = *currentAddress;
            current_access = MEM_RO;	

            // Writing operation 
            *currentAddress = readValue;
            current_access = MEM_RW;			
        }	

		// Putting the values into the array
		if(isItFirst){
			regions[listIndex].from = currentAddress;
			regions[listIndex].to = (int*)((int)currentAddress + pageSize - 1);
			regions[listIndex].mode = current_access;
			isItFirst = 0;
			regionCount++;
		}else{
			if(listIndex < size){
				if(current_access != last_access){
					listIndex ++;
					regions[listIndex].from = currentAddress;
					regions[listIndex].to = (int*)((int)currentAddress + pageSize - 1);
					regions[listIndex].mode = current_access;
				}else{
					regions[listIndex].to = (int*)((int)currentAddress + pageSize - 1);
				}
			}
			if(current_access != last_access){
				regionCount++;
			}
		}

		last_access = current_access;
		currentAddress = currentAddress + (pageSize>>2); // It is a int addition to pointer. 
    }

	// Loading the old signal handler
	sigaction(SIGSEGV, &oldsignalhandle, NULL);
	sigaction(SIGSEGV, &oldsignalbushandle, NULL);
    return regionCount;
}

int main()
{
	int size = 100;
	struct memregion *memList = malloc(size * sizeof(struct memregion) );
	
	int regionCounts = get_mem_layout(memList, size);
	
	for(int i = 0; i < size; i++){
		printf("0x%08x - ", (int)memList[i].from);
		printf("0x%08x ", (int)memList[i].to);
		switch(memList[i].mode){
			case MEM_NO:
				printf("NO\n");
				break;
			case MEM_RO:
				printf("RO\n");
				break;
			case MEM_RW:
				printf("RW\n");
				break;
		}
	}
	printf("There are %d regions.", regionCounts);

	// Deallocate regions
	free(memList);
	return 0;
}
