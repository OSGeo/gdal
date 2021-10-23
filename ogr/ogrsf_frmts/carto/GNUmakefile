

include ../../../GDALmake.opt

OBJ	=	ogrcartodriver.o ogrcartodatasource.o ogrcartolayer.o ogrcartotablelayer.o ogrcartoresultlayer.o

CPPFLAGS	:=	-iquote ../geojson $(JSON_INCLUDE) -iquote .. -iquote ../.. -iquote ../pgdump $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_carto.h