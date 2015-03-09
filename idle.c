#include "stdio.h"

#include <comp421/yalnix.h>
#include <comp421/hardware.h>

int main(){
	while (1){
		printf("Process ID is: %d\n", GetPid());
		Pause();
	}
	return 0;
}