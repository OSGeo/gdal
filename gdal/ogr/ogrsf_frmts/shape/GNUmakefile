

include ../../../GDALmake.opt

OBJ	=	shape2ogr.o shpopen.o dbfopen.o shptree.o shp_vsi.o \
		ogrshapedriver.o ogrshapedatasource.o ogrshapelayer.o

CPPFLAGS :=	-DSAOffset=vsi_l_offset -DUSE_CPL \
		-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS) 

default:	$(O_OBJ)

$(OBJ) $(O_OBJ):	ogrshape.h

clean:
	rm -f *.o $(O_OBJ)
