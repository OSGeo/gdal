GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	usgsdemdataset.o usgsdem_create.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -I$(GDAL_ROOT)/alg $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
