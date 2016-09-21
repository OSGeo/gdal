

include ../../../GDALmake.opt

OBJ	=	ogrpgdriver.o ogrpgdatasource.o ogrpglayer.o ogrpgtablelayer.o\
		ogrpgresultlayer.o ogrpgutility.o

CPPFLAGS	:=	 $(PG_INC) -I../pgdump $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_pg.h ogrpgutility.h

PLUGIN_SO	= ogr_PG.$(SO_EXT)

plugin:		$(PLUGIN_SO)

$(PLUGIN_SO):	$(OBJ)
		$(LD_SHARED) $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS_INS) $(LIBS) \
		-o $(PLUGIN_SO)
