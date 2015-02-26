#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include <comp421/hardware.h>
#include <comp421/yalnix.h>

int main(int argc, char *argv[]){
	TracePrintf(0,"Process ID is: %d", GetPid());
	while (1){

		Pause();
	}
	return 0;
}