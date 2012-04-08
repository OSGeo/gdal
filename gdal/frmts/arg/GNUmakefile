

include ../../GDALmake.opt

OBJ	=	argdataset.o

CPPFLAGS :=	-I../../ogr/ogrsf_frmts/geojson -I../raw $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
