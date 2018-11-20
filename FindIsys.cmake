# - Try to find the ISYS libraries
#  Once done this will define
#
#  ISYS_FOUND - system has ISYS
#  ISYS_INCLUDE_DIRS - the ISYS include directory
#  ISYS_LIBRARIES - ISYS library
#
#  This configuration file for finding libisys is derived from the one done
#  done for CHECK library:
#  opensync.org/browser/branches/3rd-party-cmake-modules/modules/FindCheck.cmake
#


INCLUDE( FindPkgConfig )

# Take care about isys.pc settings
#
# The following sets:
#
#    ISYS_FOUND          ... set to 1 if module(s) exist
#    ISYS_LIBRARIES      ... only the libraries (w/o the '-l')
#    ISYS_LIBRARY_DIRS   ... the paths of the libraries (w/o the '-L')
#    ISYS_LDFLAGS        ... all required linker flags
#    ISYS_LDFLAGS_OTHER  ... all other linker flags
#    ISYS_INCLUDE_DIRS   ... the '-I' preprocessor flags (w/o the '-I')
#    ISYS_CFLAGS         ... all required cflags
#    ISYS_CFLAGS_OTHER   ... the other compiler flags
#
PKG_SEARCH_MODULE( ISYS isys )

# Look for ISYS include dir and libraries
IF( NOT ISYS_FOUND )
	IF ( ISYS_INSTALL_DIR )
		MESSAGE ( STATUS "Using override ISYS_INSTALL_DIR to find isys" )
		SET ( ISYS_INCLUDE_DIRS  "${ISYS_INSTALL_DIR}/include" )
		FIND_LIBRARY( ISYS_LIBRARIES NAMES isys PATHS "${ISYS_INSTALL_DIR}/lib" )
	ELSE ( ISYS_INSTALL_DIR )
		FIND_PATH( ISYS_INCLUDE_DIR isys.h )
		FIND_LIBRARY( ISYS_LIBRARIES NAMES isys )
	ENDIF ( ISYS_INSTALL_DIR )

	IF ( ISYS_INCLUDE_DIR AND ISYS_LIBRARIES )
		SET( ISYS_FOUND 1 )
		IF ( NOT ISYS_FIND_QUIETLY )
			MESSAGE ( STATUS "Found ISYS: ${ISYS_LIBRARIES}" )
		ENDIF ( NOT ISYS_FIND_QUIETLY )
	ELSE ( ISYS_INCLUDE_DIR AND ISYS_LIBRARIES )
		IF ( ISYS_FIND_REQUIRED )
			MESSAGE( FATAL_ERROR "Could NOT find ISYS" )
		ELSE ( ISYS_FIND_REQUIRED )
			IF ( NOT ISYS_FIND_QUIETLY )
				MESSAGE( STATUS "Could NOT find ISYS" )	
			ENDIF ( NOT ISYS_FIND_QUIETLY )
		ENDIF ( ISYS_FIND_REQUIRED )
	ENDIF ( ISYS_INCLUDE_DIR AND ISYS_LIBRARIES )
ENDIF( NOT ISYS_FOUND )

# Hide advanced variables from CMake GUIs
MARK_AS_ADVANCED( ISYS_INCLUDE_DIR ISYS_LIBRARIES )

