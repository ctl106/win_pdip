#!/bin/bash
#
# Simple script to build/install PDIP
#
#  Copyright (C) 2007,2018 Rachid Koucha <rachid dot koucha at gmail dot com>
#
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


TMPDIR="/tmp/pdip_$$"

PDIP_SOURCES="ChangeLog.txt          \
              CMakeLists.txt         \
              FindGZIP.cmake         \
              FindCheck.cmake        \
              FindPdip.cmake         \
              isys/FindIsys.cmake    \
              rsys/FindRsys.cmake    \
              pdip_chown.cmake       \
              CodeCoverage.cmake     \
              pdip.c                 \
              pdip_lib.c             \
              pdip_util.c            \
              pdip.h                 \
              pdip_p.h               \
              pdip_util.h            \
              pdip_deb.ctrl          \
              pdip_deb.postinst      \
              pdip_en.1              \
              pdip_fr.1              \
              pdip_en.3              \
              pdip_fr.3              \
              pdip_configure.3       \
              pdip_signal_handler.3  \
              pdip_init_cfg.3        \
              pdip_new.3             \
              pdip_delete.3          \
              pdip_exec.3            \
              pdip_fd.3              \
              pdip_status.3          \
              pdip_set_debug_level.3 \
              pdip_send.3            \
              pdip_recv.3            \
              pdip_sig.3             \
              pdip_flush.3           \
              pdip_lib_initialize.3  \
              pdip_cpu_en.3          \
              pdip_cpu_fr.3          \
              pdip_cpu_nb.3          \
              pdip_cpu_alloc.3       \
              pdip_cpu_free.3        \
              pdip_cpu_zero.3        \
              pdip_cpu_all.3         \
              pdip_cpu_set.3         \
              pdip_cpu_unset.3       \
              pdip_cpu_isset.3       \
              pdip_install.sh        \
              README.txt             \
              COPYING                \
              COPYING.LESSER         \
              AUTHORS                \
              config.h.cmake         \
              version.cmake          \
              glmf_logo.gif"

PDIP_TESTS="test/CMakeLists.txt    \
            test/man_exe_1.c       \
            test/man_exe_2.c       \
            test/man_exe_3.c       \
            test/tpdip.c           \
            test/tisys.c           \
            test/trsys.c           \
            test/check_all.c       \
            test/check_all.h       \
            test/check_pdip.c      \
            test/check_pdip.h      \
            test/check_pdip_errcodes.c \
            test/check_pdip_api.c  \
            test/check_isys.c      \
            test/check_isys.h      \
            test/check_isys_errcodes.c \
            test/check_isys_api.c  \
            test/check_isys_api_env.c \
            test/check_rsys.c      \
            test/check_rsys.h      \
            test/check_rsys_errcodes.c  \
            test/check_rsys_api.c       \
            test/check_rsys_util.c      \
            test/check_rsys_rsystemd.c  \
            test/myaffinity.c      \
            test/pdata.c           \
            test/pterm.c"


ISYS_SOURCES="isys/CMakeLists.txt    \
              isys/isys.h            \
              isys/isys.c            \
              isys/isys_en.3         \
              isys/isystem.3         \
              isys/isys_lib_initialize.3 \
              isys/isys_deb.ctrl     \
              isys/isys_deb.postinst"


RSYS_SOURCES="rsys/CMakeLists.txt    \
              rsys/rsys.h            \
              rsys/rsys_p.h          \
              rsys/rsystem.c         \
              rsys/rsys_msg.c        \
              rsys/rsystemd.c        \
              rsys/rsystemd_en.8     \
              rsys/rsys_en.3         \
              rsys/rsystem.3         \
              rsys/rsys_lib_initialize.3 \
              rsys/rsys_deb.ctrl     \
              rsys/rsys_deb.postinst"


PDIP_DOCS="doc/index.html         \
           doc/state_diagram.png  \
           doc/state_diagram.odp  \
           doc/system_optimization.html \
           doc/system_optimization.odt  \
           doc/system_optimization.pdf  \
           doc/figures.odp        \
           doc/figure_1.png       \
           doc/figure_2.png       \
           doc/figure_3.png       \
           doc/figure_4.png       \
           doc/figure_5.png       \
           doc/figure_6.png       \
           doc/glmf_logo.jpg      \
           doc/coverage.txt       \
           doc/regression.txt"

cleanup()
{
  if [ -d ${TMPDIR} ]
  then
    rm -rf ${TMPDIR}
  fi
}

trap 'cleanup' HUP INT EXIT TERM QUIT


# Default values
INST_DIR=/usr/local
BUILD_IT=0
INSTALL_IT=0
DEB_PACKAGE_IT=NO
RPM_PACKAGE_IT=NO
ARCHIVE_IT=0
CLEANUP=0

# User manual
help()
{
{
echo
echo Usage: `basename $1` "[-c] [-d install_root_dir] [-B] [-I] [-A] [-P RPM|DEB] [-h]"
echo
echo '             -c : Cleanup built objects'
echo "             -d : Installation directory (default: ${INST_DIR})"
echo '             -P : Generate a DEB or RPM package'
echo '             -B : Build the software'
echo '             -I : Install the software'
echo '             -A : Generate an archive of the software (sources)'
echo '             -h : this help'
} 1>&2
}



# If no arguments ==> Display help
if [ $# -eq 0 ]
then
  help $0
  exit 1
fi

# Make sure that we are running in the source directory
if [ ! -f pdip.c ]
then
  echo This script must be run in the source directory of PDIP >&2
  exit 1
fi

# Parse the command line
while getopts cd:P:BIAh arg
do
  case ${arg} in
    c) CLEANUP=1;;
    d) INST_DIR=${OPTARG};;
    P) if [ ${OPTARG} = "DEB" ]
       then DEB_PACKAGE_IT=YES
       elif [ ${OPTARG} = "RPM" ]
       then RPM_PACKAGE_IT=YES
       else echo Unknown package type \'${OPTARG}\' >&2
            help $0
            exit 1
       fi;;
    B) BUILD_IT=1;;
    I) INSTALL_IT=1;;
    A) ARCHIVE_IT=1;;
    h) help $0
       exit 0;;
    *) help $0
       exit 1;;
  esac
done

shift $((${OPTIND} - 1))

# Check the arguments
if [ -n "$1" ]
then
  echo Too many arguments >&2
  help $0
  exit 1
fi

if [ ${CLEANUP} -eq 1 ]
then

  echo Cleanup...

  # Remove PDF file ?

  # Launch cmake cleanup in case it works
  make clean > /dev/null 2>&1

  #
  # Cleanup test coverage stuff
  #
  rm -rf all_coverage pdip_coverage isys_coverage rsys_coverage

  # Remove core files
  find . -type f -name core -exec rm -f {} \; -print

  # Delete emacs backup files
  find . -type f -name \*~ -exec rm -f {} \; -print

  # Delete the packages
  find . -type f \( -name \*.tgz -o -name \*.deb -o -name \*.rpm \) -exec rm -f {} \; -print

  # Delete the pkg-config files
  find . -type f -name \*.pc -exec rm -f {} \; -print

  # Delete the files generated by cmake
  rm -f CMakeCache.txt config.h install_manifest.txt
  find . -type f -name Makefile -exec rm -f {} \; -print
  find . -type f -name cmake_install.cmake -exec rm -f {} \; -print
  find . -type d -name CMakeFiles -exec rm -rf {} \; -print 2>/dev/null

  # Delete the compressed mans
  find . -type f \( -name pdip\*.gz -o -name isys\*.gz -o -name  rsys\*.gz \) -exec rm -f {} \; -print

  # Delete various common objects
  find . -type f -name a.out -exec rm -f {} \; -print
  rm -f pdip libpdip.so
  rm -f test/check_all test/check_pdip test/check_isys test/check_rsys test/tisys test/tpdip test/man_exe_1 test/man_exe_2 test/man_exe_3
  rm -f isys/libisys.so rsys/librsys.so

  # Delete the files generated by the tests
  rm -f test/rsys_socket_*

fi

# Make sure that cmake is installed if build or installation or packaging is requested
if [ ${BUILD_IT} -eq 1 -o ${INSTALL_IT} -eq 1 -o ${DEB_PACKAGE_IT} != "NO" -o ${RPM_PACKAGE_IT} != "NO" ]
then
  which cmake > /dev/null 2>&1
  if [ $? -ne 0 ]
  then
    echo To be able to compile/install PDIP, you must install cmake and/or update the PATH variable >&2
    exit 1
  fi

  # Launch cmake
  echo Configuring PDIP installation in ${INST_DIR}...
  cmake . -DCMAKE_INSTALL_PREFIX=${INST_DIR}
fi

# If archive is requested
if [ ${ARCHIVE_IT} -eq 1 ]
then

  which tar > /dev/null 2>&1
  if [ $? -ne 0 ]
  then
    echo "To be able to generate a PDIP archive, you must install 'tar' and/or update the PATH variable" >&2
    exit 1
  fi

  # Launch the build
  make

  # Get PDIP's version
  PDIP_VERSION=`./pdip -V | cut -d' ' -f3`
  if [ -z "${PDIP_VERSION}" ]
  then
    echo Something went wrong while building PDIP >&2
    exit 1
  fi

  ARCHIVE_DIR=pdip-${PDIP_VERSION}
  ARCHIVE_NAME=${ARCHIVE_DIR}.tgz
  mkdir -p ${TMPDIR}/${ARCHIVE_DIR}/test
  mkdir -p ${TMPDIR}/${ARCHIVE_DIR}/isys
  mkdir -p ${TMPDIR}/${ARCHIVE_DIR}/rsys
  mkdir -p ${TMPDIR}/${ARCHIVE_DIR}/doc
  cp ${PDIP_SOURCES} ${TMPDIR}/${ARCHIVE_DIR}
  cp ${PDIP_TESTS} ${TMPDIR}/${ARCHIVE_DIR}/test
  cp ${ISYS_SOURCES} ${TMPDIR}/${ARCHIVE_DIR}/isys
  cp ${RSYS_SOURCES} ${TMPDIR}/${ARCHIVE_DIR}/rsys
  cp ${PDIP_DOCS} ${TMPDIR}/${ARCHIVE_DIR}/doc
  echo Building archive ${ARCHIVE_NAME}...
  tar cvfz ${ARCHIVE_NAME} -C ${TMPDIR} ${ARCHIVE_DIR} > /dev/null 2>&1

  rm -rf ${TMPDIR}/${ARCHIVE_DIR}
fi

# If build is requested
if [ ${BUILD_IT} -eq 1 ]
then
  make
fi

# If installation is requested
if [ ${INSTALL_IT} -eq 1 ]
then
  make install
fi


if [ ${DEB_PACKAGE_IT} != "NO" -o ${RPM_PACKAGE_IT} != "NO" ]
then

  # Make sure that DPKG is installed
  which dpkg > /dev/null 2>&1
  if [ $? -ne 0 ]
  then
    echo "To be able to generate a DEB or RPM PDIP package, you must install 'dpkg' and/or update the PATH variable" >&2
    exit 1
  fi

  # Make sure that ALIEN is installed
  if [ ${RPM_PACKAGE_IT} != "NO" ]
  then
    which alien > /dev/null 2>&1
    if [ $? -ne 0 ]
    then
      echo "To be able to generate a RPM PDIP package, you must install 'alien' and/or update the PATH variable" >&2
      exit 1
    fi
  fi

  # Make sure that sed is installed
  which sed > /dev/null 2>&1
  if [ $? -ne 0 ]
  then
    echo "To be able to generate a DEB PDIP pakage, you must install 'sed' and/or update the PATH variable" >&2
    exit 1
  fi

  # Launch the build
  make

  # Get PDIP's version
  PDIP_VERSION=`./pdip -V | cut -d' ' -f3`
  if [ -z "${PDIP_VERSION}" ]
  then
    echo Something went wrong while building PDIP >&2
    exit 1
  fi

  # Get architecture
  PDIP_ARCH=`dpkg --print-architecture`

  # Build the pkg-config configuration file
  {
  echo "prefix=${INST_DIR}"
  echo "exec_prefix=\${prefix}/bin"
  echo "libdir=\${prefix}/lib"
  echo "includedir=\${prefix}/include"
  echo
  echo "Name: pdip"
  echo "Description: PDIP (Programmed Dialogue with Interactive Programs)"
  echo "Version: ${PDIP_VERSION}"
  echo "URL: http://pdip.sourceforge.net"
  echo "Libs: -lpdip -lpthread"
  echo "Cflags: -I\${includedir}"
  } > pdip.pc



  # Reproduce the installation tree in the temporary directory
  if [ ${DEB_PACKAGE_IT} != "NO" ]
  then
    echo "Making the PDIP DEB package (version ${PDIP_VERSION}, architecture ${PDIP_ARCH})..."
  fi
  mkdir -p ${TMPDIR}/${INST_DIR}
  mkdir -p ${TMPDIR}/${INST_DIR}/bin
  mkdir -p ${TMPDIR}/${INST_DIR}/lib
  mkdir -p ${TMPDIR}/${INST_DIR}/lib/cmake
  mkdir -p ${TMPDIR}/${INST_DIR}/lib/pkgconfig
  mkdir -p ${TMPDIR}/${INST_DIR}/include
  mkdir -p ${TMPDIR}/${INST_DIR}/share/man/man1
  mkdir -p ${TMPDIR}/${INST_DIR}/share/man/fr/man1
  mkdir -p ${TMPDIR}/${INST_DIR}/share/man/man3
  mkdir -p ${TMPDIR}/${INST_DIR}/share/man/fr/man3
  mkdir -p ${TMPDIR}/DEBIAN
  cp FindPdip.cmake ${TMPDIR}/${INST_DIR}/lib/cmake
  cp pdip.pc ${TMPDIR}/${INST_DIR}/lib/pkgconfig
  cp pdip ${TMPDIR}/${INST_DIR}/bin
  cp libpdip.so ${TMPDIR}/${INST_DIR}/lib
  cp pdip.h ${TMPDIR}/${INST_DIR}/include
  cp pdip_en.1.gz ${TMPDIR}/${INST_DIR}/share/man/man1/pdip.1.gz
  cp pdip_fr.1.gz ${TMPDIR}/${INST_DIR}/share/man/fr/man1/pdip.1.gz
  cp pdip_*.3.gz  ${TMPDIR}/${INST_DIR}/share/man/man3/
  cp pdip_*.3.gz  ${TMPDIR}/${INST_DIR}/share/man/fr/man3/
  rm ${TMPDIR}/${INST_DIR}/share/man/man3/pdip_cpu_fr.3.gz
  rm ${TMPDIR}/${INST_DIR}/share/man/man3/pdip_fr.3.gz
  mv ${TMPDIR}/${INST_DIR}/share/man/man3/pdip_en.3.gz ${TMPDIR}/${INST_DIR}/share/man/man3/pdip.3.gz
  mv ${TMPDIR}/${INST_DIR}/share/man/man3/pdip_cpu_en.3.gz ${TMPDIR}/${INST_DIR}/share/man/man3/pdip_cpu.3.gz
  rm ${TMPDIR}/${INST_DIR}/share/man/fr/man3/pdip_cpu_en.3.gz
  rm ${TMPDIR}/${INST_DIR}/share/man/fr/man3/pdip_en.3.gz
  mv ${TMPDIR}/${INST_DIR}/share/man/fr/man3/pdip_fr.3.gz ${TMPDIR}/${INST_DIR}/share/man/fr/man3/pdip.3.gz
  mv ${TMPDIR}/${INST_DIR}/share/man/fr/man3/pdip_cpu_fr.3.gz ${TMPDIR}/${INST_DIR}/share/man/fr/man3/pdip_cpu.3.gz
  # Update the version
  cat pdip_deb.ctrl | sed "s/^Version:.*$/Version: ${PDIP_VERSION}/g" > ${TMPDIR}/DEBIAN/control_1
  # Update the architecture
  cat ${TMPDIR}/DEBIAN/control_1 | sed "s/^Architecture:.*$/Architecture: ${PDIP_ARCH}/g" > ${TMPDIR}/DEBIAN/control
  rm ${TMPDIR}/DEBIAN/control_1
  # Update the install prefix in the post installation script
  cat pdip_deb.postinst | sed "s%^INSTALL_PREFIX=.*$%INSTALL_PREFIX=${INST_DIR}%g" > ${TMPDIR}/DEBIAN/postinst
  chmod +x ${TMPDIR}/DEBIAN/postinst

  # Generate the DEB package
  dpkg -b ${TMPDIR} . # >/dev/null 2>&1

  # Generate the RPM package if requested
  if [ ${RPM_PACKAGE_IT} != "NO" ]
  then
    echo Making the RPM package...
    alien --to-rpm pdip_${PDIP_VERSION}_${PDIP_ARCH}.deb --scripts
  fi


  #
  # Package for ISYS
  #

  if [ ${DEB_PACKAGE_IT} != "NO" ]
  then
    echo "Making the ISYS package (version ${PDIP_VERSION}, architecture ${PDIP_ARCH})..."
  fi

  # cleanup
  if [ -d ${TMPDIR} ]
  then
    rm -rf ${TMPDIR}
  fi

  # Build the pkg-config configuration file
  {
  echo "prefix=${INST_DIR}"
  echo "exec_prefix=\${prefix}/bin"
  echo "libdir=\${prefix}/lib"
  echo "includedir=\${prefix}/include"
  echo
  echo "Name: isys"
  echo "Description: system() service based on a remanent background shell"
  echo "Version: ${PDIP_VERSION}"
  echo "URL: http://pdip.sourceforge.net"
  echo "Libs: -lisys"
  echo "Requires: pdip >= 2.4.3"
  echo "Cflags: -I\${includedir}"
  } > isys/isys.pc

  mkdir -p ${TMPDIR}/${INST_DIR}
  mkdir -p ${TMPDIR}/${INST_DIR}/lib
  mkdir -p ${TMPDIR}/${INST_DIR}/lib/cmake
  mkdir -p ${TMPDIR}/${INST_DIR}/lib/pkgconfig
  mkdir -p ${TMPDIR}/${INST_DIR}/include
  mkdir -p ${TMPDIR}/${INST_DIR}/share/man/man3
  mkdir -p ${TMPDIR}/DEBIAN
  cp isys/FindIsys.cmake ${TMPDIR}/${INST_DIR}/lib/cmake
  cp isys/isys.pc ${TMPDIR}/${INST_DIR}/lib/pkgconfig
  cp isys/libisys.so ${TMPDIR}/${INST_DIR}/lib
  cp isys/isys.h ${TMPDIR}/${INST_DIR}/include
  cp isys/isys_en.3.gz  ${TMPDIR}/${INST_DIR}/share/man/man3/isys.3.gz
  cp isys/isystem.3.gz  ${TMPDIR}/${INST_DIR}/share/man/man3/
  cp isys/isys_lib_initialize.3.gz  ${TMPDIR}/${INST_DIR}/share/man/man3/

  # Update the version
  cat isys/isys_deb.ctrl | sed "s/^Version:.*$/Version: ${PDIP_VERSION}/g" > ${TMPDIR}/DEBIAN/control_1
  # Update the architecture
  cat ${TMPDIR}/DEBIAN/control_1 | sed "s/^Architecture:.*$/Architecture: ${PDIP_ARCH}/g" > ${TMPDIR}/DEBIAN/control
  rm ${TMPDIR}/DEBIAN/control_1
  # Update the install prefix in the post installation script
  cat isys/isys_deb.postinst | sed "s%^INSTALL_PREFIX=.*$%INSTALL_PREFIX=${INST_DIR}%g" > ${TMPDIR}/DEBIAN/postinst
  chmod +x ${TMPDIR}/DEBIAN/postinst

  # Generate the DEB package
  dpkg -b ${TMPDIR} isys #>/dev/null 2>&1

  # Generate the RPM package if requested
  if [ ${RPM_PACKAGE_IT} != "NO" ]
  then
    echo Making the RPM package...
    pushd isys > /dev/null
    alien --to-rpm isys_${PDIP_VERSION}_${PDIP_ARCH}.deb --scripts
    popd > /dev/null
  fi


  #
  # Package for RSYS
  #

  if [ ${DEB_PACKAGE_IT} != "NO" ]
  then
    echo "Making the RSYS package (version ${PDIP_VERSION}, architecture ${PDIP_ARCH})..."
  fi

  # cleanup
  if [ -d ${TMPDIR} ]
  then
    rm -rf ${TMPDIR}
  fi

  # Build the pkg-config configuration file
  {
  echo "prefix=${INST_DIR}"
  echo "exec_prefix=\${prefix}/bin"
  echo "libdir=\${prefix}/lib"
  echo "includedir=\${prefix}/include"
  echo
  echo "Name: rsys"
  echo "Description: system() service based on shared background shells"
  echo "Version: ${PDIP_VERSION}"
  echo "URL: http://pdip.sourceforge.net"
  echo "Libs: -lrsys"
  echo "Requires: pdip >= 2.4.3"
  echo "Cflags: -I\${includedir}"
  } > rsys/rsys.pc

  mkdir -p ${TMPDIR}/${INST_DIR}
  mkdir -p ${TMPDIR}/${INST_DIR}/sbin
  mkdir -p ${TMPDIR}/${INST_DIR}/lib
  mkdir -p ${TMPDIR}/${INST_DIR}/lib/cmake
  mkdir -p ${TMPDIR}/${INST_DIR}/lib/pkgconfig
  mkdir -p ${TMPDIR}/${INST_DIR}/include
  mkdir -p ${TMPDIR}/${INST_DIR}/share/man/man3
  mkdir -p ${TMPDIR}/${INST_DIR}/share/man/man8
  mkdir -p ${TMPDIR}/DEBIAN
  cp rsys/FindRsys.cmake ${TMPDIR}/${INST_DIR}/lib/cmake
  cp rsys/rsys.pc ${TMPDIR}/${INST_DIR}/lib/pkgconfig
  cp rsys/rsystemd ${TMPDIR}/${INST_DIR}/sbin
  cp rsys/librsys.so ${TMPDIR}/${INST_DIR}/lib
  cp rsys/rsys.h ${TMPDIR}/${INST_DIR}/include
  cp rsys/rsys_en.3.gz  ${TMPDIR}/${INST_DIR}/share/man/man3/rsys.3.gz
  cp rsys/rsystem.3.gz  ${TMPDIR}/${INST_DIR}/share/man/man3
  cp rsys/rsys_lib_initialize.3.gz  ${TMPDIR}/${INST_DIR}/share/man/man3
  cp rsys/rsystemd_en.8.gz  ${TMPDIR}/${INST_DIR}/share/man/man8/rsystemd.8.gz

  # Update the version
  cat rsys/rsys_deb.ctrl | sed "s/^Version:.*$/Version: ${PDIP_VERSION}/g" > ${TMPDIR}/DEBIAN/control_1
  # Update the architecture
  cat ${TMPDIR}/DEBIAN/control_1 | sed "s/^Architecture:.*$/Architecture: ${PDIP_ARCH}/g" > ${TMPDIR}/DEBIAN/control
  rm ${TMPDIR}/DEBIAN/control_1
  # Update the install prefix in the post installation script
  cat rsys/rsys_deb.postinst | sed "s%^INSTALL_PREFIX=.*$%INSTALL_PREFIX=${INST_DIR}%g" > ${TMPDIR}/DEBIAN/postinst
  chmod +x ${TMPDIR}/DEBIAN/postinst

  # Generate the DEB package
  dpkg -b ${TMPDIR} rsys # >/dev/null 2>&1

  # Generate the RPM package if requested
  if [ ${RPM_PACKAGE_IT} != "NO" ]
  then
    echo Making the RPM package...
    pushd rsys > /dev/null
    alien --to-rpm rsys_${PDIP_VERSION}_${PDIP_ARCH}.deb --scripts
    popd > /dev/null
  fi


fi
