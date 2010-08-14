

include ../../../GDALmake.opt

OBJ	=	ogrwfsdriver.o ogrwfsdatasource.o ogrwfslayer.o

CPPFLAGS	:=	-I.. -I../.. -I../gml $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_wfs.h