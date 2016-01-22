

include ../../../GDALmake.opt

OBJ	=	ogrsuadriver.o ogrsuadatasource.o ogrsualayer.o

CPPFLAGS	:=	-I.. -I../.. -I../xplane $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_sua.h