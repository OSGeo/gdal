

include ../../../GDALmake.opt

OBJ	=	ogrcouchdbdriver.o ogrcouchdbdatasource.o ogrcouchdblayer.o ogrcouchdbtablelayer.o ogrcouchdbrowslayer.o

CPPFLAGS	:=	-I.. -I../.. -I../geojson -I../geojson/jsonc $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_couchdb.h ../../swq.h ../geojson/ogrgeojsonreader.h ../geojson/ogrgeojsonwriter.h