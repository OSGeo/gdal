GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	mrsiddataset.o

ifeq ($(GEOTIFF_SETTING),internal)
GEOTIFF_INCLUDE	=	-I$(GDAL_ROOT)/frmts/gtiff/libgeotiff
endif

CPPFLAGS	+=	$(GDAL_INCLUDE) $(MRSID_INCLUDE) $(GEOTIFF_INCLUDE)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
