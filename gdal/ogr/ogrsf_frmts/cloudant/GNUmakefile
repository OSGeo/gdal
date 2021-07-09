

include ../../../GDALmake.opt

OBJ	=	ogrcloudantdriver.o ogrcloudantdatasource.o ogrcloudanttablelayer.o

CPPFLAGS	:=	$(JSON_INCLUDE) -iquote .. -iquote ../.. -iquote ../geojson -iquote ../couchdb $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_cloudant.h ../../ogr_swq.h ../geojson/ogrgeojsonreader.h ../geojson/ogrgeojsonwriter.h
