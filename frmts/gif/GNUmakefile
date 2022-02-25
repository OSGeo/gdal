
include ../../GDALmake.opt

ifeq ($(GIF_SETTING),internal)
XTRA_OPT =	-Igiflib -DHAVE_STDINT_H -DHAVE_INTTYPES_H
OBJ	=	egif_lib.o dgif_lib.o gifalloc.o gif_err.o gif_hash.o \
		\
		gifdataset.o biggifdataset.o gifabstractdataset.o
else
OBJ	=	gifdataset.o biggifdataset.o gifabstractdataset.o
endif

CPPFLAGS	:=	 $(XTRA_OPT) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):       gifabstractdataset.h

clean:
	rm -f *.o $(O_OBJ)

../o/%.$(OBJ_EXT):	giflib/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $(NO_UNUSED_PARAMETER_FLAG) $(NO_SIGN_COMPARE) $< -o $@


all:	$(OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
