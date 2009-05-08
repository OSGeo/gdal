include ../../GDALmake.opt

TERRALIB_DIR	=	../../../terralib

SRC		=	$(TERRALIB_DIR)/src/terralib/drivers/MySQL/TeMySQL.cpp

OBJ		=	TerraLibDataset.o \
			TerraLibRasterBand.o \
			$(TERRALIB_DIR)/src/terralib/drivers/MySQL/TeMySQL.o

TERRALIB_LIB	=	$(TERRALIB_DIR)/terralibx/lib/libterralib.so \
			$(TERRALIB_DIR)/terralibx/lib/libjpeg.so

EXTRAFLAGS	=  	-I$(TERRALIB_DIR)/src/terralib/kernel \
			-I$(TERRALIB_DIR)/src/terralib/drivers/MySQL \
			-I$(TERRALIB_DIR)/src/terralib/drivers/MySQL/include/

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) $(EXTRAFLAGS)

EXTRA_LIBS	:=	 $(EXTRA_LIBS) $(TERRALIB_LIB)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

plugin: gdal_TERRALIB.so

gdal_TERRALIB.so:	$(OBJ)
	$(LD_SHARED) $(LNK_FLAGS) $(OBJ) \
	$(GDAL_LIBS) $(EXTRA_LIBS) -o gdal_TERRALIB.so

