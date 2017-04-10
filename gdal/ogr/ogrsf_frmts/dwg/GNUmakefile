
TDXXFLAGS = \
	-I$(TEIGHA_DIR)/Core/Include \
	-I$(TEIGHA_DIR)/Core/Extensions/ExServices \
	-I$(TEIGHA_DIR)/Core/Examples/Common

include ../../../GDALmake.opt

OBJ	=	ogrdwgdriver.o ogrdwgdatasource.o ogrdwglayer.o \
		ogrdwg_dimension.o ogrdwg_hatch.o ogrdwgblockslayer.o \
		ogrdwg_blockmap.o

CPPFLAGS	:=	 -I../dxf $(TDXXFLAGS) $(CPPFLAGS)

PLUGIN_SO	=	ogr_DWG.so

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ) $(OBJ):	ogr_dwg.h
