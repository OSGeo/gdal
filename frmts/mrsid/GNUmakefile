
include ../../GDALmake.opt

OBJ	=	mrsiddataset.o

ifeq ($(GEOTIFF_SETTING),internal)
GEOTIFF_INCLUDE	=	-I../../frmts/gtiff/libgeotiff
endif

CPPFLAGS	:=	$(GDAL_INCLUDE) $(MRSID_FLAGS) $(MRSID_INCLUDE) $(GEOTIFF_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)
