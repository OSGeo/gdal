

include ../../../GDALmake.opt

OBJ	=	ogrmapmldataset.o

CPPFLAGS	:=	-I.. -I../.. $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
