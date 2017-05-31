

include ../../../GDALmake.opt

OBJ	=	ogropenairdriver.o ogropenairdatasource.o ogropenairlayer.o ogropenairlabellayer.o

CPPFLAGS	:=	-I.. -I../.. $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_openair.h