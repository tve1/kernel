#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h> 

#include <comp421/yalnix.h>
#include <comp421/hardware.h>

int main(){	
	printf("Starting init\n");
	Pause();
	Pause();
	Pause();
	char* str = malloc(TERMINAL_MAX_LINE);
	// int* ptr = malloc(sizeof(int));
	// int a = Wait(ptr);
	// printf("A: %d\n", a);
	// int pid = Fork();
	// if (pid != 0){
	// 	char** args = malloc(sizeof(char*));
	// 	args[0] = NULL;
	// 	Exec("mm", args);
	// }
	// int b = Wait(ptr);
	// printf("B: %d\n", b);
	// printf("status: %d\n", ptr[0]);
	// printf("THIS PID:%d\n",pid);

	 while (1){
		//printf("Or here\n");
		Fork();
		// Pause();
		// printf("This is the last thing to print\n");
		printf("Process ID is: %d\n", GetPid());
		TtyPrintf(0,"Process ID is: %d\n", GetPid());
		TtyWrite(0, "brown is sweet\n", 15);
		printf("0 Sucessfully got %s\n", str);
		TtyWrite(1, "we're nice people\n", 18);
		printf("1 Sucessfully got %s\n", str);
		TtyWrite(2, "baker women have soft skin\n", 27);
		printf("2 Sucessfully got %s\n", str);
		TtyWrite(3, "anti-anti-jacks\n", 16);
		printf("3 Sucessfully got %s\n", str);
		

		//Pause();
		//printf("Get here before breaking\n");
	 }
	printf("init is done\n");	
	return 0;
}