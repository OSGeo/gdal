

include ../../../GDALmake.opt

OBJ	=	ogrsegydriver.o ogrsegydatasource.o ogrsegylayer.o

CPPFLAGS	:=	-I.. -I../..  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_segy.h