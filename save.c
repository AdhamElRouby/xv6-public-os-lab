#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

// write() may write fewer bytes than requested.
static int
writeall(int fd, char *text, int length)
{
  int n;

  while(length > 0){
    n = write(fd, text, length);
    if(n <= 0)
      return -1;
    text += n;
    length -= n;
  }
  return 0;
}

int
main(int argc, char *argv[])
{
  int fd, i;
  struct stat st;

  if(argc < 3){
    printf(2, "Usage: save filename string [more words ...]\n");
    exit();
  }

  // xv6-public has no O_TRUNC. Recreate an existing regular file so
  // saving shorter text does not leave bytes from its previous contents.
  // Check the type first: never remove a directory or device.
  if(stat(argv[1], &st) == 0){
    if(st.type != T_FILE){
      printf(2, "save: %s is not a regular file\n", argv[1]);
      exit();
    }
    if(unlink(argv[1]) < 0){
      printf(2, "save: cannot replace %s\n", argv[1]);
      exit();
    }
  }

  fd = open(argv[1], O_CREATE | O_WRONLY);
  if(fd < 0){
    printf(2, "save: cannot open %s\n", argv[1]);
    exit();
  }

  // The stock xv6 shell does not group quoted strings. Accept multiple
  // words and join them with a space; do not append a newline or NUL.
  for(i = 2; i < argc; i++){
    if((i > 2 && writeall(fd, " ", 1) < 0) ||
       writeall(fd, argv[i], strlen(argv[i])) < 0){
      printf(2, "save: write failed for %s\n", argv[1]);
      close(fd);
      exit();
    }
  }

  close(fd);
  exit();
}
