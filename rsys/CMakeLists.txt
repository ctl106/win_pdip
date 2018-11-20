ADD_LIBRARY(rsys SHARED rsystem.c
                        rsys_msg.c)

ADD_EXECUTABLE(rsystemd rsystemd.c
                        rsys_msg.c)
TARGET_LINK_LIBRARIES(rsystemd pdip pthread)

# Installation of the header file
INSTALL(FILES rsys.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)

# Installation of the library
INSTALL(TARGETS rsys
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)

# Installation of the daemon
INSTALL(TARGETS rsystemd
        DESTINATION ${CMAKE_INSTALL_SBINDIR}
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)


SET(rsys_man_src_3 rsystem.3 rsys_lib_initialize.3 rsys_en.3)
SET(rsys_man_src_8 rsystemd_en.8)


STRING(REGEX REPLACE ".3" ".3.gz" rsys_man_gz_3 "${rsys_man_src_3}")
STRING(REGEX REPLACE ".8" ".8.gz" rsys_man_gz_8 "${rsys_man_src_8}")

# Compression of the manuals
FOREACH(man ${rsys_man_src_3} ${rsys_man_src_8})
  ADD_CUSTOM_COMMAND(OUTPUT ${man}.gz
                     COMMAND ${GZIP_TOOL} -c ${man} > ${man}.gz
                     DEPENDS ${man}
                     COMMENT "Building ${man}.gz"
                    )
ENDFOREACH(man)


# Build the manuals
ADD_CUSTOM_TARGET(man_rsys ALL DEPENDS ${rsys_man_gz_3} ${rsys_man_gz_8})


# Installation of the manuals

INSTALL(FILES rsystem.3.gz
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man3
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)

INSTALL(FILES rsys_en.3.gz
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man3
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
        RENAME rsys.3.gz)

INSTALL(FILES rsystemd_en.8.gz
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man8
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
        RENAME rsystemd.8.gz)

