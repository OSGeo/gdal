
include ../../GDALmake.opt

OBJ		=	mrsiddataset.o mrsidstream.o

PLUGIN_DLL	=	gdal_MrSID.so

ifeq ($(GEOTIFF_SETTING),internal)
GEOTIFF_INCLUDE	=	-I../../frmts/gtiff/libgeotiff
ifeq ($(RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS),yes)
CPPFLAGS	:=	-DRENAME_INTERNAL_LIBGEOTIFF_SYMBOLS $(CPPFLAGS)
endif
endif

CPPFLAGS	:=	$(GDAL_INCLUDE) $(MRSID_FLAGS) $(MRSID_INCLUDE) $(GEOTIFF_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

plugin: $(PLUGIN_DLL)

$(PLUGIN_DLL):  $(OBJ)
	$(LD_SHARED) $(OBJ) $(LDFLAGS) $(MRSID_LIBS) -o $(PLUGIN_DLL)

#$(PLUGIN_DLL):	$(OBJ:.o=.lo)
#	$(LD) $(LDFLAGS) $(MRSID_LIBS) ../../$(LIBGDAL) -o $@ $(OBJ:.o=.lo) \
#	    -rpath $(INST_LIB) \
#	    -no-undefined -export-dynamic

