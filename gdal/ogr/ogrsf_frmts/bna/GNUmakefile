include ../../../GDALmake.opt

OBJ	=	ogrbnadriver.o ogrbnadatasource.o ogrbnalayer.o ogrbnaparser.o

CPPFLAGS	:=	-I.. -I../..  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_bna.h ogrbnaparser.h

