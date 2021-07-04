
include ../../GDALmake.opt

OBJ	=	png_crc.o VRC.o VRCutils.o VRCthirtysix.o VRHV.o
LO_O_OBJ       =       $(addsuffix .lo,$(basename $(O_OBJ)))

# CPPFLAGS        :=       -I.. $(CPPFLAGS) -DFRMT_vrhv
CPPFLAGS        :=       -I.. $(CPPFLAGS) -Wno-unknown-pragmas
#LNK_FLAGS		+=		-shared
LD_SHARED		= $(LD) -shared

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f $(OBJ) $(O_OBJ) *.so $(LO_O_OBJ) .libs/*.o $(PLUGIN_SO)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

PLUGIN_SO = gdal_VRC.so

plugin: $(PLUGIN_SO)

# Do we need to add "-Lsomething -lpng" ?
$(PLUGIN_SO): $(OBJ)
	$(LD_SHARED) $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS_INS) $(LIBS) -o $(PLUGIN_SO)
