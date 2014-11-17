

include ../../../GDALmake.opt

OBJ	=	ogrsdedriver.o ogrsdedatasource.o ogrsdelayer.o

CPPFLAGS	:=	 $(SDE_INC) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_sde.h
