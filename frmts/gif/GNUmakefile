GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

ifeq ($(GIF_SETTING),internal)
XTRA_OPT =	-Ilibungif
OBJ	=	egif_lib.o dgif_lib.o gifalloc.o gif_err.o \
		\
		gifdataset.o
else
OBJ	=	gifdataset.o
endif

CPPFLAGS	=	$(GDAL_INCLUDE) $(XTRA_OPT)

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	libungif/%.c
	$(CC) -c $(XTRA_OPT) $(GDAL_INCLUDE) $(CFLAGS) libungif/$*.c -o $*.o


all:	$(OBJ)

install-obj:	$(foreach file,$(OBJ),../o/$(file))
