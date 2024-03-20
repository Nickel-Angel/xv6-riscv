#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int g(int x) {
  return x+3;
}

int f(int x) {
  return g(x);
}

unsigned int flip(unsigned int x) { // big-endian to little-endian
    unsigned int res = 0;
    for(int i = 0; i < 4; ++i)
    {
        res <<= 8;
        res |= (x >> (i * 8)) & 0xff;
    }
    return res;
}

void main(void) {
  // original call.c start
  printf("%d %d\n", f(8)+1, 13);
  // original call.c end
  unsigned int i = 0x00646c72;
  printf("H%x Wo%s 0x%x\n", 57616, &i, flip(i));
  exit(0);
}
