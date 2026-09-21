#include "types.h"
#include "stat.h"
#include "user.h"

static void
putc(int fd, char c)
{
  write(fd, &c, 1);
}

static void
printint(int fd, int xx, int base, int sgn)
{
  static char digits[] = "0123456789ABCDEF";
  char buf[16];
  int i, neg;
  uint x;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  // Digits are extracted from least significant to most significant.
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);
  if(neg)
    buf[i++] = '-';

  while(--i >= 0)
    putc(fd, buf[i]);
}

static void
printfloat(int fd, double x) {
  int negative = 0;
  if (x < 0) {
    negative = 1;
    x = -x;
  }
  int intPart = (int)x;
  double fracPart = x - intPart;
  // Round to six fractional digits before formatting.
  uint fractional = (uint)(fracPart * 1000000.0 + 0.5);

  // Rounding can carry into the integer part.
  if(fractional == 1000000){
    intPart++;
    fractional = 0;
  }

  if(negative && (intPart != 0 || fractional != 0))
    putc(fd, '-');
  printint(fd, intPart, 10, 0);
  if(fractional == 0)
    return;

  // Remove insignificant zeros from the fractional part.
  int digits = 6;
  while(fractional % 10 == 0){
    fractional /= 10;
    digits--;
  }

  putc(fd, '.');
  uint divisor = 1;
  for(int i = 1; i < digits; i++)
    divisor *= 10;

  for(; divisor > 0; divisor /= 10){
    putc(fd, '0' + fractional / divisor);
    fractional %= divisor;
  }
}

// Print to the given fd. Only understands %d, %f, %x, %p, %s.
void
printf(int fd, const char *fmt, ...)
{
  char *s;
  int c, i, state;
  uint *ap;

  state = 0;
  // Variadic arguments are words; a double occupies two words.
  ap = (uint*)(void*)&fmt + 1;
  for(i = 0; fmt[i]; i++){
    c = fmt[i] & 0xff;
    if(state == 0){
      if(c == '%'){
        state = '%';
      } else {
        putc(fd, c);
      }
    } else if(state == '%'){
      if(c == 'd'){
        printint(fd, *ap, 10, 1);
        ap++;
      } else if(c == 'f'){
        printfloat(fd, *(double*)ap);
        ap += 2; // Move the pointer by 2 because double takes 2 uints
      } else if(c == 'x' || c == 'p'){
        printint(fd, *ap, 16, 0);
        ap++;
      } else if(c == 's'){
        s = (char*)*ap;
        ap++;
        if(s == 0)
          s = "(null)";
        while(*s != 0){
          putc(fd, *s);
          s++;
        }
      } else if(c == 'c'){
        putc(fd, *ap);
        ap++;
      } else if(c == '%'){
        putc(fd, c);
      } else {
        // Unknown % sequence.  Print it to draw attention.
        putc(fd, '%');
        putc(fd, c);
      }
      state = 0;
    }
  }
}
