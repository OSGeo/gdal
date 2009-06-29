
include ../../GDALmake.opt

OBJ	=	tildataset.o

CPPFLAGS	:=	-I../vrt $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
