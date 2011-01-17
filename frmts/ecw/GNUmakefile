
include ../../GDALmake.opt

OBJ	=	ecwdataset.o ecwcreatecopy.o jp2userbox.o ecwasyncreader.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -DFRMT_ecw $(CPPFLAGS) $(EXTRA_CFLAGS)
PLUGIN_SO =	gdal_ECW_JP2ECW.so

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ) *.so

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ):	gdal_ecw.h

plugin: $(PLUGIN_SO)

$(PLUGIN_SO):	$(OBJ)
	$(LD_SHARED) $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS) $(EXTRA_LIBS) \
		-o $(PLUGIN_SO)
