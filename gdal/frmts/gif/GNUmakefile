
include ../../GDALmake.opt

ifeq ($(GIF_SETTING),internal)
XTRA_OPT =	-Igiflib
OBJ	=	egif_lib.o dgif_lib.o gifalloc.o gif_err.o gif_hash.o \
		\
		gifdataset.o biggifdataset.o gifabstractdataset.o
else
OBJ	=	gifdataset.o biggifdataset.o gifabstractdataset.o
endif

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) $(XTRA_OPT)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):       gifabstractdataset.h

clean:
	rm -f *.o $(O_OBJ)

../o/%.$(OBJ_EXT):	giflib/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@


all:	$(OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
