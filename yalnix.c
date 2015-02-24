#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include <comp421/hardware.h>
#include <comp421/yalnix.h>



void* ivt[TRAP_VECTOR_SIZE];

struct node {
  void* base;
};

struct node** physical_pages;
int physical_pages_length;

void trapKernel(ExceptionStackFrame *frame){
	TracePrintf(0, "trapKernel");
	Halt();
}
void trapClock(ExceptionStackFrame *frame){
	TracePrintf(0, "trapClock");
	Halt();
}
void trapIllegal(ExceptionStackFrame *frame){
	TracePrintf(0, "trapIllegal");
	Halt();
}
void trapMemory(ExceptionStackFrame *frame){
	TracePrintf(0, "trapMemory");
	Halt();
}
void trapMath(ExceptionStackFrame *frame){
	TracePrintf(0, "trapMath");
	Halt();
}
void trapTtyReceive(ExceptionStackFrame *frame){
	TracePrintf(0, "trapTtyReceive");
	Halt();
}
void trapTtyTransmit(ExceptionStackFrame *frame){
	TracePrintf(0, "trapTtyTransmit");
	Halt();
}

// void * allocPhysicalPage(){
// 	if (physical_pages == NULL){
// 		return NULL;
// 	}
// 	void * physicalPageAddr = physical_pages->base;
// 	struct node * tempNode = physical_pages->nextNode;
// 	free(physical_pages);
// 	physical_pages = tempNode;
// 	return physicalPageAddr;
// }

// int freePhysicalPage(void * addr) {
// 	struct node* new_page = malloc(sizeof(struct node*));
// 	new_page->nextNode = physical_pages;
// 	new_page->base = addr;
// 	physical_pages = new_page;
// 	return 1;
// }

void KernelStart (ExceptionStackFrame *frame, unsigned int pmem_size, void *orig_brk, char **cmd_args) {
	void (*ptr_to_kernel)(ExceptionStackFrame *);
	ptr_to_kernel = &trapKernel;
	ivt[TRAP_KERNEL] = ptr_to_kernel;
	void (*ptr_to_clock)(ExceptionStackFrame *);
	ptr_to_clock = &trapClock;
	ivt[TRAP_CLOCK] = ptr_to_clock;
	void (*ptr_to_illegal)(ExceptionStackFrame *);
	ptr_to_illegal = &trapIllegal;
	ivt[TRAP_ILLEGAL] = ptr_to_illegal;
	void (*ptr_to_memory)(ExceptionStackFrame *);
	ptr_to_memory = &trapMemory;
	ivt[TRAP_MEMORY] = ptr_to_memory;
	void (*ptr_to_math)(ExceptionStackFrame *);
	ptr_to_math = &trapMath;
	ivt[TRAP_MATH] = ptr_to_math;
	void (*ptr_to_receive)(ExceptionStackFrame *);
	ptr_to_receive = &trapTtyReceive;
	ivt[TRAP_TTY_RECEIVE] = ptr_to_receive;
	void (*ptr_to_transmit)(ExceptionStackFrame *);
	ptr_to_transmit = &trapTtyTransmit;
	ivt[TRAP_TTY_TRANSMIT] = ptr_to_transmit;
	
	WriteRegister(REG_VECTOR_BASE, (RCS421RegVal) ivt);

	long current_spot = (long) orig_brk;
	
	int num_nodes = (pmem_size - current_spot) / PAGESIZE;
	physical_pages = malloc(sizeof(struct node*) * num_nodes);
	int i;

	for (i = 0; i < num_nodes; i++) {
		struct node* new_page = malloc(sizeof(struct node*));
		physical_pages[i] = new_page;
	}

	current_spot = (long) &_etext;

	int index = 0;
	
	// Only going one-direction
	while (current_spot + PAGESIZE < pmem_size) {
		physical_pages[index]->base = (void*)current_spot;
		current_spot += PAGESIZE;
		index++;
	}
	physical_pages_length = index;
}





int SetKernelBrk(void *addr){
	printf("Brk\n");
	TracePrintf(0,"kernel break");
	_etext = (long)addr;
	return 0;
}

