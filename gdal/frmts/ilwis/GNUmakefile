
include ../../GDALmake.opt

OBJ	=	ilwisdataset.o ilwiscoordinatesystem.o

CPPFLAGS	=	$(GDAL_INCLUDE)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
