GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ogdidataset.o

CPPFLAGS	+=	$(GDAL_INCLUDE) $(OGDI_INCLUDE)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
