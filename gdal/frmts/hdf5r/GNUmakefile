
include ../../GDALmake.opt

OBJ	=	Hdf5rFrameData.o \
        Hdf5rLosGrid.o \
        Hdf5rIObase.o \
        Hdf5rReader.o \
        Hdf5rWriter.o \
        Hdf5rAttributeBase.o \
        CompoundBase.o \
        HDF5RDataSet.o \
        HDF5RSubDataSet.o \
        HDF5RRasterBand.o \
        Earth.o

HDFEOS_OPTS	=
SUBLIBS 	=

CPPFLAGS	:=	 $(HDF5_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT)) $(SUBLIBS)

clean:
	rm -f *.o $(O_OBJ)

$(OBJ) $(O_OBJ):	Hdf5IObase.h \
										Hdf5rFrameData.h \
                    Hdf5rLosGrid.h \
                    Hdf5rReader.h \
                    Hdf5rWriter.h \
                    Hdf5rAttributeBase.h \
                    Hdf5rFileAttributes.h \
                    Hdf5rGeoLocAttributes.h \
                    CompoundBase.h \
                    CreationOptions.h \
                    OpenOptions.h \
                    HDF5RDataSet.h \
                    HDF5RSubDataSet.h \
                    HDF5RRasterBand.h \
                    Earth.h \
                    m3d.h

install-obj:	$(SUBLIBS) $(O_OBJ:.o=.$(OBJ_EXT))

PLUGIN_SO       = gdal_HDF5R.$(SO_EXT)

plugin: $(PLUGIN_SO)

$(PLUGIN_SO):   $(OBJ)
	$(LD_SHARED) $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS_INS) $(LIBS) \
		-o $(PLUGIN_SO)
