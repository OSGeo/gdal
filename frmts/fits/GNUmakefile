GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	fitsdataset.o

FITS_OPTS	=	

CPPFLAGS	=	$(GDAL_INCLUDE) $(FITS_OPTS)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(foreach file,$(OBJ),../o/$(file))
