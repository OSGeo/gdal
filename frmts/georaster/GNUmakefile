include ../../GDALmake.opt

ifeq ($(LIBZ_SETTING),internal)
	XTRA_OPT =	-I../zlib
else
	XTRA_OPT =
endif

OBJ		=	georaster_dataset.o \
			georaster_rasterband.o \
			oci_wrapper.o \
			georaster_wrapper.o \
			cpl_vsil_ocilob.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(OCI_INCLUDE) $(XTRA_OPT) $(CPPFLAGS) -fPIC

ifneq ($(JPEG_SETTING),no)
CPPFLAGS	:=	$(CPPFLAGS) -DJPEG_SUPPORTED
endif

ifeq ($(JPEG_SETTING),internal)
CPPFLAGS	:=	$(CPPFLAGS) -I../jpeg/libjpeg
ifeq ($(RENAME_INTERNAL_LIBJPEG_SYMBOLS),yes)
CPPFLAGS 	:=	$(CPPFLAGS) -DRENAME_INTERNAL_LIBJPEG_SYMBOLS
endif
endif

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ) *.so *.lo

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

PLUGIN_SO	= gdal_GEOR.so

plugin: $(PLUGIN_SO)

$(PLUGIN_SO):	$(OBJ)
	$(LD_SHARED) $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS_INS) $(LIBS) \
		-o $(PLUGIN_SO)
