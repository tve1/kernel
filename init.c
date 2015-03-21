#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h> 

#include <comp421/yalnix.h>
#include <comp421/hardware.h>

int main(){	
	printf("Starting init\n");
	char* str = malloc(TERMINAL_MAX_LINE);
	// char* str2 = "the quick bronw fox jusmped over the lzy dog";
	// strcpy(str, str2);
	// free(str);

	 while (1){
		printf("Or here\n");
		TtyRead(0, str, TERMINAL_MAX_LINE);
		printf("Sucessfully got %s\n", str);
		printf("Process ID is: %d\n", GetPid());
		Pause();
		printf("Get here before breaking\n");
	 }
	printf("init is done\n");	
	return 0;
}