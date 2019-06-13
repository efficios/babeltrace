debug-info-data
==============

This directory contains pre-generated ELF and DWARF files used to test
the debug info analysis feature, including lookup of DWARF debugging
information via build ID and debug link methods, as well as the source
files used to generate them.

The generated files are:

* `ARCH/dwarf_full/libhello_so` (ELF and DWARF)
* `ARCH/elf_only/libhello_so` (ELF only)
* `ARCH/build_id/libhello_so` (ELF with separate DWARF via build ID)
* `ARCH/build_id/.build-id/cd/d98cdd87f7fe64c13b6daad553987eafd40cbb.debug` (DWARF for build ID)
* `ARCH/debug_link/libhello_so` (ELF with separate DWARF via debug link)
* `ARCH/debug_link/libhello_so.debug` (DWARF for debug link)

We use a suffix of "_so" instead of ".so" since some distributions
build systems will consider ".so" files as artifacts from a previous
build that were "left-over" and will remove them prior to the build.

All files are generated from the four (4) following source files:

* libhello.c
* libhello.h
* tp.c
* tp.h

The generated executables were built using a native GNU toolchain on either
Ubuntu 16.04 or 18.04 depending on the architecture.

To regenerate them, you can use the included Makefile or follow these steps:

## Generate the object files

    $ gcc -gdwarf -fdebug-prefix-map=$(pwd)=. -fPIC -c -I. tp.c libhello.c

## Combined ELF and DWARF

    $ build_id_prefix=cd
    $ build_id_suffix=d98cdd87f7fe64c13b6daad553987eafd40cbb
    $ build_id="$build_id_prefix$build_id_suffix"
    $ mkdir dwarf_full
    $ gcc -shared -g -llttng-ust -ldl -Wl,-soname,libhello.so -Wl,--build-id=0x$build_id -o dwarf_full/libhello_so tp.o libhello.o

## ELF only

    $ mkdir elf_only
    $ objcopy -g dwarf_full/libhello_so elf_only/libhello_so
    $ objcopy --remove-section=.note.gnu.build-id elf_only/libhello_so

## ELF and DWARF with Build ID

    $ mkdir -p build_id/.build-id/$build_id_prefix
    $ objcopy --only-keep-debug dwarf_full/libhello_so build_id/.build-id/$build_id_prefix/$build_id_suffix.debug
    $ objcopy -g dwarf_full/libhello_so build_id/libhello_so

##  ELF and DWARF with Debug Link

    $ mkdir debug_link
    $ objcopy --remove-section=.note.gnu.build-id dwarf_full/libhello_so debug_link/libhello_so
    $ objcopy --only-keep-debug debug_link/libhello_so debug_link/libhello_so.debug
    $ objcopy -g debug_link/libhello_so
    $ cd debug_link && objcopy --add-gnu-debuglink=libhello_so.debug libhello_so && cd ..


Test program
------------
An executable linked to this library can be compiled from the `main.c` source file.
To compile it, you can do:

    $ ln -s libhello_so libhello.so
    $ gcc -I. -o test main.c -L. -lhello_build_id -llttng-ust -ldl -Wl,--rpath=.

The trace provided in this directory was generated with lttng-ust running this
program and stripped to contain only the bare minimum. When running babeltrace
with the `--debug-info-target-prefix` option pointing to the source tree of
Babeltrace, the `my_provider:my_first_tracepoint` events should contain this
information:

    debug_info = { bin = "libhello_so+0x166b", func = "baz+0x9c", src = "libhello.c:20" } }
