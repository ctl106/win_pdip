
INCLUDE (../FindCheck.cmake)

if (CHECK_FOUND)

  MESSAGE("CHECK_INCLUDE_DIR=" "${CHECK_INCLUDE_DIR}")
  MESSAGE("CHECK_LIBRARIES=" "${CHECK_LIBRARIES}")


  add_executable(check_all check_all.c
                           check_pdip_errcodes.c
                           check_pdip_api.c
                           check_isys_errcodes.c
                           check_isys_api.c
                           check_isys_api_env.c
                           check_rsys_errcodes.c
                           check_rsys_api.c
                           check_rsys_rsystemd.c
                           check_rsys_util.c
                           )

  target_link_libraries(check_all ${CHECK_LIBRARIES} isys rsys pdip pthread)

  add_executable(check_pdip check_pdip.c
                            check_pdip_errcodes.c
                            check_pdip_api.c
                            )

  target_link_libraries(check_pdip ${CHECK_LIBRARIES} pdip pthread)

  add_executable(myaffinity myaffinity.c)
  add_executable(pdata pdata.c)
  add_executable(pterm pterm.c)


  add_executable(check_isys check_isys.c
                            check_isys_errcodes.c
                            check_isys_api.c
                            check_isys_api_env.c
                            )

  target_link_libraries(check_isys isys pdip pthread ${CHECK_LIBRARIES})

  add_executable(check_rsys check_rsys.c
                            check_rsys_errcodes.c
                            check_rsys_api.c
                            check_rsys_rsystemd.c
                            check_rsys_util.c
                            )

  target_link_libraries(check_rsys rsys pdip pthread ${CHECK_LIBRARIES})

  #
  # Test coverage
  #
  if (CMAKE_COVERAGE)

    set(COVERAGE_EXCLUDES 'test/*' 'bits/*')

    setup_target_for_coverage(NAME all_coverage
                              EXECUTABLE test/check_all
                              DEPENDENCIES pdip isys rsys)

    setup_target_for_coverage(NAME pdip_coverage
                              EXECUTABLE test/check_pdip
                              DEPENDENCIES pdip)

    setup_target_for_coverage(NAME isys_coverage
                              EXECUTABLE test/check_isys
                              DEPENDENCIES isys)

    setup_target_for_coverage(NAME rsys_coverage
                              EXECUTABLE test/check_rsys
                              DEPENDENCIES rsys)
  endif()

else(CHECK_FOUND)

  MESSAGE("Check package not found ==> No unitary test will be built")

endif(CHECK_FOUND)


ADD_EXECUTABLE(tisys tisys.c)
TARGET_LINK_LIBRARIES(tisys isys pdip pthread)

ADD_EXECUTABLE(trsys trsys.c)
TARGET_LINK_LIBRARIES(trsys rsys pthread pdip)


#add_executable(tst1 tst_1.c)
#target_link_libraries(tst1 pdip pthread)


#ADD_EXECUTABLE(tpdip tpdip.c)
#TARGET_LINK_LIBRARIES(tpdip pthread pdip)

ADD_EXECUTABLE(man_exe_1 man_exe_1.c)
TARGET_LINK_LIBRARIES(man_exe_1 pdip pthread)

ADD_EXECUTABLE(man_exe_2 man_exe_2.c)
TARGET_LINK_LIBRARIES(man_exe_2 pdip pthread)

ADD_EXECUTABLE(man_exe_3 man_exe_3.c)
TARGET_LINK_LIBRARIES(man_exe_3 pdip pthread)

