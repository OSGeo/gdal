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

CPPFLAGS	+=	$(GDAL_INCLUDE) $(XTRA_OPT)

default:	$(OBJ)

clean:
	rm -f *.o

../o/%.o:	libungif/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@


all:	$(OBJ)

install-obj:	$(O_OBJ)
