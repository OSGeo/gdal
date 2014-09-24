

include ../../../GDALmake.opt

OBJ	=	ogrmssqlspatialdatasource.o ogrmssqlspatiallayer.o ogrmssqlspatialtablelayer.o ogrmssqlspatialselectlayer.o ogrmssqlspatialdriver.o ogrmssqlgeometryparser.o ogrmssqlgeometryvalidator.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -I.. -I../.. $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
