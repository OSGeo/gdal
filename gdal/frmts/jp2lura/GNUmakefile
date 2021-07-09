include ../../GDALmake.opt

OBJ	=	jp2luracallbacks.o jp2lurarasterband.o jp2luradataset.o

CPPFLAGS	:=	 -DFRMT_jp2lura -iquote .. $(CPPFLAGS) $(JP2LURA_INCLUDE) $(EXTRA_CFLAGS)

PLUGIN_SO =	gdal_JP2Lura.so

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ) *.so

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ):	jp2luracallbacks.h jp2lurarasterband.h jp2luradataset.h

plugin: $(PLUGIN_SO)

$(PLUGIN_SO):	$(OBJ)
	$(LD_SHARED) $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS) $(EXTRA_LIBS) \
		-o $(PLUGIN_SO)
