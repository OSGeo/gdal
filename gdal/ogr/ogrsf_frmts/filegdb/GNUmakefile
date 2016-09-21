

include ../../../GDALmake.opt

OBJ	=	FGdbDatasource.o FGdbDriver.o FGdbLayer.o FGdbUtils.o FGdbResultLayer.o 

CPPFLAGS	:=	-I../generic  $(FGDB_INC) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_fgdb.h

PLUGIN_SO	= ogr_FileGDB.$(SO_EXT)

plugin:		$(PLUGIN_SO)

$(PLUGIN_SO):	$(OBJ)
		$(LD_SHARED) $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS_INS) $(LIBS) \
		-o $(PLUGIN_SO)
