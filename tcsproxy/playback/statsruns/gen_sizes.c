#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <limits.h>
#include <math.h>

#define NUM_BUCS 2048

void bucknum(FILE *of)
{
  double logmax, frac, tfrac;
  int i;
  unsigned long int theint;

  logmax = log10((double) ULONG_MAX);
  fprintf(of, "0 0\n");
  for (i=1; i<NUM_BUCS; i++) {
    frac = ((double) i) * logmax;
    frac = frac / ((double) NUM_BUCS);
    tfrac = pow((double) 10, frac);
    theint = (unsigned long int) tfrac;
    fprintf(of, "%d %lu\n", i, theint);
  }
}

#ifndef ULLONG_MAX
  #define ULLONG_MAX 18446744073709551615ULL
#endif
void  bucknumll(FILE *of)
{
  double logmax, frac, tfrac;
  int i;
  unsigned long long int theint;

  logmax = log10((double) ULLONG_MAX);
  fprintf(of, "0 0\n");
  for (i=1; i<NUM_BUCS; i++) {
    frac = ((double) i) * logmax;
    frac = frac / ((double) NUM_BUCS);
    tfrac = pow((double) 10, frac);
    theint = (unsigned long long int) tfrac;
    fprintf(of, "%d %lu\n", i, theint);
  }
}

int main(int argc, char **argv) {

   FILE *of1, *of2;

   of1 = fopen("sz_buckets.txt", "w");
   bucknum(of1);

   of2 = fopen("dur_buckets.txt", "w");
   bucknumll(of2);

   close(of1);
   close(of2);
   exit(0);
}
