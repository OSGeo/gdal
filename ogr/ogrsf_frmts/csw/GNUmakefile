

include ../../../GDALmake.opt

OBJ	=	ogrcswdataset.o

CPPFLAGS	:=	-iquote .. -iquote ../.. -iquote ../gml -iquote ../wfs  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	../wfs/ogr_wfs.h ../../ogr_swq.h
