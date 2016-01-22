

include ../../../GDALmake.opt

OBJ	=	FGdbDatasource.o FGdbDriver.o FGdbLayer.o FGdbUtils.o FGdbResultLayer.o 

CPPFLAGS	:=	-I../generic $(GDAL_INCLUDE) $(FGDB_INC) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_fgdb.h
