

include ../../GDALmake.opt

OBJ	=	argdataset.o

CPPFLAGS :=	$(JSON_INCLUDE) -I../raw $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
