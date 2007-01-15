

include ../../../GDALmake.opt

CORE_OBJ =	kmlfeatureclass.o kmlfeature.o ogr2kmlgeometry.o 		

OGR_OBJ =	ogrkmldriver.o ogrkmldatasource.o ogrkmllayer.o

OBJ =	$(CORE_OBJ) $(OGR_OBJ)

CPPFLAGS :=	-I.. -I../.. $(GDAL_INCLUDE)  $(CPPFLAGS)
#CFLAGS	:=	$(filter-out -Wall,$(CFLAGS))

default:	$(O_OBJ)

clean:
	rm -f *.o kmlview $(O_OBJ)


