
include ../../GDALmake.opt

OBJ	=	gdalogcapidataset.o

CPPFLAGS	:=	 -I../../gcore -I../../ogr -I../../ogr/ogrsf_frmts/gml $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(OBJ) $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
