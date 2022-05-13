
TDXXFLAGS = \
	-I$(TEIGHA_DIR)/Core/Include \
	-I$(TEIGHA_DIR)/Core/Extensions/ExServices \
	-I$(TEIGHA_DIR)/Core/Examples/Common \
	-I$(TEIGHA_DIR)/ThirdParty/activation \
	-I$(TEIGHA_DIR)/KernelBase/Include \
	-I$(TEIGHA_DIR)/Kernel/Include \
	-I$(TEIGHA_DIR)/Kernel/Extensions/ExServices \
	-I$(TEIGHA_DIR)/Drawing/Include \
	-I$(TEIGHA_DIR)/Drawing/Extensions/ExServices \
	-I$(TEIGHA_DIR)/Dgn/include \
	-I$(TEIGHA_DIR)/Dgn/Extensions/ExServices

include ../../../GDALmake.opt

OBJ	=	ogrdwgdriver.o ogrdwgdatasource.o ogrdwglayer.o \
		ogrdwg_dimension.o ogrdwg_hatch.o ogrdwgblockslayer.o \
		ogrdwg_blockmap.o \
		ogrdgnv8driver.o ogrdgnv8datasource.o ogrdgnv8layer.o \
		ogrteigha.o

CPPFLAGS	:=	 -iquote ../dxf $(TDXXFLAGS) $(CPPFLAGS) $(TEIGHA_CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ) createdgnv8testfile$(EXE)

$(O_OBJ) $(OBJ):	ogr_dwg.h ogr_dgnv8.h

createdgnv8testfile$(EXE):	createdgnv8testfile.$(OBJ_EXT)
	$(LD) $(LDFLAGS) createdgnv8testfile.$(OBJ_EXT) $(CONFIG_LIBS) -o createdgnv8testfile$(EXE)
