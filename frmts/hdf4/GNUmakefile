GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	hdf4dataset.o hdf4imagedataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(HDF4_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
