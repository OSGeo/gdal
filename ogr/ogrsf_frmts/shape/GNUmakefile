

include ../../../GDALmake.opt

OBJ	=	shape2ogr.o shpopen.o dbfopen.o shptree.o \
		ogrshapedriver.o ogrshapedatasource.o ogrshapelayer.o

CPPFLAGS	:=	-DUSE_CPL -I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)
