

include ../../../GDALmake.opt

OBJ	=	ogrrecdriver.o ogrrecdatasource.o ogrreclayer.o ll_recio.o

CPPFLAGS	:=	-I.. -I../..  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
