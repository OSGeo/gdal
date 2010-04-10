include ../../GDALmake.opt

# This is a temporary solution just to make it build the plugin
#
# Once the configure is changes to support it, the followinf should be removed:

MRSID_LIDAR_DIR	=	$(HOME)/Dev/Lidar_DSDK-1.1.0.2814

EXTRA_LIBS	=	-L $(MRSID_LIDAR_DIR)/lib/Release/ -l lti_lidar_dsdk

MRSID_LIDAR_INCLUDE	= -I. -I$(MRSID_LIDAR_DIR)/include/

OBJ		=	gdal_MG4Lidar.o

CPPFLAGS	:=	$(XTRA_OPT) $(GDAL_INCLUDE) $(MRSID_LIDAR_INCLUDE) $(CPPFLAGS)

PLUGIN_SO	=	gdal_MG4Lidar.so

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ) *.so

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

plugin: $(PLUGIN_SO)

$(PLUGIN_SO):	$(OBJ)
	gcc -shared $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS_INS) $(EXTRA_LIBS) \
		-o $(PLUGIN_SO)
