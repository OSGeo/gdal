# $Id$
#
# Makefile building jcon-c library (http://oss.metaparadigm.com/json-c/)
# 
include ../../../../GDALmake.opt

OBJ = \
	arraylist.o \
	debug.o \
	json_object.o \
	json_tokener.o \
	json_util.o \
	linkhash.o \
	printbuf.o \
	json_object_iterator.o \
	json_c_version.o

O_OBJ = $(foreach file,$(OBJ),../../o/$(file))

CPPFLAGS := $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

# -D_XOPEN_SOURCE=500 to enable strdup() definition in C11 mode
../../o/%.$(OBJ_EXT):	%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -D_XOPEN_SOURCE=500 -c -o $@ $<

clean:
	rm -f *.o $(O_OBJ)
	rm -f *~
