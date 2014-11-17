

include ../../../GDALmake.opt

OBJ	=	ogrcsvdriver.o ogrcsvdatasource.o ogrcsvlayer.o

CPPFLAGS	:=	-I.. -I../..  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_csv.h

