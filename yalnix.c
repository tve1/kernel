#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include <comp421/hardware.h>
#include <comp421/yalnix.h>

#define PMEM_1_BASE VMEM_1_BASE

void* ivt[TRAP_VECTOR_SIZE];

struct node {
  void* base;
  int isRemoved;
  int index;
};

struct pte** page_table;

struct node** physical_pages;
int physical_pages_length;

void* actual_brk;

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

struct node* allocPhysicalPage(){
	if (physical_pages == NULL){
		return NULL;
	}
	int i;
	for (i = 0; i < physical_pages_length; i++){
		if (physical_pages[i]->isRemoved == 0){
			physical_pages[i]->isRemoved = 1;
			return physical_pages[i];
		}
	}
	return NULL;
}

int freePhysicalPage(struct node * nodeIn) {
	physical_pages[nodeIn->index]->isRemoved = 0;
	return 1;
}

void KernelStart (ExceptionStackFrame *frame, unsigned int pmem_size, void *orig_brk, char **cmd_args) {
	actual_brk = orig_brk;
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

	unsigned long current_spot = (unsigned long) orig_brk;
	int num_nodes = (current_spot) / PAGESIZE;
	int num_ptes = (pmem_size - PMEM_BASE) / PAGESIZE;
	physical_pages = malloc(sizeof(struct node*) * num_nodes);
	page_table = malloc(sizeof(struct pte *) * num_ptes);
	
	int h;
	
	for (h = 0; h < num_ptes; h++){
		struct pte * entry = malloc(sizeof(struct pte));
		entry->valid = 0;
		entry->kprot = PROT_NONE;
		entry->uprot = PROT_NONE;
		entry->pfn = h;
		page_table[h] = entry;
	}
	
	int i;

	for (i = 0; i < num_nodes; i++) {
		struct node* new_page = malloc(sizeof(struct node*));
		physical_pages[i] = new_page;
		new_page->index = i;
	}
	
	int num_text_pages = ((unsigned long) &_etext - PMEM_1_BASE) / PAGESIZE;
	
	int j;
	
	int kernel_page_base = PMEM_1_BASE / PAGESIZE;
	
	for (j = kernel_page_base; j < num_text_pages + kernel_page_base; j++){
		page_table[j]->valid = 1;
		page_table[j]->kprot = (PROT_READ | PROT_EXEC);
		printf("j: %d\n", j);
	} 
	
	int num_dbh_pages = ((unsigned long)(actual_brk - (void*) &_etext) / PAGESIZE);

	int k;
	
	for (k = kernel_page_base + num_text_pages; k < kernel_page_base + num_text_pages + num_dbh_pages; k++){
		page_table[k]->valid = 1;
		page_table[k]->kprot = (PROT_READ | PROT_WRITE);
		printf("k: %d\n", k);
	}
	
	current_spot = (unsigned long) &_etext;

	int index = 0;
	
	// Only going one-direction
	while (current_spot - PAGESIZE > 0) {
		physical_pages[index]->base = (void*)current_spot;
		current_spot -= PAGESIZE;
		index++;
	}
	physical_pages_length = index;


	struct node *n = allocPhysicalPage();
	int b = n->index;
	printf("a %d\n", physical_pages[b]->isRemoved);
	freePhysicalPage(n);
	printf("b %d\n", physical_pages[b]->isRemoved);

}





int SetKernelBrk(void *addr){
	printf("Brk\n");
	TracePrintf(0,"kernel break");
	actual_brk = addr;
	return 0;
}

