#include "stdio.h"
#include "stdlib.h"
#include <comp421/yalnix.h>
#include <comp421/hardware.h>

int main(){
	printf("loaded idle\n");	
	while (1) {
		Pause();
	}

	return 0;
}