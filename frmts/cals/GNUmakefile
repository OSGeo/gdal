GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	calsdataset.o

ifeq ($(TIFF_SETTING),internal)
CPPFLAGS	:=	-I../gtiff/libtiff $(CPPFLAGS)
endif

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

