#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <stdio.h>
#include <sched.h>




int main(int ac, char *av[])
{
int          rc;
cpu_set_t    mask;
int          count;
unsigned int i;

  (void)ac;
  (void)av;

  rc = sched_getaffinity(0, sizeof(cpu_set_t), &mask);
  if (0 != rc)
  {
    return 1;
  }

  // Number of CPUs in set
  count = CPU_COUNT(&mask);

  printf("CPU:");
  for (i = 0; i < (unsigned)count; i ++)
  {
    if (CPU_ISSET(i, &mask))
    {
      printf(" %u", i);
    }
  } // End for

  printf("\n");

  return 0;
} // main
