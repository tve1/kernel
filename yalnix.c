#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <comp421/loadinfo.h>
#include <comp421/hardware.h>
#include <comp421/yalnix.h>

#define PMEM_1_BASE VMEM_1_BASE

void* ivt[TRAP_VECTOR_SIZE];

struct node {
	void* base;
	int isRemoved;
	unsigned int pfn;
};

struct pcb {
	SavedContext* ctxp;
	void* region_0_addr;
	int pid;
	struct pcb* next;
	struct pcb* prev;
	ExceptionStackFrame* kernel_stack;
 	struct pte* region_0;
 	int delayTick;
 	int heapTopIndex;
 	int bottomOfHeapIndex;
 	int userStackTopIndex;
 	int userStackBottomIndex;
 	struct exited_node* exited_children_queue;
 	struct pcb* parent_pcb;
};

struct pte_wrapper {
	struct pte* pte;
	int pfn; 
	void* phys_addr;
};

struct exited_node {
	struct exited_node* next;
	int pid; 
	int status; 
};


struct pcb* idle_pcb;
struct pcb* init_pcb;

struct pte** page_table;

struct pte* region_0;
struct pte* region_1;

int num_pages_region_0;
int num_pages_region_1;

struct node** physical_pages;
int physical_pages_length;
int num_free_pages;

void* actual_brk;

struct pcb* cur_pcb;

void* temp;
int tick = 0;
int is_vmem = 0;
int topIndex;
int lastPid;

ExceptionStackFrame *kernel_frame;
unsigned int allocPhysicalPage(){
	if (physical_pages == NULL){
		return -1;
	}
	int i;
	for (i = 0; i < physical_pages_length; i++){
		if (physical_pages[i]->isRemoved == 0){
			physical_pages[i]->isRemoved = 1;
			num_free_pages--;
			TracePrintf(1, "Allocing page %p\n", physical_pages[i]->pfn);

			return physical_pages[i]->pfn;
		}
	}
	return -1;
}

int freePhysicalPage(unsigned int pfn) {
	TracePrintf(1, "Free page %p\n", pfn);
	physical_pages[pfn]->isRemoved = 0;
	num_free_pages++;
	return 1;
}


SavedContext *MySwitchFunc(SavedContext *ctxp, void *p1, void *p2) {
	struct pcb* pcb1 = (struct pcb*) p1;
	struct pcb* pcb2 = (struct pcb*) p2;

	printf("Perfroming a context switch from %d to %d\n", pcb1->pid, pcb2->pid);
	WriteRegister(REG_PTR0, (RCS421RegVal) pcb2->region_0_addr);
	
  	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
	
	cur_pcb = pcb2;

	return cur_pcb->ctxp;
}

SavedContext *MyFirstSwitchFunc(SavedContext *ctxp, void *p1, void *p2) {
	struct pcb* pcb1 = (struct pcb*) p1;
	struct pcb* pcb2 = (struct pcb*) p2;

	printf("Copying from %d to %d\n", pcb1->pid, pcb2->pid);
	
	//pcb2->ctxp = ctxp; 
	int a;
	for (a = 0; a < KERNEL_STACK_PAGES; a++){
		// int pfn1 = pcb1->region_0[a].pfn;
		// int pfn2 = pcb2->region_0[a].pfn;
		// printf ("1 %d 2 %d\n", pfn1, pfn2);
		// pcb2->region_0[a] = pcb1->region_0[a];
		memcpy(temp, (void*) KERNEL_STACK_BASE + a*PAGESIZE, PAGESIZE);
		printf("copy1\n");
		WriteRegister(REG_TLB_FLUSH, KERNEL_STACK_BASE + a*PAGESIZE);
		printf("flush1\n");
		WriteRegister(REG_PTR0, (RCS421RegVal) pcb2->region_0_addr);
		printf("write1\n");
		// printf("%p\n", )
		pcb2->region_0[KERNEL_STACK_BASE/PAGESIZE + a].valid = 1;
		pcb2->region_0[KERNEL_STACK_BASE/PAGESIZE + a].kprot = (PROT_READ | PROT_WRITE) ;
		pcb2->region_0[KERNEL_STACK_BASE/PAGESIZE + a].uprot = (PROT_NONE);
		pcb2->region_0[KERNEL_STACK_BASE/PAGESIZE + a].pfn = allocPhysicalPage();
		memcpy((void*) KERNEL_STACK_BASE + a*PAGESIZE, temp, PAGESIZE);
		printf("copy2\n");
		WriteRegister(REG_TLB_FLUSH, KERNEL_STACK_BASE + a*PAGESIZE);
		printf("flush2\n");
		WriteRegister(REG_PTR0, (RCS421RegVal) pcb1->region_0_addr);
		printf("write2\n");
	}
	printf("fic %p %p\n", ctxp, pcb2->ctxp);
	memcpy(pcb2->ctxp, ctxp, sizeof(SavedContext));
	printf("toria\n");

	return ctxp;
}

void trapKernel(ExceptionStackFrame *frame){     
	TracePrintf(0, "trapKernel\n");
	frame->psr = frame->psr | PSR_MODE;
	
	if (frame->code == YALNIX_GETPID) {
		frame->regs[0] = GetPid();
	}
	else if (frame->code == YALNIX_DELAY) {
		frame->regs[0] = Delay(frame->regs[1]);
		// printf("frame->regs[0] %d\n", frame->regs[0]);
	}
	else if (frame->code == YALNIX_BRK) {
		frame->regs[0] = Brk((void*)frame->regs[1]);
		// printf("frame->regs[0] %d\n", frame->regs[0]);
	}
	else if (frame->code == YALNIX_FORK) {
		frame->regs[0] = Fork();
		// printf("frame->regs[0] %d\n", frame->regs[0]);
	}
	else if(frame->code == YALNIX_EXEC) {
		frame->regs[0] = Exec((char*) frame->regs[1], (char**) frame->regs[2]);
	}
	else if(frame->code == YALNIX_EXIT) {
		Exit((int) frame->regs[1]);
	}
} 

struct pcb* getNextProcess() {
	struct pcb* nextProcess = cur_pcb->next;
	if (nextProcess == NULL) {
		nextProcess = idle_pcb;
	}
	while (nextProcess->delayTick > 0) {
		nextProcess = nextProcess->next;
		if (nextProcess == NULL) {
			nextProcess = idle_pcb;
		}
	}
	return nextProcess;
}

void doAContextSwitch() {
	int ctxtSwitch;
	struct pcb* nextPcb = getNextProcess(); 
	printf("A: cur_pcb %d, ctxp %p, next %d\n", cur_pcb->pid, cur_pcb->ctxp, nextPcb->pid);
	ctxtSwitch = ContextSwitch(MySwitchFunc, cur_pcb->ctxp, (void *)cur_pcb, (void *)nextPcb);

	printf("Switch %d\n", ctxtSwitch);
}



void subtractDelayTicks() {
	struct pcb* temp_pcb = idle_pcb;
	while (temp_pcb != NULL) {
			printf("Delay tics %d\n", temp_pcb->delayTick);
		if (temp_pcb->delayTick > 0) {
			temp_pcb->delayTick--;
		}
		temp_pcb = temp_pcb->next;
	} 
}

void trapClock(ExceptionStackFrame *frame){
	TracePrintf(0,"trapClock %d\n", tick); 
	subtractDelayTicks();
	if (tick == 0)
		tick++;
	else if (tick >= 1){
		doAContextSwitch();
		tick = 0;
	}		
}


void trapIllegal(ExceptionStackFrame *frame){
	TracePrintf(0, "trapIllegal");    
	Halt(); 
} 

void trapMemory(ExceptionStackFrame *frame){     
	TracePrintf(0, "trapMemory %p\n", (void*)kernel_frame->addr);
	// cur_pcb->region_0[(int)frame->addr/PAGESIZE].pfn = allocPhysicalPage();
	void* new_addr = (void*)DOWN_TO_PAGE(frame->addr);
	if ((unsigned long)new_addr < cur_pcb->userStackBottomIndex * PAGESIZE && (unsigned long)new_addr > (cur_pcb->heapTopIndex + 1)* PAGESIZE) {
		printf("Extending user heap to %p\n", new_addr);
		int num_to_alloc = (cur_pcb->userStackBottomIndex * PAGESIZE - (unsigned long)new_addr) / PAGESIZE; 
		printf("Allocating %d pages\n", num_to_alloc);
		int i;
		for (i = 0; i < num_to_alloc; i++) {
			cur_pcb->userStackBottomIndex--;
			int new_pfn = allocPhysicalPage();
			printf("Allocating page %d\n", new_pfn);
			cur_pcb->region_0[cur_pcb->userStackBottomIndex].pfn = new_pfn;
			cur_pcb->region_0[cur_pcb->userStackBottomIndex].valid = 1;
	        cur_pcb->region_0[cur_pcb->userStackBottomIndex].kprot = PROT_READ | PROT_WRITE;
    	    cur_pcb->region_0[cur_pcb->userStackBottomIndex].uprot = PROT_READ | PROT_WRITE;
		}
	}
	else {
		Halt();
	}
} 

void trapMath(ExceptionStackFrame *frame){
	TracePrintf(0, "trapMath\n");     
	// TracePrintf(0, "Process %d recieved trap math. Terminating.\n");     
	
	Halt(); 
} 

void trapTtyReceive(ExceptionStackFrame *frame) {     
	TracePrintf(0, "trapTtyReceive");     
	Halt(); 
} 

void trapTtyTransmit(ExceptionStackFrame *frame) {     
	TracePrintf(0, "trapTtyTransmit");     
	Halt(); 
}

int Delay(int clock_ticks){
	TracePrintf(1, "Delaying for %d ticks\n", clock_ticks);
	if (clock_ticks < 0) {
		return ERROR;
	}
	else if (clock_ticks == 0 ){
		return 0;
	}
	else {
		cur_pcb->delayTick = clock_ticks;
		doAContextSwitch();
		return 0;
	}
}
int GetPid(){
	return cur_pcb->pid;
}

/*
 *  Load a program into the current process's address space.  The
 *  program comes from the Unix file identified by "name", and its
 *  arguments come from the array at "args", which is in standard
 *  argv format.
 *
 *  Returns:
 *      0 on success
 *     -1 on any error for which the current process is still runnable
 *     -2 on any error for which the current process is no longer runnable
 *
 *  This function, after a series of initial checks, deletes the
 *  contents of Region 0, thus making the current process no longer
 *  runnable.  Before this point, it is possible to return ERROR
 *  to an Exec() call that has called LoadProgram, and this function
 *  returns -1 for errors up to this point.  After this point, the
 *  contents of Region 0 no longer exist, so the calling user process
 *  is no longer runnable, and this function returns -2 for errors
 *  in this case.
 */
int
LoadProgram(char *name, char **args, struct pcb* pcb_in)
{
    int fd;
    int status;
    struct loadinfo li;
    char *cp;
    char *cp2;
    char **cpp;
    char *argbuf;
    int i;
    unsigned long argcount;
    int size;
    int text_npg;
    int data_bss_npg;
    int stack_npg;

    TracePrintf(0, "LoadProgram '%s', args %p\n", name, args);

    if ((fd = open(name, O_RDONLY)) < 0) {
	TracePrintf(0, "LoadProgram: can't open file '%s'\n", name);
	return (-1);
    }

    status = LoadInfo(fd, &li);
    TracePrintf(0, "LoadProgram: LoadInfo status %d\n", status);
    switch (status) {
	case LI_SUCCESS:
	    break;
	case LI_FORMAT_ERROR:
	    TracePrintf(0,
		"LoadProgram: '%s' not in Yalnix format\n", name);
	    close(fd);
	    return (-1);
	case LI_OTHER_ERROR:
	    TracePrintf(0, "LoadProgram: '%s' other error\n", name);
	    close(fd);
	    return (-1);
	default:
	    TracePrintf(0, "LoadProgram: '%s' unknown error\n", name);
	    close(fd);
	    return (-1);
    }
    TracePrintf(0, "text_size 0x%lx, data_size 0x%lx, bss_size 0x%lx\n",

	li.text_size, li.data_size, li.bss_size);
    TracePrintf(0, "entry 0x%lx\n", li.entry);

    /*
     *  Figure out how many bytes are needed to hold the arguments on
     *  the new stack that we are building.  Also count the number of
     *  arguments, to become the argc that the new "main" gets called with.
     */
    size = 0;
    for (i = 0; args[i] != NULL; i++) {
	size += strlen(args[i]) + 1;
    }
    argcount = i;
    TracePrintf(0, "LoadProgram: size %d, argcount %d\n", size, argcount);

    /*
     *  Now save the arguments in a separate buffer in Region 1, since
     *  we are about to delete all of Region 0.
     */
    cp = argbuf = (char *)malloc(size);
    for (i = 0; args[i] != NULL; i++) {
	strcpy(cp, args[i]);
	cp += strlen(cp) + 1;
    }
  
    /*
     *  The arguments will get copied starting at "cp" as set below,
     *  and the argv pointers to the arguments (and the argc value)
     *  will get built starting at "cpp" as set below.  The value for
     *  "cpp" is computed by subtracting off space for the number of
     *  arguments plus 4 (for the argc value, a 0 (AT_NULL) to
     *  terminate the auxiliary vector, a NULL pointer terminating
     *  the argv pointers, and a NULL pointer terminating the envp
     *  pointers) times the size of each (sizeof(void *)).  The
     *  value must also be aligned down to a multiple of 8 boundary.
     */
    cp = ((char *)USER_STACK_LIMIT) - size;
    cpp = (char **)((unsigned long)cp & (-1 << 4));	/* align cpp */
    cpp = (char **)((unsigned long)cpp - ((argcount + 4) * sizeof(void *)));

    text_npg = li.text_size >> PAGESHIFT;
    data_bss_npg = UP_TO_PAGE(li.data_size + li.bss_size) >> PAGESHIFT;
    stack_npg = (USER_STACK_LIMIT - DOWN_TO_PAGE(cpp)) >> PAGESHIFT;

    TracePrintf(0, "LoadProgram: text_npg %d, data_bss_npg %d, stack_npg %d\n",
	text_npg, data_bss_npg, stack_npg);

    /*
     *  Make sure we will leave at least one page between heap and stack
     */
    if (MEM_INVALID_PAGES + text_npg + data_bss_npg + stack_npg +
	1 + KERNEL_STACK_PAGES >= PAGE_TABLE_LEN) {
	TracePrintf(0, "LoadProgram: program '%s' size too large for VM\n",
	    name);
	free(argbuf);
	close(fd);
	return (-1);
    }

    /*
     *  And make sure there will be enough physical memory to
     *  load the new program.
     */
    // >>>> The new program will require text_npg pages of text,
    // >>>> data_bss_npg pages of data/bss, and stack_npg pages of
    // >>>> stack.  In checking that there is enough free physical
    // >>>> memory for this, be sure to allow for the physical memory
    // >>>> pages already allocated to this process that will be
    // >>>> freed below before we allocate the needed pages for
    // >>>> the new program being loaded.

    int total = text_npg + data_bss_npg + stack_npg;

    if (num_free_pages < total) {
	TracePrintf(0,
	    "LoadProgram: program '%s' size too large for physical memory\n",
	    name);
	free(argbuf);
	close(fd);
	return (-1);
    }
	TracePrintf(0,
	    "hi");

    // >>>> Initialize sp for the current process to (char *)cpp.
    // >>>> The value of cpp was initialized above.
    kernel_frame->sp = (void*) cpp;



    /*
     *  Free all the old physical memory belonging to this process,
     *  but be sure to leave the kernel stack for this process (which
     *  is also in Region 0) alone.
     */
    // >>>> Loop over all PTEs for the current process's Region 0,
    // >>>> except for those corresponding to the kernel stack (between
    // >>>> address KERNEL_STACK_BASE and KERNEL_STACK_LIMIT).  For
    // >>>> any of these PTEs that are valid, free the physical memory
    // >>>> memory page indicated by that PTE's pfn field.  Set all
    // >>>> of these PTEs to be no longer valid.
    int z;
    for (z = 0; z < num_pages_region_0; z++){
        if (z >= (KERNEL_STACK_BASE / PAGESIZE) && z < (KERNEL_STACK_LIMIT / PAGESIZE)){
            continue;
        }    
        if (pcb_in->region_0[z].valid){
            freePhysicalPage(pcb_in->region_0[z].pfn);
            pcb_in->region_0[z].valid = 0;
        } 
    }
	TracePrintf(0,
	    "hello");
    
    /*
     *  Fill in the page table with the right number of text,
     *  data+bss, and stack pages.  We set all the text pages
     *  here to be read/write, just like the data+bss and
     *  stack pages, so that we can read the text into them
     *  from the file.  We then change them read/execute.
     */

    // >>>> Leave the first MEM_INVALID_PAGES number of PTEs in the
    // >>>> Region 0 page table unused (and thus invalid)MP

    int a;
    int bottom = 0;
    int top = MEM_INVALID_PAGES;

    for (a = bottom; a < top; a++) {
        pcb_in->region_0[a].valid = 0;
    }
    bottom = top;
    top += text_npg;
	TracePrintf(0,
	    "hey");

    /* First, the text pages */
    // >>>> For the next text_npg number of PTEs in the Region 0
    // >>>> page table, initialize each PTE:
    // >>>>     valid = 1
    // >>>>     kprot = PROT_READ | PROT_WRITE
    // >>>>     uprot = PROT_READ | PROT_EXEC
    // >>>>     pfn   = a new page of physical memory

    int b;

    for (b = bottom; b < top; b++){
        pcb_in->region_0[b].valid = 1;
        pcb_in->region_0[b].kprot = PROT_READ | PROT_WRITE;
        pcb_in->region_0[b].uprot = PROT_READ | PROT_EXEC;
        pcb_in->region_0[b].pfn   = allocPhysicalPage();
    }
    bottom = top;
    top += data_bss_npg;
	pcb_in->bottomOfHeapIndex = bottom;
    pcb_in->heapTopIndex = top - 1;

	TracePrintf(0,
	    "howdy");
    /* Then the data and bss pages */
    // >>>> For the next data_bss_npg number of PTEs in the Region 0
    // >>>> page table, initialize each PTE:
    // >>>>     valid = 1
    // >>>>     kprot = PROT_READ | PROT_WRITE
    // >>>>     uprot = PROT_READ | PROT_WRITE
    // >>>>     pfn   = a new page of physical memory

    int c;

    for(c = bottom; c < top; c++){
        pcb_in->region_0[c].valid = 1;
        pcb_in->region_0[c].kprot = PROT_READ | PROT_WRITE;
        pcb_in->region_0[c].uprot = PROT_READ | PROT_WRITE;
        pcb_in->region_0[c].pfn   = allocPhysicalPage();
    }
    bottom = top;
    top += stack_npg;

	TracePrintf(0,
	    "yo");

    /* And finally the user stack pages */
    // >>>> For stack_npg number of PTEs in the Region 0 page table
    // >>>> corresponding to the user stack (the last page of the
    // >>>> user stack *ends* at virtual address USER_STACK_LMIT),
    // >>>> initialize each PTE:
    // >>>>     valid = 1
    // >>>>     kprot = PROT_READ | PROT_WRITE
    // >>>>     uprot = PROT_READ | PROT_WRITE
    // >>>>     pfn   = a new page of physical memory
	int user_stack_page = USER_STACK_LIMIT/PAGESIZE;
    int d;
    for (d = user_stack_page - stack_npg; d < user_stack_page; d++){
        pcb_in->region_0[d].valid = 1;
        pcb_in->region_0[d].kprot = PROT_READ | PROT_WRITE;
        pcb_in->region_0[d].uprot = PROT_READ | PROT_WRITE;
        pcb_in->region_0[d].pfn   = allocPhysicalPage();
    }
    pcb_in->userStackBottomIndex = user_stack_page - stack_npg;
    pcb_in->userStackTopIndex = user_stack_page - 1;

	TracePrintf(0,
	    "flushing %p %p\n", (void*) REG_TLB_FLUSH, (void*) TLB_FLUSH_0);

    /*
     *  All pages for the new address space are now in place.  Flush
     *  the TLB to get rid of all the old PTEs from this process, so
     *  we'll be able to do the read() into the new pages below.
     */

    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
TracePrintf(0,
	    "flushed\n");
    /*
     *  Read the text and data from the file into memory.
     */
    if (read(fd, (void *)MEM_INVALID_SIZE, li.text_size+li.data_size)
	!= li.text_size+li.data_size) {
	TracePrintf(0, "LoadProgram: couldn't read for '%s'\n", name);
	free(argbuf);
	close(fd);
	// >>>> Since we are returning -2 here, this should mean to
	// >>>> the rest of the kernel that the current process shouldz
	// >>>> be terminated with an exit status of ERROR reported
	// >>>> to its parent process.
    //come back to it later????
	return (-2);
    }

    close(fd);			/* we've read it all now */

    /*
     *  Now set the page table entries for the program text to be readable
     *  and executable, but not writable.
     */
    // >>>> For text_npg number of PTEs corresponding to the user text
    // >>>> pages, set each PTE's kprot to PROT_READ | PROT_EXEC.
	TracePrintf(0,
	    "ez\n");

    int e;
    for (e = MEM_INVALID_PAGES; e < MEM_INVALID_PAGES + text_npg; e++){
        pcb_in->region_0[e].kprot = PROT_READ | PROT_EXEC;
    }
	TracePrintf(0,
	    "e\n");

    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

    /*
     *  Zero out the bss
     */
    memset((void *)(MEM_INVALID_SIZE + li.text_size + li.data_size),
	'\0', li.bss_size);

    /*
     *  Set the entry point in the exception frame.
     */
    // >>>> Initialize pc for the current process to (void *)li.entry
    kernel_frame->pc = li.entry;
	    TracePrintf(0, "starting pc is %p\n", kernel_frame->pc);
    /*
     *  Now, finally, build the argument list on the new stack.
     */
	TracePrintf(0,
	    "0 %p %p\n", (char*) cpp, (char*) argcount);
    *cpp++ = (char *)argcount;		/* the first value at cpp is argc */
	TracePrintf(0,
	    "1\n");
    cp2 = argbuf;
    for (i = 0; i < argcount; i++) {      /* copy each argument and set argv */
		TracePrintf(0,
	    "2\n");
	*cpp++ = cp;
		TracePrintf(0,
	    "3\n");

	strcpy(cp, cp2);
	TracePrintf(0,
	    "4\n");
	cp += strlen(cp) + 1;
		TracePrintf(0,
	    "5\n");

	cp2 += strlen(cp2) + 1;
	TracePrintf(0,
	    "6\n");

    }
    	TracePrintf(0,
	    "7\n");
	free(argbuf);
	TracePrintf(0,
	    "8\n");

    *cpp++ = NULL;	/* the last argv is a NULL pointer */
    	TracePrintf(0,
	    "9\n");

    *cpp++ = NULL;	/* a NULL pointer for an empty envp */
	TracePrintf(0,
	    "10\n");
    *cpp++ = 0;		/* and terminate the auxiliary vector */
	TracePrintf(0,
	    "i bet this won't run\n");

    /*
     *  Initialize all regs[] registers for the current process to 0,
     *  initialize the PSR for the current process also to 0.  This
     *  value for the PSR will make the process run in user mode,
     *  since this PSR value of 0 does not have the PSR_MODE bit set.
     */
    // >>>> Initialize regs[0] through regs[NUM_REGS-1] for the
    // >>>> current process to 0.
    // >>>> Initialize psr for the current process to 0.

    int f;
    for (f = 0; f < NUM_REGS; f++){
        kernel_frame->regs[f] = 0;
    }
    kernel_frame->psr = 0;  
	TracePrintf(0,
	    "f");

    return (0);
}

void KernelStart (ExceptionStackFrame *frame, unsigned int pmem_size, void *orig_brk, char **cmd_args) {
	actual_brk = orig_brk;
	kernel_frame = frame;

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

	unsigned long current_spot = (unsigned long) VMEM_0_LIMIT;
	int num_nodes = (current_spot) / PAGESIZE;
	//int num_nodes = 1023;
	int num_ptes = (pmem_size - PMEM_BASE) / PAGESIZE;
	physical_pages = malloc(sizeof(struct node*) * num_nodes);
	page_table = malloc(sizeof(struct pte *) * num_ptes);
	
	num_pages_region_0 = (VMEM_0_LIMIT - VMEM_0_BASE ) / PAGESIZE;
	region_0 = malloc(sizeof(struct pte) * num_pages_region_0);
	struct pte* region_0_idle = malloc(sizeof(struct pte) * num_pages_region_0);

	num_pages_region_1 = (VMEM_1_LIMIT - VMEM_1_BASE ) / PAGESIZE;
	topIndex = num_pages_region_1 - 1; 
	region_1 = malloc(sizeof(struct pte) * num_pages_region_1);

	char** idleArgs = malloc(sizeof(char*));
	char** initArgs = malloc(sizeof(char*));
	idle_pcb = malloc(sizeof(struct pcb));
	init_pcb = malloc(sizeof(struct pcb));
	idle_pcb->ctxp = malloc(sizeof(SavedContext));
	init_pcb->ctxp = malloc(sizeof(SavedContext));
	ExceptionStackFrame* idle_stack = malloc(sizeof(ExceptionStackFrame));
	ExceptionStackFrame* init_stack = malloc(sizeof(ExceptionStackFrame));
	temp = malloc(PAGESIZE);
	idle_pcb->kernel_stack = idle_stack;
	init_pcb->kernel_stack = init_stack;
	idle_pcb->region_0 = region_0_idle;
	init_pcb->region_0 = region_0;
	idle_pcb->region_0_addr = region_0_idle;
	idle_pcb->pid = 0;
	idle_pcb->delayTick = 0;
	init_pcb->delayTick = 0;
	init_pcb->parent_pcb = NULL;
	idle_pcb->parent_pcb = NULL;
	init_pcb->exited_children_queue = NULL;
	idle_pcb->exited_children_queue = NULL;

	int h;
	
	for (h = 0; h < num_ptes; h++){
		struct pte * entry = malloc(sizeof(struct pte));
		entry->valid = 1;
		entry->kprot = PROT_NONE;
		entry->uprot = PROT_NONE;
		entry->pfn = h;
		page_table[h] = entry;
	}
	
	int i;

	for (i = 0; i < num_nodes; i++) {
		struct node* new_page = malloc(sizeof(struct node*));
		physical_pages[i] = new_page;
		new_page->pfn = i;
	}

	int k;

	for (k = 0; k < num_pages_region_0; k++) {
		struct pte entry;
		entry.valid = 1;
		entry.kprot = (PROT_READ | PROT_WRITE) ;
		entry.uprot = (PROT_NONE);
		entry.pfn = k;
		region_0[k] = entry;

		struct pte entry_idle;
		entry_idle.valid = 0;
		// entry_init.kprot = (PROT_READ | PROT_WRITE) ;
		// entry_init.uprot = (PROT_NONE);
		// entry_init.pfn = -1;
		// printf("init pfn: %d\n", entry_init.pfn);
		region_0_idle[k] = entry_idle;
	}

	int a;

	for (a = 0; a < MEM_INVALID_PAGES; a++) {
		region_0[a].valid = 0;
		region_0_idle[a].valid = 0;

	}
	int m;

	for (m = 0; m < num_pages_region_1; m++) {
		struct pte entry;
		entry.valid = 0;
		entry.kprot = PROT_NONE;
		entry.uprot = PROT_NONE;
		entry.pfn = m + num_pages_region_0;
		region_1[m] = entry;
	}
	
	int num_text_pages = ((unsigned long) &_etext - PMEM_1_BASE) / PAGESIZE;
	
	int j;
	
	int kernel_page_base = num_pages_region_0;
	
	for (j = kernel_page_base; j < num_text_pages + kernel_page_base; j++){
		page_table[j]->valid = 1;
		page_table[j]->kprot = (PROT_READ | PROT_EXEC);

		struct pte entry = region_1[j - kernel_page_base];
		entry.valid = 1;
		entry.kprot = (PROT_READ | PROT_EXEC);
		region_1[j - kernel_page_base] = entry;
		printf("index: %d, pfn: %d\n", j-kernel_page_base, entry.pfn);
	}

	
	int num_dbh_pages = ((unsigned long)(actual_brk - (void*) &_etext) / PAGESIZE);

	int n;
	printf("dbh: %d\n", num_dbh_pages);
	
	for (n = kernel_page_base + num_text_pages; n < kernel_page_base + num_text_pages + num_dbh_pages; n++){
		page_table[n]->valid = 1;
		page_table[n]->kprot = (PROT_READ | PROT_WRITE);
		struct pte entry = region_1[n - kernel_page_base];
		entry.valid = 1;
		entry.kprot = (PROT_READ | PROT_WRITE);
		region_1[n - kernel_page_base] = entry;
		printf("index: %d, pfn: %d\n", n-kernel_page_base, entry.pfn);
	}

	current_spot = (unsigned long) 0;

	int pfn = 0;
	
	// Only going one-direction
	while (pfn < num_nodes) {
		physical_pages[pfn]->base = (void*)current_spot;
		current_spot += PAGESIZE;
		pfn++;
	}
	physical_pages_length = pfn;
	printf("physical pages: %d\n", physical_pages_length);
	num_free_pages = physical_pages_length;
	
	WriteRegister(REG_PTR0, (RCS421RegVal) region_0);
	WriteRegister(REG_PTR1, (RCS421RegVal) region_1);

	//ENABLING VIRTUAL MEMORY!!!!!
	WriteRegister(REG_VM_ENABLE, 1);
	is_vmem = 1;

TracePrintf(0,
	    "Virtual memory enabled\n");
	printf("WE STARTED!!!\n");
	
	idleArgs[0] = NULL; 
	initArgs[0] = NULL; 
	printf("Loading init...\n");

	LoadProgram("init", initArgs, init_pcb);
	
	init_pcb->region_0_addr = region_0;
	init_pcb->pid = 1;
	lastPid = 1;
	init_pcb->next = NULL;


	printf("Loading idle...\n");

	region_0 = region_0_idle;

	// WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
	// WriteRegister(REG_PTR0, (RCS421RegVal) region_0_init);
	
	cur_pcb = init_pcb;
	
	ContextSwitch(MyFirstSwitchFunc, init_pcb->ctxp, (void *)init_pcb, (void *)idle_pcb);
	LoadProgram("idle", idleArgs, idle_pcb);
	
	
	idle_pcb->next = init_pcb;
	idle_pcb->prev = NULL;
	init_pcb->prev = idle_pcb;
	// LoadProgram("idle", idleArgs, idle_stack);
	
	// idle_pcb->region_0_addr = region_0;
	// idle_pcb->pid = 0;
	// idle_pcb->next = init_pcb;
	
	
	printf("Finished loading!!!!\n");
}

struct pte_wrapper  allocNewRegion0() {
	//use second half 
	//else
	if ((void*) ( (unsigned long) topIndex*PAGESIZE+VMEM_1_BASE) < actual_brk){
		struct pte_wrapper err_wrapper;
		err_wrapper.pfn = -1;
		return err_wrapper;
	}
	int pfn = allocPhysicalPage();

	region_1[topIndex].valid = 1;
	region_1[topIndex].pfn = pfn;
	region_1[topIndex].kprot = (PROT_READ | PROT_WRITE);
	
	struct pte_wrapper wrapper;
	wrapper.pfn = pfn;
	wrapper.phys_addr = (void*) ((unsigned long) wrapper.pfn * PAGESIZE);
	struct pte* new_region0 = (struct pte*) ((unsigned long)VMEM_1_BASE + (unsigned long)topIndex *PAGESIZE);
	wrapper.pte = new_region0;
	topIndex--;
	return wrapper;
}



int Fork() {
	TracePrintf(0, "Fork\n");
	struct pte_wrapper wrapper = allocNewRegion0();
	if (wrapper.pfn == -1){
		return ERROR;
	}
	struct pte* child_region0 = wrapper.pte; 

	struct pcb* child_pcb = malloc(sizeof(struct pcb));
	child_pcb->region_0_addr = wrapper.phys_addr;
	child_pcb->pid = ++lastPid;
	child_pcb->region_0 = child_region0;
	child_pcb->delayTick = 0;
	child_pcb->heapTopIndex = cur_pcb->heapTopIndex;
	child_pcb->bottomOfHeapIndex = cur_pcb->bottomOfHeapIndex;
	child_pcb->userStackTopIndex = cur_pcb->userStackTopIndex;
	child_pcb->userStackBottomIndex = cur_pcb->userStackBottomIndex;
	child_pcb->ctxp = malloc(sizeof(SavedContext));
	child_pcb->exited_children_queue = NULL;
	child_pcb->parent_pcb = cur_pcb;




	int i;
	for (i = 0; i < num_pages_region_0 - KERNEL_STACK_PAGES; i++){
		child_region0[i].valid = cur_pcb->region_0[i].valid;
		child_region0[i].kprot = cur_pcb->region_0[i].kprot;
		child_region0[i].uprot = cur_pcb->region_0[i].uprot;

		if (cur_pcb->region_0[i].valid) {  
			child_pcb->region_0[i].pfn = allocPhysicalPage();
			child_region0[i].kprot = (PROT_WRITE | PROT_READ);

			memcpy(temp, (void*) VMEM_0_BASE + i*PAGESIZE, PAGESIZE);
			WriteRegister(REG_TLB_FLUSH, VMEM_0_BASE + i*PAGESIZE);
			WriteRegister(REG_PTR0, (RCS421RegVal) child_pcb->region_0_addr);

			memcpy((void*) VMEM_0_BASE + i*PAGESIZE, temp, PAGESIZE);
			WriteRegister(REG_TLB_FLUSH, VMEM_0_BASE + i*PAGESIZE);
			WriteRegister(REG_PTR0, (RCS421RegVal) cur_pcb->region_0_addr);
		}		

	}
	struct pcb* temp = cur_pcb->next;
	cur_pcb->next = child_pcb;
	child_pcb->prev = cur_pcb;
	child_pcb->next = temp;
	ContextSwitch(MyFirstSwitchFunc, cur_pcb->ctxp, (void *)cur_pcb, (void *)child_pcb);
		
	if (cur_pcb->pid == child_pcb->pid){
		return child_pcb->pid;
	}

	return 0;
}

int Wait(int* status_ptr) {
	// TODO after ball pit
	return -1;
}

void Exit(int status) {
	TracePrintf(2, "exiting child %d\n", cur_pcb->pid);
		
	if (cur_pcb->parent_pcb != NULL) {
		struct exited_node* exited_node= malloc(sizeof(struct exited_node));
		exited_node->status = status;
		exited_node->pid = cur_pcb->pid;
		exited_node->next = NULL;
		struct exited_node* last_node = cur_pcb->parent_pcb->exited_children_queue;
		if (last_node == NULL) {
			cur_pcb->parent_pcb->exited_children_queue = exited_node;
		}
		else {
			while (last_node->next != NULL) {
				last_node = last_node->next;
			}
			last_node->next = exited_node;
		}

	}	
	struct pcb* p = cur_pcb->prev;
	struct pcb* n = cur_pcb->next;	
	p->next = n;
	n->prev = p;

	free(cur_pcb->ctxp);
	// free(cur_pcb->region_0) Do optimizaiton stuff here.
	while (cur_pcb->exited_children_queue != NULL) {
		struct exited_node* next = cur_pcb->exited_children_queue->next;
		free(cur_pcb->exited_children_queue);
		cur_pcb->exited_children_queue = next;
	}
	free(cur_pcb);
	doAContextSwitch();	

}

int Exec(char* filename, char **argvec){
	TracePrintf(0, "executing %s\n", filename);
	return LoadProgram(filename, argvec, cur_pcb);

}

int Brk(void *addr) {
	TracePrintf(0, "Doing Brk %p\n", addr);
	void* new_addr = (void*) UP_TO_PAGE(addr);
	void* cur_top_addr = (void*)(((unsigned long)cur_pcb->heapTopIndex + 1)* PAGESIZE);
	if ((unsigned long)new_addr < cur_pcb->bottomOfHeapIndex * PAGESIZE) {
		printf("error\n");
		return ERROR;
	}
	else if ((unsigned long)new_addr > (cur_pcb->userStackBottomIndex - 1) * PAGESIZE) {
		return ERROR;
	}
	else if (new_addr == cur_top_addr) {
		return 0;
	}
	else {
		if (new_addr < cur_top_addr) {
			int num_to_free = (cur_top_addr - new_addr) / PAGESIZE;
			int j;
			for (j = 0; j < num_to_free; j++) {
				int pfnToFree = cur_pcb->region_0[cur_pcb->heapTopIndex].pfn;
				freePhysicalPage(pfnToFree);
				cur_pcb->heapTopIndex--;

			}
		}
		else {
			int num_to_alloc = (new_addr - cur_top_addr) / PAGESIZE;
			int i;
			for (i = 0; i < num_to_alloc; i++) {
				cur_pcb->heapTopIndex++;
				int newPage = allocPhysicalPage();
				if (newPage == -1) {
					return ERROR;
				}
				cur_pcb->region_0[cur_pcb->heapTopIndex].pfn = newPage;
				cur_pcb->region_0[cur_pcb->heapTopIndex].valid = 1;
		        cur_pcb->region_0[cur_pcb->heapTopIndex].kprot = PROT_READ | PROT_WRITE;
    			cur_pcb->region_0[cur_pcb->heapTopIndex].uprot = PROT_READ | PROT_WRITE;

			}
		}
	}		
	return 0;
}


int SetKernelBrk(void *addr){
	printf("Brk\n");
	TracePrintf(0,"kernel break\n");

	if (addr > (void*) VMEM_1_LIMIT || addr < (void*) VMEM_1_BASE){
		TracePrintf(0, "addr > VMEM_1_LIMIT\n");
		return -1;
	}
	if (is_vmem) {
		void* new_brk =	new_brk = (void*) UP_TO_PAGE(addr);
		if (new_brk < actual_brk || (void*) ((unsigned long)topIndex*PAGESIZE+VMEM_1_BASE) < new_brk){
			return ERROR;
		}
		else if (new_brk > actual_brk){
			
			int num_to_alloc = (new_brk - actual_brk) / PAGESIZE;
			int j;
			for (j=0; j < num_to_alloc; j++) {
				int index = ((unsigned long) (actual_brk) / PAGESIZE + j ) - num_pages_region_0; 
				struct pte entry = region_1[index];
				entry.valid = 1;
				entry.kprot = (PROT_READ | PROT_WRITE);
				region_1[index] = entry;
			}
			actual_brk = new_brk;
		}
	}
	else {
		actual_brk = addr;
	}
	return 0;
}

