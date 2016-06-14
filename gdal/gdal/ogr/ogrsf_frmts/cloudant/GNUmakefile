

include ../../../GDALmake.opt

OBJ	=	ogrcloudantdriver.o ogrcloudantdatasource.o ogrcloudanttablelayer.o

CPPFLAGS	:=	$(JSON_INCLUDE) -I.. -I../.. -I../geojson -I../couchdb $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_cloudant.h ../../swq.h ../geojson/ogrgeojsonreader.h ../geojson/ogrgeojsonwriter.h