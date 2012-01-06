
include ../../GDALmake.opt

OBJ	=	mbtilesdataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) -I../../ogr

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
