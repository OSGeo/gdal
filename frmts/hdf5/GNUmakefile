
include ../../GDALmake.opt

OBJ	=	hdf5dataset.o hdf5imagedataset.o \
		bagdataset.o gh5_convenience.o iso19115_srs.o

HDFEOS_OPTS	=	
SUBLIBS 	=

CPPFLAGS	:=	$(GDAL_INCLUDE) $(HDF5_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT)) $(SUBLIBS)

clean:
	rm -f *.o $(O_OBJ)

$(OBJ) $(O_OBJ):	hdf5dataset.h

install-obj:	$(SUBLIBS) $(O_OBJ:.o=.$(OBJ_EXT))
