#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  
  char buf[100];

  if(getcwd(buf, sizeof(buf)) < 0){
    printf(2, "getcwd failed\n");
    exit();
  }  
  printf(1, "%s\n", buf);

  exit();
}