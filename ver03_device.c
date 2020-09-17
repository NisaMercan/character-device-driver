/*** Character Device Drivers ver03_device.c
#  *   
#  * 25.08.2020 Nisa Mercan <nisamercan11@gmail.com>
#  * 26.08.2020 -updated-
#  * 27.08.2020 -updated- added ioctl added mmap 
#  * Please do not remove this header.
#  ***/

#include<linux/init.h>      /* To be able to use module_init() and module_exit() macros which are defined in linux/init.h */
#include<linux/module.h>    /* Necessary for all modules                                                                  */
#include<linux/kernel.h>    /* Types, macros and function for the kernel such as KERN_ALERT, KERN_INFO.                   */
#include<linux/cdev.h>      /* The relation between the device numbers and their devices                                  */
#include<linux/types.h>     /* dev_t type defined in 									  */
#include<asm/uaccess.h>     /* put_user, get_user, copy_from_user, copy_to_user 					  */
#include <linux/uaccess.h>  
#include<linux/fs.h>        /* Linux file system support. file_operations structure is defined in linux/fs.h              */
#include<linux/semaphore.h> /* A generalized mutex(=synchronizes access to a resource)                                    
			        Semaphore is used to access a single resource(integer pointer)		                  
			        sem_wait:When the semaphore value is > 0, resource is available.			  
			      		  If it is = 0, then the process waits until it becomes > 0.	                  
                                         It automatically decrements the semaphore variable by 1.			  
                                sem_post:Used to releasing the resource by increments the semaphore value by 1. 	  */ 

/* Updated 27.08.2020 10:44 */ 
#include <linux/ioctl.h>    /* Input Output Control                                                                       */
#include <linux/kdev_t.h>   /* Kernel header									          */	
#include <linux/device.h>   /* 												  */
#include<linux/slab.h>      /* For kmalloc. A slab is a set of contiguous pages of memory set aside by the slab allocator for an individual cache. */
#include <linux/mman.h>     /* Updated 27.08.2020 14:36 */ /* Memory management declarations */								  
#include <linux/mm.h>       /* Shared Memory Allocation	*/ 

MODULE_LICENSE("GPL"); /*General Public License*/
MODULE_AUTHOR("Nisa Mercan <nisamercan11@gmail.com>");
MODULE_DESCRIPTION("Character Device Driver");


#define WR_VALUE _IOW('a','a',int32_t*) /* To write value ioctl */
#define RD_VALUE _IOR('a','b',int32_t*) /* To  read value ioctl */

#define MAX_SIZE (PAGE_SIZE * 2)   /* max size is mmaped to userspace */

#define DEVICE_NAME "ver03_device" //defining a macro as the device name 


/* Structure for the device */
struct fake_device { 
	char data[100];
	struct semaphore sem; /*provides synchronization*/
} virtual_device;


/* Updated 27.08.2020 11:51 */
int32_t value = 0; // 32bit width

/* To be able to register a device; create a character device and the necessary variables such as major number.*/
struct cdev *mcdev; /* Represents a character device within the kernel */
int major_number;   /* Stores the the major number from dev_t using the macro MAJOR()*/
int ret;	    /* Holds the return values of operations. */
dev_t dev_num;      /* Holds the device numbers, has 32-bit quantity. 12 bits for the major MAJOR(dev_t dev) and 20 bits for the minor MINOR(dev_t dev).*/
static int countOpens = 0; //count how many times device has opened

/* Updated 27.08.2020 15:45 */
static char *shrd_mem = NULL;  /* Shared Memory, first initialized to the null. */



/* Open Operation
 * @param inode (index node) is a pointer to a disk location where the file is located. The inode gives us information about the file and the file's permissions.
 * @param struct file represents open file */
int device_open(struct inode *inode, struct file *filp) {

	/* Using a semaphore as mutual exclusive lock(mutex) and allow one operation for opening the device */
	if (down_interruptible(&virtual_device.sem) != 0) { /* down_interruptible allows the operations when the semaphore to be unlocked by the user
							       prevents other programs use this device that time */
		printk(KERN_ALERT "@ver03_device: Device could not lock during open operation."); /* Alert message, inside the printk() is printed to the console not just to the logfile. */
		return -1;
	}
	printk(KERN_INFO "@ver03_device: Opened the device");
	countOpens++;
	printk(KERN_INFO "@ver03_device: Device has been opened %d times\n", countOpens);
	return 0;
}


/* Read Operation 
 * @param char* bufStoreData is the location that data will send    
 * @param size_t bufCount is the size to read         
 * @param loff_t* curOffset indicates the file position the user is accessing*/
ssize_t device_read(struct file* filp, char* bufStoreData, size_t bufCount, loff_t* curOffset) {
	printk(KERN_INFO "@ver03_device: Reading from the device");
	ret = copy_to_user(bufStoreData, virtual_device.data, bufCount); /* copy_to_user(to, from, sizeToRead) */
	/*Updated 26.08.2020 12:36 */ printk(KERN_INFO "@ver03_device: %s : virtual device data", virtual_device.data);
 /* Updated 26.08.2020 08:31 */
   if(ret == 0) {
      printk(KERN_INFO "@ver03_device: Sent %ld characters to the user\n", bufCount);
   }
    else {
      printk(KERN_INFO "@ver03_device: Failed to send %d characters to the user\n", ret);
      return -EFAULT;  /* bad address */            
   } 
	return ret;
}


/* Write Operation
 * To send info from user to the device(kernel) */
ssize_t device_write(struct file* filp, const char* bufSourceData, size_t bufCount, loff_t* curOffset) {
	printk(KERN_INFO "@ver03_device: writing to the device");
	/* copy_from_user(destination, source, count) */
	ret = copy_from_user(virtual_device.data, bufSourceData, bufCount); 
	/* Updated 26.08.2020 12:37 */ printk(KERN_INFO "@ver03_device: %s : virtual device data", virtual_device.data);
	return ret;
}
 

/* Close Operation
 * */
int device_close(struct inode *inode, struct file *filp) {
	/* up() is opposite of down() for semaphore, we release the mutex(unlocked) that we obtained at device open
	 * this allows other operations to use the device */
	up(&virtual_device.sem);
	printk(KERN_INFO "@ver03_device: Closed device");
	return 0;
}


/* Ioctl Operation   Updated 27.08.2020 11:32
 * @param unsigned int cmd: Takes integer to make operations. Reading and writing operations are defined in macro.
 * @param unsigned long arg: Data that will be written.
 * */
long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
         switch(cmd) { //write or read
                case WR_VALUE:
                        copy_from_user(&value ,(int32_t*) arg, sizeof(value));
                        printk(KERN_INFO "@ver03_device: value = %d\n", value);
                        break;
                case RD_VALUE:
                        copy_to_user((int32_t*) arg, &value, sizeof(value));
                        break;
		}
        return 0;
}


/* Mmap Operation  Updated 27.08.2020 15:44 
 * @param struct_vm_area_struct *vma: The vma field is used to indicate the virtual address space where the memory should be mapped by the device.
 * */
int device_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret = 0;
    struct page *page = NULL; /* Page is the basic unit for virtual memory management. */
    unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);

    if (size > MAX_SIZE) {
        ret = -EINVAL; /* Invalid argument */
        goto out;  
    } 
   
    /* Struct page is used to embed information about all physical pages in the system.	
     * virt_to_page() returns the page associated with a virtual address 
     * page_to_pfn() return the page frame number associated with a struct page
     * vm_pgoff is the offset of the area within the file
     * */
    page = virt_to_page((unsigned long)shrd_mem + (vma->vm_pgoff << PAGE_SHIFT)); /* PAGE_SHIFT is equal to 12 (2^12 = 4096, the size of a page in bytes) */
    
    /* int remap_pfn_range (structure vm_area_struct *vma, unsigned long addr, unsigned long pfn, unsigned long size, pgprot_t prot) 
     * @param structure vm_area_struct *vma: the virtual memory space in which mapping is made  
     * @param usigned long addr: the virtual address space from where remapping begins
     * @param unsgined long pfn: the page frame number to which the virtual address should be mapped
     * @param unsigned long size: the size (in bytes) of the memory to be mapped
     * @param pgrot_t prot: protection flags for this mapping
     * */
    ret = remap_pfn_range(vma, vma->vm_start, page_to_pfn(page), size, vma->vm_page_prot);
    if (ret != 0) {
        goto out;
    }   

    out:
    return ret;
}


/* Updated 27.08.2020 14:24 */
/* File Operations
 * Functions can be called when user operates on the device file */
struct file_operations fops = {
	.owner   = THIS_MODULE,          /* Prevents unloading of the module when operations are in use. Kernel generates a struct module object and THIS_MODULE points to that object. */
	.open    = device_open,          /* called when opening the device */
	.release = device_close,         /* called when closing the device */
	.write   = device_write,         /* called when writing the device */
	.read    = device_read,          /* called when reading from the device */
	.unlocked_ioctl = device_ioctl,  /* without unlocked, ioctl was executed it will take the Big Kernel Lock (BKL) and nothing will execute at the same time. */
	.mmap    = device_mmap	         /*  maps a kernel address space to a user address space */ 
};


/* Register
 * Register our device with the system */
static int driver_entry(void){
	 
	/* Using the dynamic allocation to assign device a major number. 
	 * alloc_chrdev_region(dev_t* dev, uint baseminor, uint count, const char* name) registers a range of char device numbers
	 * @param dev_t* dev output is a parameter for the assigned numbers
	 * @param unsigned int baseminor is first of the requested range of minor numbers 
	 * @param unsigned int count is the number of minor numbers required
	 * @param char* name the name of the device or driver */
	ret = alloc_chrdev_region(&dev_num,0,1,DEVICE_NAME);
	if(ret < 0) { /*at the time kernel functions return negatives, we know that there is an error.*/
		printk(KERN_ALERT "@ver03_device: Failed to allocate a major number");
		return ret; /*propagate error*/
	}
	major_number = MAJOR(dev_num); /*extracts the major number using macro function and stores it*/
	if(major_number<0){
	  printk(KERN_ALERT "@ver03_device: Failed to register a major number\n");
          return major_number;
	}
	printk(KERN_INFO "@ver03_device: Major number is %d", major_number);
	printk(KERN_INFO "@ver03_device: \t\"mknod /dev/%s c %d 0\" for device file",DEVICE_NAME, major_number); /*drive message or display message*/


	/* Allocate the device structure and initialized correctly
	 * struct cdev * cdev_alloc (void) */

	/* Device Structure
	 * struct cdev is part of the inode structure.
	 * inode structure is used by the kernel internally to represent files, struct cdev is the kernel’s internal structure that represents char devices.
	 * This field contains a pointer to that structure when the inode refers to a char device file. */
	mcdev = cdev_alloc(); /*create the cdev structure and initialize the cdev*/
	if (mcdev != NULL) { /* if mcdev has no memory then failure. */
	   mcdev->ops = &fops; /* file operations structure */
	   mcdev->owner = THIS_MODULE;
	}

	else { /* Added on 26.08.2020 13:47 */
		printk(KERN_ALERT "@ver03_device: Could not allocate the memory for cdev structure.");
		return -ENOMEM;
        }
    
	/* Now that we created cdev, we have to add it to the kernel
	 * int cdev_add (struct cdev *p, dev_t dev, unsigned int count);
	 * @param cdev *p is the structure of the device
	 * @param dev_t dev is the 1st number of the device 
	 * @param unsigned count is the number of consecutive minor numbers corresponding to this device */
	ret = cdev_add(mcdev, dev_num, 1);    /* bunu da kullanabiliriz? void cdev_init (struct cdev * cdev, const struct file_operations * fops); */
	if(ret < 0) { /* A negative value returns on failure therefore checking for errors.*/
		printk(KERN_ALERT "@ver03_device: Unable to add cdev to kernel");
		return ret;
	}
	/* Initializing The Semaphore
	 * void sema_init(struct semaphore *sem, int val)
	 * @param val is the initial value to assign to a semaphore. */
	sema_init(&virtual_device.sem,1);  /*initialize the semaphore to the value of one*/

	
	/* Updated 27.08.2020 15:46 */
	/* void * kmalloc(size_t size, gfp_t flags)
	 * @param size_t size: 
	 * @param gfp_t flags:
	 * kmalloc is the normal method of allocating memory for objects smaller than page size in the kernel. Memory allocated by kmalloc resides in lowmem and it is physically contiguous. */
	shrd_mem = kmalloc(MAX_SIZE, GFP_KERNEL); /* GFP_KERNEL: Allocate normal kernel ram. May sleep */  
    	if (shrd_mem == NULL) {
        	return -ENOMEM; /* not enough memory read */
	}

    	sprintf(shrd_mem, "ver03_device: This is the shared memory.\n"); /* int sprintf(char *str, const char *format, ...) */
	return 0;
}


/* Unregister */
static void driver_exit(void) {

	/* Remove the character device from the system
	 * void cdev_del (struct cdev *p )*/
	cdev_del(mcdev);

	/* Unregister the character region that we registered 
	 * void unregister_chrdev_region (dev_t from, unsigned count)
	 * @param dev_t from is the first in the range of numbers to unregister
	 * @param unsigned count is the number of device numbers to unregister */
	unregister_chrdev_region(dev_num,1);
	kfree(shrd_mem); //frees the memory 
	printk(KERN_ALERT "@ver03_device: Unloaded the module");
}

/* Inform the kernel where to start and stop with our module/driver */
module_init(driver_entry);
module_exit(driver_exit);
