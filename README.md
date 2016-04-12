# Rootkit-functionality-implementation-under-Unix
Implement portion of functionality related to Rootkit in C under unix environment.
sneaky_process.c is the attack program which copy the file in "/etc/pwd" to a temp file and modify the original content in pwd file,
then load the sneaky_kernel module
sneaky_mod.c is the sneaky Linux kernel module which modify "getdents", "read" and "open" system calls to hide the running attack program
from the OS and hide the modifications attack program made to the "/etc/pwd" file
