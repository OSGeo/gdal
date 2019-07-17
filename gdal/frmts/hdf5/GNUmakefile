
include ../../GDALmake.opt

OBJ	=	hdf5dataset.o hdf5imagedataset.o \
		bagdataset.o gh5_convenience.o iso19115_srs.o \
		hdf5multidim.o

HDFEOS_OPTS	=
SUBLIBS 	=

CPPFLAGS	:=	 $(HDF5_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT)) $(SUBLIBS)

clean:
	rm -f *.o $(O_OBJ)

$(OBJ) $(O_OBJ):	hdf5dataset.h

install-obj:	$(SUBLIBS) $(O_OBJ:.o=.$(OBJ_EXT))

PLUGIN_SO       = gdal_HDF5.$(SO_EXT)

plugin: $(PLUGIN_SO)

$(PLUGIN_SO):   $(OBJ)
	$(LD_SHARED) $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS_INS) $(LIBS) \
		-o $(PLUGIN_SO)
