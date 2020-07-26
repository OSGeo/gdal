

include ../../../GDALmake.opt

OBJ	=	ogredgdriver.o ogredgdatasource.o ogredglayer.o edgappendix.o

CPPFLAGS	:=	-I.. -I../..  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):	ogr_edg.h

clean:
	rm -f *.o $(O_OBJ)
