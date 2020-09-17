/*** Test Character Device Drivers testver03.c
#  *   
#  * 26.08.2020 Nisa Mercan <nisamercan11@gmail.com>
#  * 27.08.2020 -updated- added ioctl added mmap
#  * 
#  * Please do not remove this header.
#  ***/

 
#include <linux/mman.h>  
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h> // O_RDWR Open for reading and writing
#include<string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include<unistd.h> //defines miscellaneous symbolic constants and types, and declares miscellaneous functions
#include<sys/ioctl.h>
#include <sys/mman.h> /* Updated 27.08.2020 16:13*/


#define WR_VALUE _IOW('a','a',int32_t*) 
#define RD_VALUE _IOR('a','b',int32_t*)

#define DEVICE "/dev/ver03_device"
 

int main() {
	int i, fd;
	int32_t value, number; 
	char ch, write_buf[100], read_buf[100];
	char *p = NULL; //

	fd = open(DEVICE, O_RDWR |O_NDELAY); /* O_RDWR: allows us to write and read on the device
						O_NDELAY: the file is opened in nonblocking mode. Proccess do not wait. */

	if(fd == -1) { //error check
		printf("@testver03: Could not open the file.");
		exit(-1);
	}

	printf("@testver03: To read data from the device press r.\nTo write data to the device press w.\nTo ioctl to the device press i.\nTo map the device press m.\nPlease enter your command:\n");
	scanf("%c", &ch); //operation command from the user

	switch(ch) {
		case 'w':
			printf("@testver03: Please enter your data: ");
			scanf(" %[^\n] ", write_buf); //%[^\n]% all the characters entered as the input until someone hit the enter button are stored in the variable write_buf
			write(fd, write_buf, sizeof(write_buf)); //file_operation
			break;

		case 'r':
			read(fd, read_buf, sizeof(read_buf)); //file_operation
			printf("@testver03: device: %s", read_buf);
			break;

		case 'i':
			printf("@testver03: Enter the value to send\n");
      		 	scanf("%d",&number);
        		printf("@testver03: Writing value to device driver\n");
        		ioctl(fd, WR_VALUE, (int32_t*) &number); 
 
        		printf("@testver03: Reading value from driver\n");
        		ioctl(fd, RD_VALUE, (int32_t*) &value);
        		printf("@testver03: Value is %d\n", value);

		case 'm':
			/* void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) 
			 * @param void* addr: The starting address for the mapping is in addr.
			 * @param size_t length: The length of the mapping.
			 * @param int prot: Memory protection of the mapping. Error when the flag conflict with open modes.
			 * @param int flags: Determines whether the mapping is visible to other processes mapping the same addres. 
			 * @param int fd: File descriptor.
			 * @param off_t offset: Starting at the offset.
			 * */
			p = (char*)mmap(0, 4096,  
                			PROT_READ | PROT_WRITE,  
                			MAP_SHARED,  
                			fd,  
                			0);  
        		printf("@testver03: %s", p);  
        		munmap(p, 4096); /* int munmap(void *addr, size_t length) */
			break;
		default:
			printf("@testver03: Invalid command, try again.");
			break;
		}
	
	close(fd); //close the file
	return 0;
}




