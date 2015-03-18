#include "stdio.h"

#include <comp421/yalnix.h>
#include <comp421/hardware.h>

int main(){
	printf("Starting idle\n");
	Pause();
	Fork();
	while (1){
		Pause();
		printf("Process ID is: %d\n", GetPid());
	}
	return 0;
}