

include ../../../GDALmake.opt

OBJ	=	ogrplscenesdriver.o \
		ogrplscenesdatav1layer.o ogrplscenesdatav1dataset.o

CPPFLAGS	:=	$(JSON_INCLUDE) -I.. -I../.. -I../geojson  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_plscenes.h ../../swq.h ../geojson/ogrgeojsonreader.h