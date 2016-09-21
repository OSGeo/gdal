include ../../GDALmake.opt

ifeq ($(LIBZ_SETTING),internal)
	XTRA_OPT =	-I../zlib
else
	XTRA_OPT =
endif

OBJ		=	georaster_dataset.o \
			georaster_rasterband.o \
			oci_wrapper.o \
			georaster_wrapper.o

CPPFLAGS	:=	$(XTRA_OPT)  $(OCI_INCLUDE) $(CPPFLAGS)

ifneq ($(JPEG_SETTING),no)
CPPFLAGS	:=	$(CPPFLAGS) -DJPEG_SUPPORTED
endif

ifeq ($(JPEG_SETTING),internal)
CPPFLAGS	:=	$(CPPFLAGS) -I../jpeg/libjpeg
endif

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ) *.so *.lo

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

PLUGIN_SO	= gdal_GEOR.$(SO_EXT)

plugin: $(PLUGIN_SO)

$(PLUGIN_SO):	$(OBJ)
	$(LD_SHARED) $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS_INS) $(LIBS) \
		-o $(PLUGIN_SO)
