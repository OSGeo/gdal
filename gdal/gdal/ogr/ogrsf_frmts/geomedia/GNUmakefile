

include ../../../GDALmake.opt

OBJ	=	ogrgeomediadatasource.o ogrgeomedialayer.o ogrgeomediadriver.o \
		ogrgeomediatablelayer.o ogrgeomediaselectlayer.o

CPPFLAGS	:=	 -I.. -I../.. -I../pgeo $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_geomedia.h
