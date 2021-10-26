
include ../../../GDALmake.opt

OBJ	=	ogrgpsbabeldriver.o ogrgpsbabeldatasource.o ogrgpsbabelwritedatasource.o

CPPFLAGS	:=	-iquote .. -iquote ../..  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_gpsbabel.h

