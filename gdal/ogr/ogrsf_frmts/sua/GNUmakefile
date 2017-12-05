

include ../../../GDALmake.opt

OBJ	=	ogrsuadriver.o ogrsuadatasource.o ogrsualayer.o

CPPFLAGS	:=	-I.. -I../.. $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_sua.h