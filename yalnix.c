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
int LoadProgram(char *name, char **args, ExceptionStackFrame* stackIn);

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
	ExceptionStackFrame* kernel_stack;
 	struct pte* region_0;
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

struct pte temp[KERNEL_STACK_PAGES];

ExceptionStackFrame *kernel_frame;

SavedContext *MySwitchFunc(SavedContext *ctxp, void *p1, void *p2) {
	struct pcb* pcb1 = (struct pcb*) p1;
	struct pcb* pcb2 = (struct pcb*) p2;

	printf("Perfroming a context switch from %d to %d\n", pcb1->pid, pcb2->pid);
	
  	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
	WriteRegister(REG_PTR0, (RCS421RegVal) pcb2->region_0_addr);
	
	cur_pcb = pcb2;

	return cur_pcb->ctxp;
}

SavedContext *MyFirstSwitchFunc(SavedContext *ctxp, void *p1, void *p2) {
	struct pcb* pcb1 = (struct pcb*) p1;
	struct pcb* pcb2 = (struct pcb*) p2;

	printf("Copying from %d to %d\n", pcb1->pid, pcb2->pid);
	
	//pcb2->ctxp = ctxp; 
	int a;
	for (a = KERNEL_STACK_BASE / PAGESIZE; a < KERNEL_STACK_LIMIT/ PAGESIZE; a++){
		int pfn1 = pcb1->region_0[a].pfn;
		int pfn2 = pcb2->region_0[a].pfn;
		printf ("1 %d 2 %d\n", pfn1, pfn2);
		pcb2->region_0[a] = pcb1->region_0[a];
		
	}

	memcpy(pcb2->ctxp, ctxp, sizeof(SavedContext));


	return ctxp;
}

void trapKernel(ExceptionStackFrame *frame){     
	TracePrintf(0, "trapKernel\n");
	frame->psr = frame->psr | PSR_MODE;
	
	if (frame->code == YALNIX_GETPID) {
		frame->regs[0] = GetPid();
	}
	
} 

void trapClock(ExceptionStackFrame *frame){
	TracePrintf(0,"trapClock"); 
// TODO: Do every 2 ticks
	
	printf("cur_pcb %d, ctxp %p, next %d\n", cur_pcb->pid, cur_pcb->ctxp, cur_pcb->next->pid);
	int ctxtSwitch = ContextSwitch(MySwitchFunc, cur_pcb->ctxp, (void *)cur_pcb, (void *)cur_pcb->next);
	printf("Switch %d\n", ctxtSwitch);
}


void trapIllegal(ExceptionStackFrame *frame){
	TracePrintf(0, "trapIllegal");    
	Halt(); 
} 

void trapMemory(ExceptionStackFrame *frame){     
	TracePrintf(0, "trapMemory %p\n", (void*)kernel_frame->pc);
	Halt(); 
} 

void trapMath(ExceptionStackFrame *frame){
	TracePrintf(0, "trapMath");     
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

unsigned int allocPhysicalPage(){
	if (physical_pages == NULL){
		return -1;
	}
	int i;
	for (i = 0; i < physical_pages_length; i++){
		if (physical_pages[i]->isRemoved == 0){
			physical_pages[i]->isRemoved = 1;
			num_free_pages--;
			TracePrintf(1, "Allocing ppage %p\n", physical_pages[i]->pfn);
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

int GetPid(){
	return cur_pcb->pid;
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
	int num_ptes = (pmem_size - PMEM_BASE) / PAGESIZE;
	physical_pages = malloc(sizeof(struct node*) * num_nodes);
	page_table = malloc(sizeof(struct pte *) * num_ptes);
	
	num_pages_region_0 = (VMEM_0_LIMIT - VMEM_0_BASE ) / PAGESIZE;
	region_0 = malloc(sizeof(struct pte) * num_pages_region_0);
	struct pte* region_0_init = malloc(sizeof(struct pte) * num_pages_region_0);

	num_pages_region_1 = (VMEM_1_LIMIT - VMEM_1_BASE ) / PAGESIZE;
	region_1 = malloc(sizeof(struct pte) * num_pages_region_1);

	char** idleArgs = malloc(sizeof(char*));
	idle_pcb = malloc(sizeof(struct pcb));
	init_pcb = malloc(sizeof(struct pcb));
	idle_pcb->ctxp = malloc(sizeof(SavedContext));
	init_pcb->ctxp = malloc(sizeof(SavedContext));
	ExceptionStackFrame* idle_stack = malloc(sizeof(ExceptionStackFrame));
	ExceptionStackFrame* init_stack = malloc(sizeof(ExceptionStackFrame));
	idle_pcb->kernel_stack = idle_stack;
	init_pcb->kernel_stack = init_stack;
	idle_pcb->region_0 = region_0;
	init_pcb->region_0 = region_0_init;

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

		struct pte entry_init;
		entry_init.valid = 1;
		entry_init.kprot = (PROT_READ | PROT_WRITE) ;
		entry_init.uprot = (PROT_NONE);
		entry_init.pfn = k + num_pages_region_0;
		region_0_init[k] = entry_init;
	}

	int a;

	for (a = 0; a < MEM_INVALID_PAGES; a++) {
		region_0[a].valid = 0;
		region_0_init[a].valid = 0;

	}
	int m;

	for (m = 0; m < num_pages_region_1; m++) {
		struct pte entry;
		entry.valid = 1;
		entry.kprot = PROT_NONE;
		entry.uprot = PROT_NONE;
		entry.pfn = m + num_pages_region_0;
		region_1[m] = entry;
	}
	
	int num_text_pages = ((unsigned long) &_etext - PMEM_1_BASE) / PAGESIZE;
	
	int j;
	
	int kernel_page_base = PMEM_1_BASE / PAGESIZE;
	
	for (j = kernel_page_base; j < num_text_pages + kernel_page_base; j++){
		page_table[j]->valid = 1;
		page_table[j]->kprot = (PROT_READ | PROT_EXEC);

		struct pte entry = region_1[j - kernel_page_base];
		entry.valid = 1;
		entry.kprot = (PROT_READ | PROT_EXEC);
		region_1[j - kernel_page_base] = entry;
	}

	
	int num_dbh_pages = ((unsigned long)(actual_brk - (void*) &_etext) / PAGESIZE);

	int n;
	
	for (n = kernel_page_base + num_text_pages; n < kernel_page_base + num_text_pages + num_dbh_pages; n++){
		page_table[n]->valid = 1;
		page_table[n]->kprot = (PROT_READ | PROT_WRITE);
		struct pte entry = region_1[n - kernel_page_base];
		entry.valid = 1;
		entry.kprot = (PROT_READ | PROT_WRITE);
		region_1[n - kernel_page_base] = entry;
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
	num_free_pages = physical_pages_length;
	
	WriteRegister(REG_PTR0, (RCS421RegVal) region_0);
	WriteRegister(REG_PTR1, (RCS421RegVal) region_1);

	//ENABLING VIRTUAL MEMORY!!!!!
	WriteRegister(REG_VM_ENABLE, 1);
TracePrintf(0,
	    "Virtual memory enabled\n");
	printf("WE STARTED!!!\n");
	
	printf("Loading idle...\n");
	idleArgs[0] = NULL; 
	// idleArgs[1] = NULL; 

	LoadProgram("idle", idleArgs, idle_stack);
	
	idle_pcb->region_0_addr = region_0;
	idle_pcb->pid = 0;
	idle_pcb->next = init_pcb;


	printf("Loading init...\n");

	region_0 = region_0_init;

	// WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
	// WriteRegister(REG_PTR0, (RCS421RegVal) region_0_init);
	
	LoadProgram("idle", idleArgs, init_stack);
	int ctxtSwitch = ContextSwitch(MyFirstSwitchFunc, idle_pcb->ctxp, (void *)idle_pcb, (void *)init_pcb);
	printf("Switch %d\n", ctxtSwitch);
	
	init_pcb->region_0_addr = region_0_init;
	init_pcb->pid = 1;
	init_pcb->next = NULL;

	cur_pcb = idle_pcb;
	printf("Finished loading!!!!\n");
	TracePrintf(0, "PC starts at %p\n", frame->pc);

}





int SetKernelBrk(void *addr){
	printf("Brk\n");
	TracePrintf(0,"kernel break\n");
	actual_brk = addr;
	return 0;
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
LoadProgram(char *name, char **args, ExceptionStackFrame* stackIn)
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
    stackIn->sp = (void*) cpp;



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
        if (region_0[z].valid){
            freePhysicalPage(region_0[z].pfn);
            region_0[z].valid = 0;
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
    // >>>> Region 0 page table unused (and thus invalid)

    int a;
    int bottom = 0;
    int top = MEM_INVALID_PAGES;

    for (a = bottom; a < top; a++) {
        region_0[a].valid = 0;
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
        region_0[b].valid = 1;
        region_0[b].kprot = PROT_READ | PROT_WRITE;
        region_0[b].uprot = PROT_READ | PROT_EXEC;
        region_0[b].pfn   = allocPhysicalPage();
    }
    bottom = top;
    top += data_bss_npg;
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
        region_0[c].valid = 1;
        region_0[c].kprot = PROT_READ | PROT_WRITE;
        region_0[c].uprot = PROT_READ | PROT_WRITE;
        region_0[c].pfn   = allocPhysicalPage();
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
        region_0[d].valid = 1;
        region_0[d].kprot = PROT_READ | PROT_WRITE;
        region_0[d].uprot = PROT_READ | PROT_WRITE;
        region_0[d].pfn   = allocPhysicalPage();
    }

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
        region_0[e].kprot = PROT_READ | PROT_EXEC;
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
    stackIn->pc = li.entry;
	    TracePrintf(0, "starting pc is %p\n", kernel_frame->pc);
    /*
     *  Now, finally, build the argument list on the new stack.
     */
	// TracePrintf(0,
	//     "0 %p %p\n", (char*) cpp, (char*) argcount);
 //    *cpp++ = (char *)argcount;		/* the first value at cpp is argc */
	// TracePrintf(0,
	//     "1\n");
 //    *cpp++;
 //    cp2 = argbuf;
 //    for (i = 0; i < argcount; i++) {      /* copy each argument and set argv */
	// 	TracePrintf(0,
	//     "2\n");
	// *cpp++ = cp;
	// 	TracePrintf(0,
	//     "3\n");

	// strcpy(cp, cp2);
	// TracePrintf(0,
	//     "4\n");
	// cp += strlen(cp) + 1;
	// 	TracePrintf(0,
	//     "5\n");

	// cp2 += strlen(cp2) + 1;
	// TracePrintf(0,
	//     "6\n");

 //    }
 //    	TracePrintf(0,
	//     "7\n");
	// free(argbuf);
	// TracePrintf(0,
	//     "8\n");

 //    *cpp++ = NULL;	/* the last argv is a NULL pointer */
 //    	TracePrintf(0,
	//     "9\n");

 //    *cpp++ = NULL;	/* a NULL pointer for an empty envp */
	// TracePrintf(0,
	//     "10\n");
 //    *cpp++ = 0;		/* and terminate the auxiliary vector */
	// TracePrintf(0,
	//     "i bet this won't run\n");

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
        stackIn->regs[f] = 0;
    }
    kernel_frame->psr = 0;
    stackIn->psr = 0;     
	TracePrintf(0,
	    "f");

    return (0);
}
