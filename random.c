/******************************************************************/
/* Генератор случайных чисел */
/* конкатенация двух следующих 16-битных генераторов с переносом (multiply-with-carry) */
/* x(n)=a*x(n-1)+перенос mod 2^16 и y(n)=b*y(n-1)+перенос mod 2^16, */
/* число и перенос упакованы в одно 32-битное целое число.        */
/******************************************************************/
#include <stdint.h>
#include "config.h"
#include "smp.h"

/* Хранить отдельное начальное значение (seed) для каждого процессора */
/* Размещайте начальные значения на расстоянии как минимум одной кэш-линии, иначе производительность сильно упадет! */
static uint32_t SEED_X[MAX_CPUS*16];
static uint32_t SEED_Y[MAX_CPUS*16];

uint32_t memtest_rand (int cpu)
{
   static unsigned int a = 18000, b = 30903;
   int me;

   me = cpu*16;

   SEED_X[me] = a*(SEED_X[me]&65535) + (SEED_X[me]>>16);
   SEED_Y[me] = b*(SEED_Y[me]&65535) + (SEED_Y[me]>>16);

   return ((SEED_X[me]<<16) + (SEED_Y[me]&65535));
}


void rand_seed( unsigned int seed1, unsigned int seed2, int cpu)
{
   int me;

   me = cpu*16;
   SEED_X[me] = seed1;   
   SEED_Y[me] = seed2;
}