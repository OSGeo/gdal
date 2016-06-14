

include ../../../GDALmake.opt

OBJ	=	ogrcouchdbdriver.o ogrcouchdbdatasource.o ogrcouchdblayer.o ogrcouchdbtablelayer.o ogrcouchdbrowslayer.o

CPPFLAGS	:=	$(JSON_INCLUDE) -I.. -I../.. -I../geojson  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_couchdb.h ../../swq.h ../geojson/ogrgeojsonreader.h ../geojson/ogrgeojsonwriter.h