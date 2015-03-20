#include "stdio.h"
#include "stdlib.h"
#include <comp421/yalnix.h>
#include <comp421/hardware.h>

int main(){
	int* ptr = malloc(sizeof(int));
	int a = Wait(ptr);
	printf("A: %d\n", a);
	printf("Starting idle\n");
	Pause();
	int pid = Fork();
	int b = Wait(ptr);
	printf("B: %d\n", b);
	printf("THIS PID:%d\n",pid);
	if (pid != 0){
		char** args = malloc(sizeof(char*));
		args[0] = NULL;
		Exec("mm", args);
	}
	int i = 20;
	while (i > 0){
		Pause();
		printf("Process ID is: %d \n", GetPid());
		i--;
	}
	printf("idle is OVER\n");

	return 0;
}