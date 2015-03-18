#include "stdio.h"

#include <comp421/yalnix.h>
#include <comp421/hardware.h>

int main(){
	int x = 0;
	while (1){
		Pause();
		printf("Process ID is: %d\n", GetPid());
		x++;
		printf("hell yeah %d\n",x);
	}
	return 0;
}