

include ../../../GDALmake.opt

OBJ	=	ogredigeodriver.o ogredigeodatasource.o ogredigeolayer.o

CPPFLAGS	:=	-iquote .. -iquote ../..  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_edigeo.h