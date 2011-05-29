TD_ROOT = /home/warmerda/pkg/td_4.2
TDLIB_ROOT = $(TD_ROOT)/lib/lnxX64_4.2pic
TDXXFLAGS = \
	-I$(TD_ROOT)/TD/Include \
	-I$(TD_ROOT)/TD/Extensions/ExServices \
	-I$(TD_ROOT)/TD/Examples/Common

TD_LIBS =	$(TDLIB_ROOT)/libModelerGeometry.a \
	$(TDLIB_ROOT)/libTD_BrepRenderer.a \
	$(TDLIB_ROOT)/libTD_Br.a \
        $(TDLIB_ROOT)/libTD_AcisBuilder.a \
        $(TDLIB_ROOT)/libTD_DynBlocks.a \
        $(TDLIB_ROOT)/libTD_Db.a \
        $(TDLIB_ROOT)/libTD_DbRoot.a \
        $(TDLIB_ROOT)/libTD_Gs.a \
        $(TDLIB_ROOT)/libTD_SpatialIndex.a \
        $(TDLIB_ROOT)/libTD_Ave.a \
        $(TDLIB_ROOT)/libTD_Root.a \
        $(TDLIB_ROOT)/libTD_Gi.a \
        $(TDLIB_ROOT)/libTD_Ge.a \
        $(TDLIB_ROOT)/libTD_FT.a \
        $(TDLIB_ROOT)/libTD_Alloc.a \
        $(TDLIB_ROOT)/libRxRasterServices.a \
        $(TDLIB_ROOT)/libJpeg.a \
        $(TDLIB_ROOT)/libRecomputeDimBlock.a \
        $(TDLIB_ROOT)/libExFieldEvaluator.a \
        $(TDLIB_ROOT)/libOdBagFiler.a \
        $(TDLIB_ROOT)/libRasterProcessor.a \
	$(TDLIB_ROOT)/libTD_ExamplesCommon.a


include ../../../GDALmake.opt

OBJ	=	ogrdwgdriver.o ogrdwgdatasource.o ogrdwglayer.o \
		ogrdwg_dimension.o ogrdwg_hatch.o ogrdwgblockslayer.o \
		ogrdwg_blockmap.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -I../dxf $(TDXXFLAGS) $(CPPFLAGS)

PLUGIN_SO	=	ogr_DWG.so

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ) $(PLUGIN_SO)

$(O_OBJ) $(OBJ):	ogr_dwg.h 

plugin: $(PLUGIN_SO)

$(PLUGIN_SO):	$(OBJ) 
	gcc -shared $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS_INS) $(EXTRA_LIBS) $(TD_LIBS) \
		-o $(PLUGIN_SO)
