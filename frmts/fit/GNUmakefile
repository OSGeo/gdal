GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	fitdataset.o fit.o

CPPFLAGS	=	$(GDAL_INCLUDE) $(XTRA_OPT)

default:	$(OBJ)

clean:
	rm -f *.o

all:	$(OBJ)

install-obj:	$(O_OBJ)
