#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>


int main(int ac, char *av[])
{
int           opt;
int           value;

  while ((opt = getopt(ac, av, "e:k:")) != EOF)
  {
    switch(opt)
    {
      case 'e' : // Exit code
      {
        value = atoi(optarg);
        printf("Terminating with exit code %d...\n", value);
        exit(value);
      }
      break;

      case 'k': // Signal
      {
        value = atoi(optarg);
        printf("Killing myself with signal %d...\n", value);
        kill(0, value);
      }
      break;

      default:
      {
        return 1;
      }
    } // End switch
  } // End while

  return 0;

} // main
