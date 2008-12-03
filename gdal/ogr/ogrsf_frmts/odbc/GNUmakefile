

include ../../../GDALmake.opt

OBJ	=	ogrodbcdatasource.o ogrodbclayer.o ogrodbcdriver.o \
		ogrodbctablelayer.o ogrodbcselectlayer.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -I.. -I../.. $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
