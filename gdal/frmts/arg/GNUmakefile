

include ../../GDALmake.opt

OBJ	=	argdataset.o

CPPFLAGS :=	-I../../ogr/ogrsf_frmts/geojson $(JSON_INCLUDE) -I../raw  $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
