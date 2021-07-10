

include ../../../GDALmake.opt

OBJ	=	ogrelasticdriver.o ogrelasticdatasource.o ogrelasticlayer.o

CPPFLAGS	:=	$(JSON_INCLUDE) -iquote .. -iquote ../.. -iquote ../geojson $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_elastic.h ../geojson/ogrgeojsonreader.h ../geojson/ogrgeojsonwriter.h