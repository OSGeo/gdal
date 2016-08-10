

include ../../../GDALmake.opt

OBJ	=	ogramigoclouddriver.o ogramigoclouddatasource.o ogramigocloudlayer.o ogramigocloudtablelayer.o ogramigocloudresultlayer.o

CPPFLAGS	:=	$(JSON_INCLUDE) -I.. -I../.. -I../pgdump $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_amigocloud.h