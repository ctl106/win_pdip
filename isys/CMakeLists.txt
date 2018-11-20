ADD_LIBRARY(isys SHARED isys.c)


# Installation of the header file
INSTALL(FILES isys.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)

# Installation of the library
INSTALL(TARGETS isys
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)


SET(isys_man_src_3 isystem.3 isys_lib_initialize.3 isys_en.3)


STRING(REGEX REPLACE ".3" ".3.gz" isys_man_gz_3 "${isys_man_src_3}")

# Compression of the manuals
FOREACH(man ${isys_man_src_3})
  ADD_CUSTOM_COMMAND(OUTPUT ${man}.gz
                     COMMAND ${GZIP_TOOL} -c ${man} > ${man}.gz
                     DEPENDS ${man}
                     COMMENT "Building ${man}.gz"
                    )
ENDFOREACH(man)


# Build the manuals
ADD_CUSTOM_TARGET(man_isys ALL DEPENDS ${isys_man_gz_3})


# Installation of the manuals
INSTALL(FILES isystem.3.gz
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man3
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)

INSTALL(FILES isys_en.3.gz
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man3
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
        RENAME isys.3.gz)


