
include ../../GDALmake.opt

ifeq ($(GIF_SETTING),internal)
XTRA_OPT =	-Ilibungif
OBJ	=	egif_lib.o dgif_lib.o gifalloc.o gif_err.o \
		\
		gifdataset.o
else
OBJ	=	gifdataset.o
endif

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) $(XTRA_OPT)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)

../o/%.o:	libungif/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@


all:	$(OBJ)

install-obj:	$(O_OBJ)
