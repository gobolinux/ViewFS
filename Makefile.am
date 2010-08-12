
bin_PROGRAMS = viewfs
EXTRA_DIST = GlobalView

AM_CFLAGS = -std=c99 -D_FILE_OFFSET_BITS=64

viewfs_SOURCES = src/cmdline.c src/cmdline.h src/depfile.c \
src/depfile.h src/dirlist.c src/dirlist.h src/entrydata.c \
src/entrydata.h src/list.c src/list.h src/stringset.c \
src/stringset.h src/vect.c src/vect.h src/version.c \
src/version.h src/viewfs.c

%.h: %.c
	GenerateHeader $<
