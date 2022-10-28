#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include <stddef.h>

int dir_num = 0;
int file_num = 0;

char*
fmtname(char *path)
{
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--);
  p++;
  return p;
}

char *last_one(char *path) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  
  fd = open(path, 0);
  fstat(fd, &st);
  
  switch(st.type){
  case T_FILE:
    close(fd);
    return fmtname(buf);

  case T_DIR:
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0) continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
    }
    close(fd);
    return p;
  
  default:
    close(fd);
    return NULL;
  }
}

void tree(char *path, char *to_print, char root, char last) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0 && root) {
    if(root) printf("%s [error opening dir]\n", path);
    return;
  }
  
  if(fstat(fd, &st) < 0 && root){
    if(root) printf("%s [error opening dir]\n", path);
    close(fd);
    return;
  }
  
  switch(st.type){
  case T_FILE:
    if(root) {
      printf("%s [error opening dir]\n", path);
      return;
    }
    else  {
      file_num += 1;
      printf("%s|\n", to_print);
      printf("%s+-- %s\n", to_print, fmtname(path));
      return;
    }
    
  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }
    
    if(!root) dir_num += 1;
    
    if(root) printf("%s\n", path);
    else {
      printf("%s|\n", to_print);
      printf("%s+-- %s\n", to_print, fmtname(path));
    }
        
    char *to_pad;
    if(root) to_pad = "\0";
    else if(last) to_pad = "    ";
    else to_pad = "|   ";
    
    char str[strlen(to_print)+5];
    strcpy(str, to_print);
    strcpy(str+strlen(to_print), to_pad);
    
    char *last = last_one(path);
    
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0) continue;
      if (strcmp(".", de.name)==0 || strcmp("..", de.name)==0) continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;          
      
      if(strcmp(p, last)==0) tree(buf, str, 0, 1);
      else tree(buf, str, 0, 0);
    }
    break;
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  int fd[2], pid;
  
	if(pipe(fd) < 0) printf("Create Pipe Error!\n");
   
  if((pid = fork()) < 0) printf("Fork Error!\n");
  
  //child
  if(pid == 0) {
    close(fd[0]);
    char c = '\0';
    tree(argv[1], &c, 1, 0); //argc will always be 2
    write(fd[1], &dir_num, sizeof(int));
    write(fd[1], &file_num, sizeof(int));
  }
  //parent
  if(pid > 0) {
    int d_num, f_num;
    close(fd[1]);
    read(fd[0], &d_num, sizeof(int));
    read(fd[0], &f_num, sizeof(int));
    printf("\n%d directories, %d files\n", d_num, f_num);
  }
  exit(0);
}