

include ../../../GDALmake.opt

CORE_OBJ =	ogr2kmlgeometry.o

OGR_OBJ =	ogrkmldriver.o ogrkmldatasource.o ogrkmllayer.o kml.o kmlvector.o kmlnode.o

OBJ =	$(CORE_OBJ) $(OGR_OBJ)

CPPFLAGS :=	-I.. -I../.. $(EXPAT_INCLUDE) $(GDAL_INCLUDE)  $(CPPFLAGS)
#CFLAGS	:=	$(filter-out -Wall,$(CFLAGS))

default:	$(O_OBJ)

clean:
	rm -f *.o kmlview $(O_OBJ)



