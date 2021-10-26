
include ../../GDALmake.opt

OBJ	=	openjpegdataset.o

CPPFLAGS        :=       -iquote .. $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f $(OBJ) $(O_OBJ) *.so *.lo

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

PLUGIN_SO = gdal_JP2OpenJPEG.so

plugin: $(PLUGIN_SO)

$(PLUGIN_SO): $(OBJ)
	$(LD_SHARED) $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS_INS) $(LIBS) \
	-o $(PLUGIN_SO)
