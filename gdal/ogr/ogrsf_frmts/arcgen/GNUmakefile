

include ../../../GDALmake.opt

OBJ	=	ograrcgendriver.o ograrcgendatasource.o ograrcgenlayer.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_arcgen.h