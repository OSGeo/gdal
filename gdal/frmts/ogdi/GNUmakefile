
include ../../GDALmake.opt

OBJ	=	ogdidataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(OGDI_INCLUDE) $(PROJ_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
