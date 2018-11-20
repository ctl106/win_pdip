// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_isys.c
// Description : Unitary test for ISYS
//               
// License     :
//
//  Copyright (C) 2007-2018 Rachid Koucha <rachid dot koucha at gmail dot com>
//
//
//  This program is free and destined to help people using ISYS library
//
//
// Evolutions  :
//
//     26-Feb-2018  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=



#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


#include "check_isys.h"




int main(void)
{
int      number_failed = 0;
Suite   *s;
SRunner *sr;

  printf("%s: %d\n", __FUNCTION__, getpid());

  s = suite_create("ISYS tests");
  suite_add_tcase(s, isys_errcodes_tests());
  suite_add_tcase(s, isys_api_tests());
  suite_add_tcase(s, isys_api_env_tests());

  sr = srunner_create(s);

  srunner_run_all(sr, CK_ENV);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (0 == number_failed) ? EXIT_SUCCESS : EXIT_FAILURE;
} // main

