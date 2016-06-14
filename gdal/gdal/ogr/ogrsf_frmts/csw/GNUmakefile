

include ../../../GDALmake.opt

OBJ	=	ogrcswdataset.o

CPPFLAGS	:=	-I.. -I../.. -I../gml -I../wfs  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	../wfs/ogr_wfs.h ../../swq.h