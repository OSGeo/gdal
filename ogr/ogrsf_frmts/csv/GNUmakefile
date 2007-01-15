

include ../../../GDALmake.opt

OBJ	=	ogrcsvdriver.o ogrcsvdatasource.o ogrcsvlayer.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_csv.h

