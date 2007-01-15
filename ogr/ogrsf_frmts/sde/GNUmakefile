

include ../../../GDALmake.opt

OBJ	=	ogrsdedriver.o ogrsdedatasource.o ogrsdelayer.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(SDE_INC) $(CPPFLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_sde.h
