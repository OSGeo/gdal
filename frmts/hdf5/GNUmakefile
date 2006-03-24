
include ../../GDALmake.opt

OBJ	=	hdf5dataset.o hdf5imagedataset.o

HDFEOS_OPTS	=	
SUBLIBS 	=

CPPFLAGS	:=	$(GDAL_INCLUDE) $(HDF5_INCLUDE) $(CPPFLAGS)

default:	$(OBJ) $(SUBLIBS)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(SUBLIBS) $(O_OBJ)
