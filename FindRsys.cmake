# - Try to find the RSYS libraries
#  Once done this will define
#
#  RSYS_FOUND - system has RSYS
#  RSYS_INCLUDE_DIRS - the RSYS include directory
#  RSYS_LIBRARIES - RSYS library
#
#  This configuration file for finding librsys is derived from the one done
#  done for CHECK library:
#  opensync.org/browser/branches/3rd-party-cmake-modules/modules/FindCheck.cmake
#


INCLUDE( FindPkgConfig )

# Take care about rsys.pc settings
#
# The following sets:
#
#    RSYS_FOUND          ... set to 1 if module(s) exist
#    RSYS_LIBRARIES      ... only the libraries (w/o the '-l')
#    RSYS_LIBRARY_DIRS   ... the paths of the libraries (w/o the '-L')
#    RSYS_LDFLAGS        ... all required linker flags
#    RSYS_LDFLAGS_OTHER  ... all other linker flags
#    RSYS_INCLUDE_DIRS   ... the '-I' preprocessor flags (w/o the '-I')
#    RSYS_CFLAGS         ... all required cflags
#    RSYS_CFLAGS_OTHER   ... the other compiler flags
#
PKG_SEARCH_MODULE( RSYS rsys )

# Look for RSYS include dir and libraries
IF( NOT RSYS_FOUND )
	IF ( RSYS_INSTALL_DIR )
		MESSAGE ( STATUS "Using override RSYS_INSTALL_DIR to find rsys" )
		SET ( RSYS_INCLUDE_DIRS  "${RSYS_INSTALL_DIR}/include" )
		FIND_LIBRARY( RSYS_LIBRARIES NAMES rsys PATHS "${RSYS_INSTALL_DIR}/lib" )
	ELSE ( RSYS_INSTALL_DIR )
		FIND_PATH( RSYS_INCLUDE_DIR rsys.h )
		FIND_LIBRARY( RSYS_LIBRARIES NAMES rsys )
	ENDIF ( RSYS_INSTALL_DIR )

	IF ( RSYS_INCLUDE_DIR AND RSYS_LIBRARIES )
		SET( RSYS_FOUND 1 )
		IF ( NOT RSYS_FIND_QUIETLY )
			MESSAGE ( STATUS "Found RSYS: ${RSYS_LIBRARIES}" )
		ENDIF ( NOT RSYS_FIND_QUIETLY )
	ELSE ( RSYS_INCLUDE_DIR AND RSYS_LIBRARIES )
		IF ( RSYS_FIND_REQUIRED )
			MESSAGE( FATAL_ERROR "Could NOT find RSYS" )
		ELSE ( RSYS_FIND_REQUIRED )
			IF ( NOT RSYS_FIND_QUIETLY )
				MESSAGE( STATUS "Could NOT find RSYS" )	
			ENDIF ( NOT RSYS_FIND_QUIETLY )
		ENDIF ( RSYS_FIND_REQUIRED )
	ENDIF ( RSYS_INCLUDE_DIR AND RSYS_LIBRARIES )
ENDIF( NOT RSYS_FOUND )

# Hide advanced variables from CMake GUIs
MARK_AS_ADVANCED( RSYS_INCLUDE_DIR RSYS_LIBRARIES )

