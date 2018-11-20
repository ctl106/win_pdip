cmake_minimum_required(VERSION 2.6)

PROJECT(pdip C)

# Set where to start looking for *nix programs
IF(WIN32)
  SET(POSIX_ROOT "C:/Program\ Files/Git")
ELSE()
  SET(POSIX_ROOT "")
ENDIF()

INCLUDE (FindGZIP.cmake)

INCLUDE(version.cmake)


# Define GNU standard installation directories
#
# This defines several variables:
#
# CMAKE_INSTALL_<dir>      - destination for files of a given type
# CMAKE_INSTALL_FULL_<dir> - corresponding absolute path
#
# where <dir> is:
#
#   BINDIR           - user executables (bin)
#   SBINDIR          - system admin executables (sbin)
#   LIBEXECDIR       - program executables (libexec)
#   SYSCONFDIR       - read-only single-machine data (etc)
#   SHAREDSTATEDIR   - modifiable architecture-independent data (com)
#   LOCALSTATEDIR    - modifiable single-machine data (var)
#   LIBDIR           - object code libraries (lib or lib64 or
#                      lib/<multiarch-tuple> on Debian)
#   INCLUDEDIR       - C header files (include)
#   OLDINCLUDEDIR    - C header files for non-gcc (/usr/include)
#   DATAROOTDIR      - read-only architecture-independent data root (share)
#   DATADIR          - read-only architecture-independent data (DATAROOTDIR)
#   INFODIR          - info documentation (DATAROOTDIR/info)
#   LOCALEDIR        - locale-dependent data (DATAROOTDIR/locale)
#   MANDIR           - man documentation (DATAROOTDIR/man)
#   DOCDIR           - documentation root (DATAROOTDIR/doc/PROJECT_NAME)
INCLUDE(GNUInstallDirs)


MESSAGE(STATUS "Building PDIP version ${PDIP_VERSION}")

#CONFIGURE_FILE(config.h.cmake config.h)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})


SET(pdip_man_api_src_3 pdip_configure.3 pdip_lib_initialize.3 pdip_signal_handler.3 pdip_init_cfg.3 pdip_new.3 pdip_delete.3 pdip_exec.3 pdip_fd.3 pdip_status.3 pdip_set_debug_level.3 pdip_send.3 pdip_recv.3 pdip_sig.3 pdip_flush.3 pdip_cpu_nb.3 pdip_cpu_alloc.3 pdip_cpu_free.3 pdip_cpu_zero.3 pdip_cpu_all.3 pdip_cpu_set.3 pdip_cpu_unset.3 pdip_cpu_isset.3)

SET(pdip_man_src_3 ${pdip_man_api_src_3} pdip_en.3 pdip_fr.3 pdip_cpu_en.3 pdip_cpu_fr.3)

SET(pdip_man_src_1 pdip_en.1 pdip_fr.1)

STRING(REGEX REPLACE ".1" ".1.gz" pdip_man_gz_1 "${pdip_man_src_1}")
STRING(REGEX REPLACE ".3" ".3.gz" pdip_man_gz_3 "${pdip_man_src_3}")
STRING(REGEX REPLACE ".3" ".3.gz" pdip_man_api_gz_3 "${pdip_man_api_src_3}")


ADD_DEFINITIONS(-g -O2 -fsigned-char -freg-struct-return -Wall -W -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-qual -Winline -Werror -pthread)


if (CMAKE_COVERAGE)

  include(CodeCoverage.cmake)

  APPEND_COVERAGE_COMPILER_FLAGS()

endif()


# Compression of the manuals
FOREACH(man ${pdip_man_src_1} ${pdip_man_src_3})
  ADD_CUSTOM_COMMAND(OUTPUT ${man}.gz
                     COMMAND ${GZIP_TOOL} -c ${man} > ${man}.gz
                     DEPENDS ${man}
                     COMMENT "Building ${man}.gz"
                    )
ENDFOREACH(man)

# Build the manuals
ADD_CUSTOM_TARGET(pdip_man ALL DEPENDS ${pdip_man_gz_1} ${pdip_man_gz_3})

# Build the library
SET(PDIP_LIB_SRC pdip_lib.c pdip_util.c)
ADD_LIBRARY(pdip SHARED ${PDIP_LIB_SRC})


# Build of the program
ADD_EXECUTABLE(pdip_exe pdip.c pdip_util.c)
SET_TARGET_PROPERTIES(pdip_exe PROPERTIES OUTPUT_NAME pdip)

# Installation of the library
INSTALL(TARGETS pdip
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)

# Installation of the program
INSTALL(TARGETS pdip_exe
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

# Installation of the header file
INSTALL(FILES pdip.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)

# Installation of the manuals
INSTALL(FILES pdip_fr.1.gz
        DESTINATION ${CMAKE_INSTALL_MANDIR}/fr/man1
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ RENAME pdip.1.gz)
INSTALL(FILES pdip_en.1.gz
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ RENAME pdip.1.gz)

INSTALL(FILES pdip_fr.3.gz
        DESTINATION ${CMAKE_INSTALL_MANDIR}/fr/man3
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ RENAME pdip.3.gz)
INSTALL(FILES pdip_en.3.gz
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man3
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ RENAME pdip.3.gz)

INSTALL(FILES pdip_cpu_fr.3.gz
        DESTINATION ${CMAKE_INSTALL_MANDIR}/fr/man3
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ RENAME pdip_cpu.3.gz)
INSTALL(FILES pdip_cpu_en.3.gz
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man3
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ RENAME pdip_cpu.3.gz)

FOREACH(man ${pdip_man_api_gz_3})
  INSTALL(FILES ${man}
          DESTINATION ${CMAKE_INSTALL_MANDIR}/man3
          PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)
  INSTALL(FILES ${man}
          DESTINATION ${CMAKE_INSTALL_MANDIR}/fr/man3
          PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)
ENDFOREACH(man)

# Get the user identifier
IF(WIN32)
  SET(USER_ID_COMMAND "${CMAKE_SOURCE_DIR}/GetUserID.bat")
ELSE()
  SET(USER_ID_COMMAND "id -u")
ENDIF()
EXECUTE_PROCESS(COMMAND ${USER_ID_COMMAND} OUTPUT_VARIABLE USER_ID OUTPUT_STRIP_TRAILING_WHITESPACE)

MESSAGE("The user id is ${USER_ID}")

# Check if current user is super user
IF(WIN32)
  EXECUTE_PROCESS(COMMAND "${CMAKE_SOURCE_DIR}/IsAdmin.bat" OUTPUT_VARIABLE IS_ADMIN OUTPUT_STRIP_TRAILING_WHITESPACE)
ELSE()
  IF(${USER_ID} EQUAL 0)
    SET(IS_ADMIN 1)
  ELSE()
    SET(IS_ADMIN 0)
  ENDIF()
ENDIF()

# We change the rights accesses only if we are running as super user
IF(${IS_ADMIN})
  INSTALL(SCRIPT pdip_chown.cmake)
ENDIF()


#INCLUDE_DIRECTORIES(.)

ADD_SUBDIRECTORY(test)
ADD_SUBDIRECTORY(isys)
ADD_SUBDIRECTORY(rsys)

