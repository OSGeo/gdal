
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	elasdataset.o

CPPFLAGS	=	$(GDAL_INCLUDE)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(foreach file,$(OBJ),../o/$(file))
