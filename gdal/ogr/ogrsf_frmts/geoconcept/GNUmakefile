include ../../../GDALmake.opt

OBJ	=	geoconcept.o geoconcept_syscoord.o \
		ogrgeoconceptdriver.o ogrgeoconceptdatasource.o ogrgeoconceptlayer.o

CPPFLAGS	:=	-DUSE_CPL -I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
