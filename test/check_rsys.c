// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_rsys.c
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
//     07-Mar-2018  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=



#include <stdlib.h>

#include "check_rsys.h"
#include "../rsys/rsys.h"
#include "../rsys/rsys_p.h"




int main(void)
{
int      number_failed = 0;
Suite   *s;
SRunner *sr;

  fprintf(stderr, "%s: pid = %d\n", __FUNCTION__, getpid());

  rsys_startup();

  // Launch the tests
  s = suite_create("RSYS tests");
  suite_add_tcase(s, rsys_errcodes_tests());
  suite_add_tcase(s, rsys_api_tests());
  suite_add_tcase(s, rsys_rsystemd_tests());
  sr = srunner_create(s);

  srunner_run_all(sr, CK_ENV);

  rsys_end();

  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (0 == number_failed) ? EXIT_SUCCESS : EXIT_FAILURE;

} // main

