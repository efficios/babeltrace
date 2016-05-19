debug-info-data
==============

This directory contains pre-generated ELF and DWARF files used to test
the debug info analysis feature, including lookup of DWARF debugging
information via build ID and debug link methods, as well as the source
files used to generate them.

The generated files are:

* `libhello_so` (ELF and DWARF)
* `libhello_elf_so` (ELF only)
* `libhello_build_id_so` (ELF with separate DWARF via build ID)
* `libhello_debug_link_so` (ELF with separate DWARF via debug link)
* `libhello_debug_link_so.debug` (DWARF for debug link)
* `.build-id/cd/d98cdd87f7fe64c13b6daad553987eafd40cbb.debug` (DWARF for build ID)

We use a suffix of "_so" instead of ".so" since some distributions
build systems will consider ".so" files as artifacts from a previous
build that were "left-over" and will remove them prior to the build.

All files are generated from the four (4) following source files:

* libhello.c
* libhello.h
* tp.c
* tp.h

The generated executables were built using a GNU toolchain on an
x86_64 machine.

To regenerate them, you can use follow these steps:

## ELF and DWARF

    $ gcc -g -fPIC -c -I. tp.c libhello.c
    $ gcc -shared -g -llttng-ust -ldl -Wl,-soname,libhello.so -o libhello_so tp.o libhello.o

## ELF only

    $ gcc -fPIC -c -I. tp.c libhello.c
    $ gcc -shared -llttng-ust -ldl -Wl,-soname,libhello_elf.so -o libhello_elf_so tp.o libhello.o

## ELF and DWARF with Build ID

    $ gcc -g -fPIC -c -I. tp.c libhello.c
    $ gcc -shared -g -llttng-ust -ldl -Wl,-soname,libhello_build_id.so -Wl,--build-id=sha1 -o libhello_build_id_so tp.o libhello.o
    $ mkdir -p .build-id/cd/
    $ objcopy --only-keep-debug libhello_build_id_so .build-id/cd/d98cdd87f7fe64c13b6daad553987eafd40cbb.debug
    $ strip -g libhello_build_id_so

The build ID might not be the same once the executable is regenerated
on your system, so adjust the values in the directory and file names
accordingly. Refer to the GDB documentation for more information:
https://sourceware.org/gdb/onlinedocs/gdb/Separate-Debug-Files.html

##  ELF and DWARF with Debug Link

    $ gcc -g -fPIC -c -I. tp.c libhello.c
    $ gcc -shared -g -llttng-ust -ldl -Wl,-soname,libhello_debug_link.so -o libhello_debug_link_so tp.o libhello.o

    $ objcopy --only-keep-debug libhello_debug_link_so libhello_debug_link_so.debug
    $ strip -g libhello_debug_link_so
    $ objcopy --add-gnu-debuglink=libhello_debug_link_so.debug libhello_debug_link_so
