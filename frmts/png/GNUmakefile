# $Id$
#
# Makefile to build libpng using GNU Make and GCC.
#

include ../../GDALmake.opt

ifeq ($(PNG_SETTING),internal)
XTRA_OPT =	-DPNG_IMPEXP= -Ilibpng
OBJ = \
	png.o \
	pngerror.o \
	pngget.o \
	pngmem.o \
	pngpread.o \
	pngread.o \
	pngrio.o \
	pngrtran.o \
	pngrutil.o \
	pngset.o \
	pngtrans.o \
	pngwio.o \
	pngwrite.o \
	pngwtran.o \
	pngwutil.o \
	pngdataset.o

else
OBJ	=	pngdataset.o
endif

ifeq ($(LIBZ_SETTING),internal)
XTRA_OPT	:=	$(XTRA_OPT) -I../zlib
endif

CPPFLAGS	:=	$(XTRA_OPT)  $(CPPFLAGS)
# Enable this to compile with -Wextra -Werror and get around the jump complaint.
# CPPFLAGS += -Wno-clobbered

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o libpng/*.o $(O_OBJ)

# Disable Neon optimizations for internal libpng as we don't include the needed source file
../o/%.$(OBJ_EXT):	libpng/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS_NOFTRAPV) -DPNG_ARM_NEON_IMPLEMENTATION=0 -DPNG_ARM_NEON_OPT=0 $< -o $@

all:	$(OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
