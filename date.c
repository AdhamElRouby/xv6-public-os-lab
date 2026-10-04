#include "types.h"
#include "user.h"
#include "date.h"

int
main(int argc, char *argv[])
{
  struct rtcdate r;

  if(date(&r)){
    printf(2, "date failed\n");
    exit();
  }

  printf(1, "%d-", r.year);
  if(r.month < 10) printf(1, "0");
  printf(1, "%d-", r.month);
  if(r.day < 10) printf(1, "0");
  printf(1, "%d ", r.day);
  if(r.hour < 10) printf(1, "0");
  printf(1, "%d:", r.hour);
  if(r.minute < 10) printf(1, "0");
  printf(1, "%d:", r.minute);
  if(r.second < 10) printf(1, "0");
  printf(1, "%d\n", r.second);

  exit();
}
