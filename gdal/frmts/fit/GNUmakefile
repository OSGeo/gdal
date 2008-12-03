
include ../../GDALmake.opt

OBJ	=	fitdataset.o fit.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(XTRA_OPT) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o

all:	$(OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
