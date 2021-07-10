

include ../../../GDALmake.opt

OBJ	=	ogrvrtdatasource.o ogrvrtlayer.o ogrvrtdriver.o

CPPFLAGS	:=	-iquote .. -iquote ../.. -iquote ../generic  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):  ogr_vrt.h ../generic/ogrwarpedlayer.h ../generic/ogrunionlayer.h
