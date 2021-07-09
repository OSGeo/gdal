

include ../../../GDALmake.opt

OBJ	=	ogrplscenesdriver.o \
		ogrplscenesdatav1layer.o ogrplscenesdatav1dataset.o

CPPFLAGS	:=	$(JSON_INCLUDE) -iquote .. -iquote ../.. -iquote ../geojson  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_plscenes.h ../../ogr_swq.h ../geojson/ogrgeojsonreader.h
