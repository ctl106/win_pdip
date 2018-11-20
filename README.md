# WIN_PDIP
###### A windows port of PDIP - Programmed Dialogue with Interactive Programs

------

### STATUS: under development

------

Original (linux) project location: https://sourceforge.net/projects/pdip/

The readme from the original version of this program has been retained: README.txt

As I could not find a way to properly fork from the project (no "fork" link on
the SourceForge page), I have had to download a static tarball at version 2.4.5.
After I have a working demo done, hopefully I can communicate with the original
author to get a proper upstream source link and incorperate back my changes.

The author may have to incorperate my changes before my version could be
compatible with patches to master, though, as I likely will need to refactor, due
to the appearance of `goto` commands and, of course, translating from a posix API
to the WIN32 API.

## Dependancies

------

1. CMake - build system - https://cmake.org/