GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	jpeg2000dataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(JASPER_INCLUDE) $(CPPFLAGS)
CXXFLAGS	+=	-g -O2

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
