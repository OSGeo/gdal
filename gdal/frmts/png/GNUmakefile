# $Id$
#
# Makefile to build libpng using GNU Make and GCC.
#

include ../../GDALmake.opt

ifeq ($(PNG_SETTING),internal)
XTRA_OPT =	-DPNG_NO_GLOBAL_ARRAYS -Ilibpng
OBJ = \
	png.o \
	pngerror.o \
	pnggccrd.o \
	pngget.o \
	pngmem.o \
	pngpread.o \
	pngread.o \
	pngrio.o \
	pngrtran.o \
	pngrutil.o \
	pngset.o \
	pngtrans.o \
	pngvcrd.o \
	pngwio.o \
	pngwrite.o \
	pngwtran.o \
	pngwutil.o \
	pngdataset.o

else
OBJ	=	pngdataset.o
endif

XTRA_OPT	:=	$(XTRA_OPT) -I../zlib

CPPFLAGS	:=	$(XTRA_OPT) $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o libpng/*.o $(O_OBJ)

../o/%.o:	libpng/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

all:	$(OBJ)

install-obj:	$(O_OBJ)
