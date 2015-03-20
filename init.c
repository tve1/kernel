#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h> 

#include <comp421/yalnix.h>
#include <comp421/hardware.h>

int main(){	
	printf("Starting init\n");
	char* str = malloc(10000);
	char* str2 = "the quick bronw fox jusmped over the lzy dog";
	strcpy(str, str2);
	printf("Sucessfully got %s\n", str);
	free(str);
	//while (1){
		printf("Process ID is: %d\n", GetPid());
		Pause();
		Pause();
		Pause();
		Pause();
		Pause();
		Pause();
	//}
	printf("init is done\n");	
	return 0;
}