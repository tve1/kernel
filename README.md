(Te-Rue) Victoria Eng tve1
David Nichol - dan1

ALGORITHMS/DATA STRUCTURES
Our struct PCB contains everything the kernel needed to know about a process. We added things to it as we realized we needed them. Most of the variable names are self-explanatory. It also contains two linked lists of structs we made, exited_node and child_node. 
 exited_node is a struct that contains process ID and exit status of a process that was created when a process exited to be given to its parent process.
 child_node is a stuct that contains the pid and pcb of a process, so that a parent process had a list of its running children processes.

node is a struct which stored a page's base address, whether or not it'd been removed for allocation, and its pfn.

pte_wrapper is a struct used to keep track of a PTE's pfn and physical address, because we found we needed it when we needed to allocate a new region0 for the child process when a process called fork.

TESTING
We tested mostly within the init program and with a program called mm. mm was used as a child process after we got Fork to work to debug Exec, Wait, and Exit. 


ETC. 
We encountered a bug where we have a segfault if a process calls fork infinitely. However, our kernel functions properly, in which it stops making new processes and continues switching between the existing processes when it runs out of memory, if we call pause after forking.   