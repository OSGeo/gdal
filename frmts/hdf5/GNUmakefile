GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	hdf5dataset.o hdf5imagedataset.o

HDFEOS_OPTS	=	
SUBLIBS 	=

CPPFLAGS	:=	$(GDAL_INCLUDE) $(HDF5_INCLUDE) $(CPPFLAGS)

default:	$(OBJ) $(SUBLIBS)

clean:
	rm -f *.o

install-obj:	$(SUBLIBS) $(O_OBJ)
