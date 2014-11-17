

include ../../../GDALmake.opt

OBJ	=	\
	ogrgmedriver.o ogrgmedatasource.o ogrgmelayer.o ogrgmejson.o

CPPFLAGS	:=	$(JSON_INCLUDE) -I../geojson -I.. -I../.. \
	 $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_gme.h