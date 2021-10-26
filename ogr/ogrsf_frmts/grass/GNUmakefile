

include ../../../GDALmake.opt

OBJ	=	ogrgrassdriver.o ogrgrassdatasource.o ogrgrasslayer.o

CPPFLAGS	:=	-DUSE_CPL -DGRASS_GISBASE=\"$(GRASS_GISBASE)\" -iquote .. -iquote ../.. $(GRASS_INCLUDE) $(PROJ_INCLUDE) $(PG_INC) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
