
include ../../GDALmake.opt

OBJ	=	plmosaicdataset.o

CPPFLAGS	:=	  -iquote ../../ogr/ogrsf_frmts/geojson $(JSON_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(OBJ) $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
