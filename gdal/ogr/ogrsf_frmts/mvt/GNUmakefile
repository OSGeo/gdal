

include ../../../GDALmake.opt

OBJ	=	ogrmvtdataset.o mvtutils.o

CPPFLAGS	:=	-I.. -I../.. -I../osm $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
