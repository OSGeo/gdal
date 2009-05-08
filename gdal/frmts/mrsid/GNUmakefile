
include ../../GDALmake.opt

OBJ	=	mrsiddataset.o mrsidstream.o

ifeq ($(GEOTIFF_SETTING),internal)
GEOTIFF_INCLUDE	=	-I../../frmts/gtiff/libgeotiff
endif

CPPFLAGS	:=	$(GDAL_INCLUDE) $(MRSID_FLAGS) $(MRSID_INCLUDE) $(GEOTIFF_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
