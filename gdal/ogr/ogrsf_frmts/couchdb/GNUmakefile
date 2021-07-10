

include ../../../GDALmake.opt

OBJ	=	ogrcouchdbdriver.o ogrcouchdbdatasource.o ogrcouchdblayer.o ogrcouchdbtablelayer.o ogrcouchdbrowslayer.o

CPPFLAGS	:=	$(JSON_INCLUDE) -iquote .. -iquote ../.. -iquote ../geojson  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_couchdb.h ../../ogr_swq.h ../geojson/ogrgeojsonreader.h ../geojson/ogrgeojsonwriter.h
