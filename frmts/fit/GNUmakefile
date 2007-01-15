
include ../../GDALmake.opt

OBJ	=	fitdataset.o fit.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(XTRA_OPT) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

all:	$(OBJ)

install-obj:	$(O_OBJ)
