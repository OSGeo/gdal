
include ../../GDALmake.opt

OBJ	=	png_crc.o VRC.o VRCutils.o VRCthirtysix.o VRHV.o

# CPPFLAGS        :=       -I.. $(CPPFLAGS) -DFRMT_vrhv
CPPFLAGS        :=       -I.. $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ) *.so *.lo .libs/*.o

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

PLUGIN_SO = gdal_VRC.so

plugin: $(PLUGIN_SO)

# Do we need to add "-Lsomething -lpng" ?
$(PLUGIN_SO): $(OBJ)
	$(LD) $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS_INS) $(LIBS) \
	-o $(PLUGIN_SO)
