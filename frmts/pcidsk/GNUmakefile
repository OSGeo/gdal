GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	pcidskdataset.o

CPPFLAGS	+=	$(GDAL_INCLUDE) -I../raw

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
