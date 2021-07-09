

include ../../../GDALmake.opt

OBJ	=	gnmdbdriver.o gnmdbnetwork.o

CPPFLAGS	:=	-iquote . -iquote ../ -iquote ../../ $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
	rm -f *.lo $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))



