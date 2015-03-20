#include "stdio.h"

#include <comp421/yalnix.h>
#include <comp421/hardware.h>

int main(){
	int x = 0;
	Pause();
	printf("Process ID is: %d\n", GetPid());
	x++;
	printf("hell yeah %d\n",x);
	printf("mm is exiting\n");
	Exit(100);
	return 0;
}