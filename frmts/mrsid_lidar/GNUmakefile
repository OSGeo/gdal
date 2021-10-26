include ../../GDALmake.opt

OBJ		=	gdal_MG4Lidar.o

CPPFLAGS	:=	$(XTRA_OPT)  $(MRSID_LIDAR_INCLUDE) $(CPPFLAGS)

PLUGIN_SO	=	gdal_MG4Lidar.so

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ) *.so

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

plugin: $(PLUGIN_SO)

$(PLUGIN_SO):	$(OBJ)
	gcc -shared $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS_INS) $(EXTRA_LIBS) \
		-o $(PLUGIN_SO)
