

include ../../../GDALmake.opt

OBJ	=	ogrcartodbdriver.o ogrcartodbdatasource.o ogrcartodblayer.o ogrcartodbtablelayer.o ogrcartodbresultlayer.o

CPPFLAGS	:=	$(JSON_INCLUDE) -I.. -I../.. -I../pgdump $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_cartodb.h