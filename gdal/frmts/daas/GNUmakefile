
include ../../GDALmake.opt

OBJ =	daasdataset.o

CPPFLAGS	:=	  -I../../ogr/ogrsf_frmts/geojson $(JSON_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(OBJ) $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
