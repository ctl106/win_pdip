// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_rsys_util.c
// Description : Unitary test for RSYS
//               
// License     :
//
//  Copyright (C) 2007-2018 Rachid Koucha <rachid dot koucha at gmail dot com>
//
//
//  This program is free and destined to help people using RSYS library
//
//
// Evolutions  :
//
//     28-Mar-2018  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>


#include "check_rsys.h"
#include "../rsys/rsys.h"
#include "../rsys/rsys_p.h"

#include "check_all.h"


// ----------------------------------------------------------------------------
// Name   : CK_RSYS_SOCK_PATH
// Usage  : Pathname of the server socket
// ----------------------------------------------------------------------------
#define CK_RSYS_SOCK_PATH_1   "test"


static pid_t ck_rsystemd_pid[6];


static unsigned int ck_rsys_nb_cpu = 0;


// ----------------------------------------------------------------------------
// Name   : ck_rsys_sock_var
// Usage  : Make the pathname of a server socket
// Return : Pathname of the server socket (static string, not reentrant)
// ----------------------------------------------------------------------------
char *ck_rsys_sock_var(unsigned int srv_nb)
{
static char env[128];
int  rc;

  rc = snprintf(env, sizeof(env), "%s/rsys_socket_%d", CK_RSYS_SOCK_PATH_1, srv_nb);
  assert(rc < (int)sizeof(env));

  return env;

} // ck_rsys_sock_var



// ----------------------------------------------------------------------------
// Name   : ck_rsys_child_rsystem
// Usage  : Call rsystem() in a child process
// Return : In the father : pid of the child, if OK or -1 if error
//          The child : its exit status is the result of the rsystem() call
// ----------------------------------------------------------------------------
int ck_rsys_child_rsystem(
                          const char *cmd,
                          unsigned int srv_nb
                         )
{
int rc;

  rc = check_fork();

  switch(rc)
  {
    case 0: // Child process
    {
      // Initialize RSYS
      rc = setenv(RSYS_SOCKET_PATH_ENV, ck_rsys_sock_var(srv_nb), 1);
      ck_assert_int_eq(rc, 0);

      // As the test cases are run in child processes, RSYS service
      // must be initialized inside them
      rc = rsys_lib_initialize();
      ck_assert_int_eq(rc, 0);

      rc = rsystem("%s", cmd);
      if (rc < 0)
      {
        ck_assert_int_eq(rc, -1);
        ck_assert_int_eq(errno, EAGAIN);
        exit(1);
      }
      else
      {
        ck_assert(WIFEXITED(rc));
        exit(WEXITSTATUS(rc));
      }
    }
    break;

    case -1 : // Error
    {
      return -1;
    }
    break;

    default : // Father
    {
      // Return the pid
      return rc;
    }
    break;
  } // end switch

} // ck_rsys_child_rsystem


static int rsys_launch_rsystemd(
				unsigned int  srv_nb,
                                char         *shells,
                                unsigned int  dbg_level
                               )
{
pid_t  pid;
int    rc;
char  *sock_path;

  sock_path = ck_rsys_sock_var(srv_nb);
  rc = setenv(RSYS_SOCKET_PATH_ENV, sock_path, 1);
  if (rc != 0)
  {
    return -1;
  }

  fprintf(stderr, "Starting RSYSTEMD with socket '%s'\n", getenv(RSYS_SOCKET_PATH_ENV));

  // Remove the socket file
  (void)unlink(sock_path);

  printf("Launching server#%u with shells \"%s\"\n", srv_nb, shells ? shells : "");

  pid = fork();
  switch(pid)
  {
    case 0: // Child process
    {
    char         *av[6];
    unsigned int  i;

      // Execute rsystemd
      i = 0;
      av[i++] = "rsys/rsystemd";
      if (shells)
      {
        av[i++] = "-s";
        av[i++] = shells;
      }

      if (dbg_level)
      {
      char level[10];

        av[i++] = "-d";
        rc = snprintf(level, sizeof(level), "%d", dbg_level);
        assert(rc < (int)sizeof(level));
        av[i++] = level;
      }

      av[i] = NULL;

      (void)execv(av[0], av);
      _exit(1);
    }
    break;

    case -1: // Error
    {
      // Errno is set
      return -1;
    }
    break;

    default: // Father
    {
    struct timespec to;
    unsigned int max = 1000;

      // Save the pid to be able to terminate the daemon
      ck_rsystemd_pid[srv_nb] = pid;

      // Make sure that rsystemd is started before going forward
      // by checking the existence of the socket file
      while ((max > 0) && (0 != access(sock_path, F_OK)))
      {
        to.tv_sec = 0;
        to.tv_nsec = 5000000; // 5 ms
        (void)nanosleep(&to, 0);
        max --;
      } // End while

      fprintf(stderr, "Forked daemon RSYSTEMD with pid %d\n", ck_rsystemd_pid[srv_nb]);
    }
    break;
  } // End switch

  return 0;

} // rsys_launch_rsystemd


static void check_sig_child(
                        int        sig,   // Received signal
                        siginfo_t *info,   // Context of the signal
                        void *ctx
                       )
{
  (void)ctx;

  printf("Signal %d from %d\n", sig, info->si_pid);

} // check_sig_child



void rsys_startup(void)
{
int           rc;
size_t        offset, sz;
char         *ptr1;
unsigned int  i;
int           status;
struct sigaction action;

    // Register the signal handler
    action.sa_sigaction = check_sig_child;
    (void)sigemptyset(&(action.sa_mask));
    action.sa_flags     = SA_SIGINFO;
    rc = sigaction(SIGCHLD, &action, 0);
    if (rc < 0)
    {
      exit(1);
    }
  


  // Get the number of CPUs
  rc = sysconf(_SC_NPROCESSORS_ONLN);
  if (rc < 0)
  {
    exit(1);
  }
  ck_rsys_nb_cpu = rc; 


  // Start the RSYS servers

  rc = rsys_launch_rsystemd(0, 0, 0);
  if (rc != 0)
  {
    exit(1);
  }
  printf("Server#0 with no parameters\n");


  sz = ck_rsys_nb_cpu * (5 + 1) + 1;
  ptr1 = (char *)malloc(sz);
  if (!ptr1)
  {
    exit(1);
  }
  offset = 0;
  for (i = 0; i < ck_rsys_nb_cpu; i ++)
  {
    rc = snprintf(&(ptr1[offset]), sz, "%d%s", i, (i < (ck_rsys_nb_cpu - 1) ? ":":""));
    if (rc >= (int)sz)
    {
      exit(1);
    }
    offset += rc;
    sz -= rc;
  } // End for
  ptr1[offset] = '\0';

  rc = rsys_launch_rsystemd(1, ptr1, 0);
  if (rc != 0)
  {
    exit(1);
  }
  printf("Server#1 with parameter '%s'\n", ptr1);
  free(ptr1);

  // Server#2
  sz = 5 + 1 + 5;
  ptr1 = (char *)malloc(sz);
  if (!ptr1)
  {
    exit(1);
  }
  rc = snprintf(ptr1, sz, "%d-%d", 0, ck_rsys_nb_cpu - 1);
  if (rc >= (int)sz)
  {
    exit(1);
  }
  rc = rsys_launch_rsystemd(2, ptr1, 0);
  if (rc != 0)
  {
    exit(1);
  }
  printf("Server#2 with parameter '%s'\n", ptr1);
  free(ptr1);

  // Server#3
  sz = ck_rsys_nb_cpu * (5 + 1) + 1;
  ptr1 = (char *)malloc(sz);
  if (!ptr1)
  {
    exit(1);
  }
  offset = 0;
  for (i = 0; i < ck_rsys_nb_cpu; i ++)
  {
    rc = snprintf(&(ptr1[offset]), sz, "%d%s", i, (i < (ck_rsys_nb_cpu - 1) ? ",":""));
    if (rc >= (int)sz)
    {
      exit(1);
    }
    offset += rc;
    sz -= rc;
  } // End for
  ptr1[offset] = '\0';

  rc = rsys_launch_rsystemd(3, ptr1, 0);
  if (rc != 0)
  {
    exit(1);
  }
  printf("Server#3 with parameter '%s'\n", ptr1);
  free(ptr1);


  rc = rsys_launch_rsystemd(4, "0-", 0);
  if (rc != 0)
  {
    exit(1);
  }
  printf("Server#4' with parameter '0-'\n");

  (void)kill(ck_rsystemd_pid[4], SIGTERM);
  rc = waitpid(ck_rsystemd_pid[4], &status, 0);
  assert(rc == ck_rsystemd_pid[4]);
  assert(WIFEXITED(status));
  assert(2 == WEXITSTATUS(status));

  sz = 1 + 5 + 1;
  ptr1 = (char *)malloc(sz);
  if (!ptr1)
  {
    exit(1);
  }
  rc = snprintf(ptr1, sz, "-%d", ck_rsys_nb_cpu - 1);
  if (rc >= (int)sz)
  {
    exit(1);
  }

  rc = rsys_launch_rsystemd(4, ptr1, 0);
  if (rc != 0)
  {
    exit(1);
  }
  printf("Server#4 with parameter '%s'\n", ptr1);
  free(ptr1);

  (void)kill(ck_rsystemd_pid[4], SIGTERM);
  rc = waitpid(ck_rsystemd_pid[4], &status, 0);
  assert(rc == ck_rsystemd_pid[4]);
  assert(WIFEXITED(status));
  assert(2 == WEXITSTATUS(status));

  sz = 1 + 5 + 1;
  ptr1 = (char *)malloc(sz);
  if (!ptr1)
  {
    exit(1);
  }
  rc = snprintf(ptr1, sz, ",%d", ck_rsys_nb_cpu - 1);
  if (rc >= (int)sz)
  {
    exit(1);
  }

  rc = rsys_launch_rsystemd(4, ptr1, 0);
  if (rc != 0)
  {
    exit(1);
  }
  printf("Server#4 with parameter '%s'\n", ptr1);
  free(ptr1);

  (void)kill(ck_rsystemd_pid[4], SIGTERM);
  rc = waitpid(ck_rsystemd_pid[4], &status, 0);
  assert(rc == ck_rsystemd_pid[4]);
  assert(WIFEXITED(status));
  assert(2 == WEXITSTATUS(status));

  rc = rsys_launch_rsystemd(4, "0,", 0);
  if (rc != 0)
  {
    exit(1);
  }
  printf("Server#4 with parameter '0,'\n");

  (void)kill(ck_rsystemd_pid[4], SIGTERM);
  rc = waitpid(ck_rsystemd_pid[4], &status, 0);
  assert(rc == ck_rsystemd_pid[4]);
  assert(WIFEXITED(status));
  assert(2 == WEXITSTATUS(status));

  rc = rsys_launch_rsystemd(4, "0,-", 0);
  if (rc != 0)
  {
    exit(1);
  }
  printf("Server#4 with parameter '0,-'\n");

  (void)kill(ck_rsystemd_pid[4], SIGTERM);
  rc = waitpid(ck_rsystemd_pid[4], &status, 0);
  assert(rc == ck_rsystemd_pid[4]);
  assert(WIFEXITED(status));
  assert(2 == WEXITSTATUS(status));

  rc = rsys_launch_rsystemd(4, "0,,", 0);
  if (rc != 0)
  {
    exit(1);
  }
  printf("Server#4 with parameter '0,,'\n");

  (void)kill(ck_rsystemd_pid[4], SIGTERM);
  rc = waitpid(ck_rsystemd_pid[4], &status, 0);
  assert(WIFEXITED(status));
  assert(2 == WEXITSTATUS(status));

  rc = rsys_launch_rsystemd(4, ",", 0);
  if (rc != 0)
  {
    exit(1);
  }
  printf("Server#4 with parameter ','\n");

  (void)kill(ck_rsystemd_pid[4], SIGTERM);
  rc = waitpid(ck_rsystemd_pid[4], &status, 0);
  assert(rc == ck_rsystemd_pid[4]);
  assert(WIFEXITED(status));
  assert(2 == WEXITSTATUS(status));

  rc = rsys_launch_rsystemd(4, "::", 0);
  if (rc != 0)
  {
    exit(1);
  }
  printf("Server#4 with parameter '::'\n");


  rc = rsys_launch_rsystemd(5, (char *)0, 10);
  if (rc != 0)
  {
    exit(1);
  }
  printf("Server#4 with parameter '-d 10'\n");

} // rsys_startup


int rsys_wait_child(pid_t pid)
{
int          rc;
int          status;

  // When they dies, the rsystemd daemons interrupt waitpid()
  do
  {
    rc = waitpid(pid, &status, 0);
  } while ((-1 == rc) && (EINTR == errno));
  ck_pdip_assert(rc == pid, "rc=%d, pdi=%d, errno='%m' (%d)\n", rc, pid, errno);

  return status;

} // rsys_wait_child


static void rsys_wait_systemd(void)
{
int          status;
unsigned int i;

  for (i = 0; i < (sizeof(ck_rsystemd_pid) / sizeof(pid_t)); i ++)
  {
    status = rsys_wait_child(ck_rsystemd_pid[i]);
    assert(WIFEXITED(status));
    assert(2 == WEXITSTATUS(status));
  } // End for
} // rsys_wait_pid


void rsys_end(void)
{
unsigned int i;

  //int status;

  printf("Cleanup...\n");

  //status = system("ps -ef");

  for (i = 0; i < (sizeof(ck_rsystemd_pid) / sizeof(pid_t)); i ++)
  {
    (void)kill(ck_rsystemd_pid[i], SIGTERM);
  } // End for

  rsys_wait_systemd();

} // rsys_end


