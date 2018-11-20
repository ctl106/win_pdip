# - Try to find the PDIP libraries
#  Once done this will define
#
#  PDIP_FOUND - system has PDIP
#  PDIP_INCLUDE_DIRS - the PDIP include directory
#  PDIP_LIBRARIES - PDIP library
#
#  This configuration file for finding libpdip is derived from the one done
#  done for CHECK library:
#  opensync.org/browser/branches/3rd-party-cmake-modules/modules/FindCheck.cmake
#


INCLUDE( FindPkgConfig )

# Take care about pdip.pc settings
#
# The following sets:
#
#    PDIP_FOUND          ... set to 1 if module(s) exist
#    PDIP_LIBRARIES      ... only the libraries (w/o the '-l')
#    PDIP_LIBRARY_DIRS   ... the paths of the libraries (w/o the '-L')
#    PDIP_LDFLAGS        ... all required linker flags
#    PDIP_LDFLAGS_OTHER  ... all other linker flags
#    PDIP_INCLUDE_DIRS   ... the '-I' preprocessor flags (w/o the '-I')
#    PDIP_CFLAGS         ... all required cflags
#    PDIP_CFLAGS_OTHER   ... the other compiler flags
#
PKG_SEARCH_MODULE( PDIP pdip )

# Look for PDIP include dir and libraries
IF( NOT PDIP_FOUND )
	IF ( PDIP_INSTALL_DIR )
		MESSAGE ( STATUS "Using override PDIP_INSTALL_DIR to find pdip" )
		SET ( PDIP_INCLUDE_DIRS  "${PDIP_INSTALL_DIR}/include" )
		FIND_LIBRARY( PDIP_LIBRARIES NAMES pdip PATHS "${PDIP_INSTALL_DIR}/lib" )
	ELSE ( PDIP_INSTALL_DIR )
		FIND_PATH( PDIP_INCLUDE_DIR pdip.h )
		FIND_LIBRARY( PDIP_LIBRARIES NAMES pdip )
	ENDIF ( PDIP_INSTALL_DIR )

	IF ( PDIP_INCLUDE_DIR AND PDIP_LIBRARIES )
		SET( PDIP_FOUND 1 )
		IF ( NOT PDIP_FIND_QUIETLY )
			MESSAGE ( STATUS "Found PDIP: ${PDIP_LIBRARIES}" )
		ENDIF ( NOT PDIP_FIND_QUIETLY )
	ELSE ( PDIP_INCLUDE_DIR AND PDIP_LIBRARIES )
		IF ( PDIP_FIND_REQUIRED )
			MESSAGE( FATAL_ERROR "Could NOT find PDIP" )
		ELSE ( PDIP_FIND_REQUIRED )
			IF ( NOT PDIP_FIND_QUIETLY )
				MESSAGE( STATUS "Could NOT find PDIP" )	
			ENDIF ( NOT PDIP_FIND_QUIETLY )
		ENDIF ( PDIP_FIND_REQUIRED )
	ENDIF ( PDIP_INCLUDE_DIR AND PDIP_LIBRARIES )
ENDIF( NOT PDIP_FOUND )

# Hide advanced variables from CMake GUIs
MARK_AS_ADVANCED( PDIP_INCLUDE_DIR PDIP_LIBRARIES )

