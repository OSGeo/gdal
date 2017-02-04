
include ../../GDALmake.opt

OBJ	=	fitsdataset.o

FITS_OPTS	=

CPPFLAGS	:=	 $(FITS_OPTS) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
