

include ../../../GDALmake.opt

OBJ	=	ogrrecdriver.o ogrrecdatasource.o ogrreclayer.o ll_recio.o

CPPFLAGS	:=	-iquote .. -iquote ../..  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
