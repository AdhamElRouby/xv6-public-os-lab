#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 3){
    printf(2, "Usage: mv old new\n");
    exit();
  }

  if(rename(argv[1], argv[2]) < 0){
    printf(2, "mv: rename failed\n");
    exit();
  }

  exit();
}