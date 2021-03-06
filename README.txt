Version 2.4.4 (29-May-2018)
=============

     . pdip_lib.c: Fixed a compilation error because on some machines, regoff_t
                   may be "int" (ubuntu Linux) or "long int" (Alpine Linux)
     . Transfered FindIsys/Rsys.cmake respectively in isys/rsys sub-directories
     . pdip_install.sh:
          . Packaging/Installation of Find/Isys/Rsys.cmake
     . ISYS
         . isys_deb.postinst: Added access right management for FindIsys.cmake
     . RSYS
         . rsys_deb.postinst: Added access right management for FindRsys.cmake
         . rsystemd.c: Inclusion of <string.h> was missing for memset()


Version 2.4.3 (03-May-2018)
=============

     . Test suite:
         . Suppressed the useless surrounding braces in unit tests
           as START_TEST/END_TEST macros respectively include the
           opening/closing braces
         . Replaced fork() by check_fork() for robustness as the latter
           put the child process in the process group of the unit test process
           ==> This facilitates its termination in case of failure
     . doc/index.html:
         . Fixed broken link for the reference to EXPECT tool
         . Added a reference to ISYS and RSYS as examples of applications using
           PDIP library
     . pdip_install.sh:
         . Added pdip/rsys/isys library respectively in pdip.pc/isys.pc/rsys.pc
           as the dependency mechanism (i.e. "Requires" field) makes
           pkg-config retrieve all the needed libraries

           e.g. $ pkg-config --libs pdip
                -lpdip -lpthread
                $ pkg-config --libs isys
                -lisys -lpdip -lpthread
                $ pkg-config --libs rsys
                -lrsys -lpdip -lpthread

     . test/CMakeLists.txt
         . Added double quotes around PKG_CONFIG variables to get a correct
           display of the libraries when cmake is configuring



Version 2.4.2 (10-Apr-2018)
=============

     . Moved all build files concerning ISYS and RSYS into their respective
       directories
     . RSYS:
          . rsystemd.c:
                 . Updated the program's help
                 . Cleanup of the shells upon termination
                 . Added daemon mode (-D option)
                   ==> Updated on line manual, doc/* and regression tests
                       accordingly
     . pdip_install.sh:
          . doc/* files were missing in the archive



Version 2.4.1 (05-Apr-2018)
=============

     . Updated doc/regression.txt to explain how to launch all the regression
       tests
     . RSYS/ISYS:
               . Avoid dynamic allocation in isystem()/rsystem() services when
                 the command line is short (< 256 chars for ISYS and 4096 for RSYS)
               . No longer ignore SIGINT/SIGQUIT in the shell as system() ignores
                 those signals in the father process while running the command
                 (the default disposition is restablished at the end of the command)
                 ==> We don't manage those signals even in isystem() code as this
                     is not adapted to multithreaded applications
               . Updated the test suite accordingly



Version 2.4.0 (02-Apr-2018)
=============

     . PDIP: . Redesigned the process termination to make it
               more flexible for users (e.g. ISYS and RSYS libraries)
             . Bug fix: the number of CPU was not updated in child processes
             . Updated the on line manuals as well as doc/index.html for
               pdip_configure() API
     . TESTS: . Simplification of the test suite (only one suite instead of
                multiple suites per sub-element: PDIP, RSYS and ISYS)
              . Global executable to launch all the tests: test/check_all
              . Added robustness in the procedure killing/waiting rsystemd
                server termination
              . Take in account EINTR when invoking waitpid()
              . Added global test coverage (cf. doc/coverage.txt)
              . Increased test coverage
     . RSYS: Made sure that exit code of RSYSTEMD is 2 upon signal receipt
     . ISYS: . Configure PDIP library with external SIGCHLD signal handler
               (pdip_configure(1, 0))
             . PDIP cleanup at library unloading
 


Version 2.3.3 (26-Mar-2018)
=============

     . pdip_install.sh: Added generation of pkg-config files: pdip.pc, isys.pc
                        and rsys.pc
     . FindIsys.cmake
       FindRsys.cmake
       FindPdip.cmake: New files to help the build of cmake based packages using PDIP,
                       ISYS and RSYS
     . Updated doc/system_optimization* to present the preceding files
     . test/tisys.c
       test/trsys.c: Change pathname of "isys.h" and "rsys.h" to make the
                     test compile in a virgin system where ISYS and RSYS are
                     not installed


Version 2.3.2 (22-Mar-2018)
=============

     . Added doc/regression.txt to present the regression tests
     . Updated doc/system_optimization.*
     . PDIP, ISYS, RSYS:
         . Increased test coverage
         . Moved tisys.c and trsys.c in test directory
     . RSYS
         . rsystemd:
                . Added cleanup of the socket file upon exit
                . Added "-d" option for debug level
                  ==> Updated on line manual accordingly


Version 2.3.1 (21-Mar-2018)
=============

     . Packaging of isys/rsys_lib_initialize on line manuals
     . RSYS:
         . Increased test coverage
         . Some enhancements in shell parameter analysis
         . Bug fixes and robustness in client's context deallocation


Version 2.3.0 (16-Mar-2018)
=============

     . Better management of fork for PDIP_LIB and ISYS
       ==> Added isys_lib_initialize() and pdip_lib_initialize() services
       to be called in a child process if one need to use those services in
       the childs
     . Use local header files in test sources files (e.g. "../pdip.h") instead
       of installed ones in case the package is not installed yet on the target
     . Added GCC format attribute to pdip_send(), isystem() and rsystem()
       to detect format errors at compilation time
     . Bug fix: The value of the status returned by isystem()/rsystem()
                reflects the Linux rules after a better
                understanding of the content of $? shell variable
     . PDIP:
        . Increased test code coverage for pdip_util.c
          (total for PDIP is 83.4% lines covered as more error branches
          difficult to enter in are added with new APIs)
     . ISYS:
         . Added PDIP_FLAG_RECV_ON_THE_FLOW in the configuration of the PDIP
           object to receive the data on the flow from verbose applications
           ==> This saves dynamic memory
         . Added test coverage (~70.4 % lines covered, uncovered lines are
           error branches difficult to enter in)
         . Added ISYS_TIMEOUT environment variable to specify the maximum time
           in seconds to wait for data from the shell
         . Updated the on line manuals
     . RSYS:
         . Complete redesign of the server to manage multiple requests from
           the same client
         . Added RSYS_SOCKET_PATH environment variable to specify an alternate
           pathname for the server socket (useful for unitary tests)
         . Updated on line manual concerning default number of shell and
           affinity for rsystemd
     . Updated the documents in doc/*



Version 2.2.2 (24-Feb-2018)
=============

     . Increased test code coverage for pdip_lib.c (84% lines covered)
     . Updated on line manuals
     . pdip_lib.c: Some source code enhancements and bug fixes


Version 2.2.1 (29-Jan-2018)
=============

      . Added examples in "man rsystem" and "man isystem"
      . PDIP_LIB:
         . Secured the killing of the controlled process to avoid to
           trigger a kill(-1, SIGTERM/KILL)
         . Some small fixes revealed by the test coverage measurement
     . Added unitary tests based on CHECK package
     . Added code coverage measurement based on GCOV/LCOV (cf. doc/coverage.txt)
       ==> This is the beginning of the unit test and only for PDIP_LIB
       ==> More will come in subsequent editions


Version 2.2.0 (25-Jan-2018)
=============

      . PDIP_LIB:
          . Object configuration (pdip_cfg_t):
             . Added PDIP_FLAG_RECV_ON_THE_FLOW to save memory space on user
               side (no longer need to get all the data from the program in
               one shot)
             . Added buf_resize_increment field
          . Bug fix and redesign of pdip_recv():
             . When a timeout is passed, suppressed the internal read
               reiterations: any read data is immediately returned to the user.
             . If PDIP_RECV_ERROR is returned, there are no longer received
               data in the buffers
             . If there are outstanding data, they returned immediately if
               a read request without regex is invoked
          . Bug fix in pdip_flush_internal(): the computation of the data
            to flush was completely wrong !!!
      . RSYS:
          . Bug fix: when -s (--shells) is not passed to rsystemd, a crash
            occured
          . Increase dependency version on PDIP as it uses latest goodies



Version 2.1.1 (24-Jan-2018)
=============

      . RSYSTEMD:
           . Management of the "--shells" option
           . Management of RSYSD_SHELLS environment variable
      . RSYS package:
           . Increased version of PDIP dependency as it uses new pdip_cpu_zero() service
       . Added pdip_cpu_zero() and pdip_cpu_all() in pdip_cpu API
       . Bug fix in pdip_cpu_alloc() (Reset of bitmap too far away !)
       . Updates in on line manuals and documents


Version 2.1.0 (12-Jan-2018)
=============

       . pdip_xx.3
         index.html: Update of pdip_delete() prototype (i.e. description of
                     "status" argument)
       . CMakeLists.txt: Enhanced the manual management 
       . Added isys and rsys services
       . Added doc directory for addtional documentations and studies



Version 2.0.4 (20-Dec-2017)
=============

       . pdip_signal_handler(): Added error checking with new return code
         PDIP_SIG_ERROR
         ==> Updated documentation accordingly
       . Added pdip_cpu services:
             . On line manuals
             . Ability to set CPU affinity for PDIP objects
       . pdip_signal_handler.3: Manual entry for pdip_signal_handler()
       . pdip_status():
             . Redesigned because of deadlocks when interacting with
               signal handler
             . errno set to EAGAIN if the controlled process is not dead
       . pdip_exec(): interface break (returns pid of the executed program
         instead of 0 when it is OK)
       . pdip_flush(): Bug fixed in the object state checking (&& instead of ||)
       . pdip_install.sh:
             . Added "-c" option for cleanup
             . Auto-detection of architecture for the packages
       . pdip_deb.postinst: Update the rights/owner/group of the new manuals


Version 2.0.3 (11-Dec-2017)
=============

       . Added a "flags" field in the cfg structure passed to pdip_new()
       . pdip_lib.c/pdip_recv(): Fixed a segmentation fault occuring in
         pdip_recv() when the timeout elapses without incoming data
       . Manuals/Documents: Fixed typos, new cfg structure for pdip_new()
       . Test directory: Added a little lib providing an alternate system(3)
         service based on PDIP lib (i.e. isystem())
       . Added missing test/man_exe_2.c in pdip_install.sh


Version 2.0.2 (04-Dec-2017)
=============

       . Changed prototype and behaviour of pdip_status(): added a 3rd
         parameter to make the call blocking or non blocking
       . Updated documents and examples accordingly


Version 2.0.1 (01-Dec-2017)
=============

       . Documents: Added transition diagram of PDIP objects, fixed typos
         and formatting
       . libpdip.so under LGPL instead of GPL

Version 2.0.0 (27-Nov-2017)
=============

       . Added an API with libpdip.so along with its header file (pdip.h),
         online manual (pdip.3) and test directory
         ==> The pattern matching is faster as it is done on multiple lines
             in one shot instead of line by line (as it is done in pdip tool)
       . Added UTF-8 accented characters in the french on line manuals


Version 1.9.0 (09-Jun-2015)
=============

       . Added -R option to make PDIP read internally the data coming from
         the controlled program even if no "recv" command is on track


Version 1.8.12 (18-Mar-2015)
==============

       . Replaced memcpy() by memmove() to avoid memory overlaps
       . Added timeout in flush procedure upon program termination otherwise
         pdip may hang

Version 1.8.11 (06-Nov-2013)
==============

        . Added 'sh' command
        . Display (flush) outstanding data when program finishes prematurely
        . Default buffer size 512 --> 4096
        . pdip_en.1        ==> Updated the manuals
          pdip_fr.1
          index.html

Version 1.8.9 (15-Jul-2012)
=============

        . Compilation with a 64-bit compiler


Version 1.8.8 (31-Aug-2010)
=============
        . version.cmake    ==> 1.8.7 to 1.8.8
        . pdip.c           ==> Added option "-p" to propagate exit code of
                               controlled program (no longer the default
                               behaviour !)
        . pdip_en.1        ==> Updated manuals for "-p" option
          pdip_fr.1
          index.html



Version 1.8.7 (29-Jul-2010)
=============

        . version.cmake    ==> 1.8.6 to 1.8.7
        . pdip_install.sh  ==> Little enhancements
        . config.h.cmake   ==> Updated the copyright
        . pdip.c           ==> Made the slave side of the PTY become the
                               controlling terminal of the controlled
                               program
                               Added management of the control characters
                               through the notation "^character"
                               .Added new keyword "sig" to send signals to
                               the controlled program
       . pdip_en.1         ==> Added manual of "sig" and "^character"
         pdip_fr.1
         index.html


Version 1.8.6 (10-Jul-2010)
=============

        . version.cmake    ==> 1.8.5 to 1.8.6
        . pdip_install.sh  ==> Little enhancements
        . config.h.cmake   ==> Updated the copyright
          pdip.c


Version 1.8.5 (16-Apr-2010)
=============

        . CMakeLists.txt   ==> Added cmake_minimum_required(VERSION 2.6) to
                               avoid warnings
        . version.cmake    ==> 1.8.1 to 1.8.5
        . pdip.c           ==> Added 'print' command
                           ==> Added 'dbg' command with debug level
                           ==> Fixed pdip_write() because it didn't work
                               when multiple writes were necessary
                           ==> pdip_read_program(): Replaced polling by a
                               timed wait 
                           ==> Better management of the end of the program
                           ==> Dynamic allocation of the internal buffers
                               to accept very long lines
                           ==> Do not redirect the error output of the
                               child unless "-e" option is specified
                           ==> Management of background mode
                           ==> Added -t option
        . pdip_en.1        ==> Added description of new options
        . pdip_fr.1        ==> Idem
        . index.html       ==> Idem


Version 1.8.1 (10-Feb-2009)
=============

        . version.cmake    ==> 1.8.0 to 1.8.1
        . pdip.c           ==> Fixed parameter mngt of sleep keyword


Version 1.8.0 (06-Feb-2009)
=============

        . version.cmake    ==> 1.7.0 to 1.8.0
        . pdip.c           ==> Mngt of exception signals
                           ==> Fixed coverity warnings/errors
                           ==> Ability top send ESC character
                           ==> Do not skip blanks in input strings
        . index.html
        . pdip_en.1
        . pdip_fr.1        ==> Added ESC character


Version 1.7.0 (27-Jun-2008)
=============

        . Added version management: version.cmake, config.h.cmake
        . CMakeLists.txt   ==> Generation of config.h
        . Added AUTHORS
        . pdip_install.sh  ==> Added new files

Version 1.6 (27-Jan-2008)
===========

        . pdip.c           ==> Slight changes in usage() function
        . index.html       ==> Update to add a link on an article about pty

Version 1.5 (25-Jan-2008)
===========

        . pdip.c           ==> Suppressed pdip_buf test in pdip_format_params() internal function
        . pdip_install.sh  ==> Suppressed test on user id to be able to install even if not super user
        . CMakeLists.txt   ==> Change access rights only if super user
                           ==> Better cmake rules to make "make clean" work on manuals
        . index.html       ==> Added hyperlink on sourceforge in the title

Version 1.4 (18-Jan-2008)
===========

        . Added file ChangeLog.txt
        . pdip.c          ==> Infinite loop in pdip_write() when piloted program crashes
        . pdip_install.sh ==> Added ChangeLog.txt
                        ==> Mngt of multiple occurences of -P option on cmd line



