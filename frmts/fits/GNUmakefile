
include ../../GDALmake.opt

OBJ	=	fitsdataset.o

FITS_OPTS	=	

CPPFLAGS	:=	$(GDAL_INCLUDE) $(FITS_OPTS) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
