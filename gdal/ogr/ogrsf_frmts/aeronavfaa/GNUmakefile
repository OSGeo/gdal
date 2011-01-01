

include ../../../GDALmake.opt

OBJ	=	ograeronavfaadriver.o ograeronavfaadatasource.o ograeronavfaalayer.o

CPPFLAGS	:=	-I.. -I../.. -I../xplane $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_aeronavfaa.h