

include ../../../GDALmake.opt

OBJ	=	\
	ogrgmedriver.o ogrgmedatasource.o \
	ogrgmelayer.o ogrgmetablelayer.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_gme.h