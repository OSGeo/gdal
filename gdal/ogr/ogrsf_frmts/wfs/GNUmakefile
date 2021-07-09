

include ../../../GDALmake.opt

OBJ	=	ogrwfsdriver.o ogrwfsdatasource.o ogrwfslayer.o ogrwfsfilter.o ogrwfsjoinlayer.o ogroapifdriver.o

CPPFLAGS	:=	-iquote .. -iquote ../.. -iquote ../gml  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_wfs.h ../../ogr_swq.h ../gml/gmlreader.h ../gml/parsexsd.h
