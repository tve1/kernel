#include stdio.h
#include stdlib.h
#include string.h

#include <comp421/hardware.h>
#include <comp421/yalnix.h>

int SetKernelBrk(void *addr){
	TracePrintf(0,"kernel break");
}

int* ivt[TRAP_VECTOR_SIZE];

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

void KernelStart (ExceptionStackFrame *frame, unsigned int pmem_size, void *orig_brk, char **cmd_args) {
	void (*ptr_to_kernel)(int);
	ptr_to_kernel = &trapKernel;
	ivt[TRAP_KERNEL] = ptr_to_kernel;
	void (*ptr_to_clock)(int);
	ptr_to_clock = &trapClock;
	ivt[TRAP_CLOCK] = ptr_to_clock;
	void (*ptr_to_illegal)(int);
	ptr_to_illegal = &trapIllegal;
	ivt[TRAP_ILLEGAL] = ptr_to_illegal;
	void (*ptr_to_memory)(int);
	ptr_to_memory = &trapMemory;
	ivt[TRAP_MEMORY] = ptr_to_memory;
	void (*ptr_to_math)(int);
	ptr_to_math = &trapMath;
	ivt[TRAP_MATH] = ptr_to_math;
	void (*ptr_to_receive)(int);
	ptr_to_receive = &trapTtyReceive;
	ivt[TRAP_TTY_RECEIVE] = ptr_to_receive;
	void (*ptr_to_transmit)(int);
	ptr_to_transmit = &trapTtyTransmit;
	ivt[TRAP_TTY_TRANSMIT] = ptr_to_transmit;

}



// int main(){
	// return 0;
// }