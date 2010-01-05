GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	IntergraphDataset.o IntergraphBand.o IngrTypes.o JpegHelper.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

ifeq ($(TIFF_SETTING),internal)
CPPFLAGS	:=	$(CPPFLAGS) -I../gtiff/libtiff
endif

default:	$(OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ):        IngrTypes.h IntergraphBand.h IntergraphDataset.h JpegHelper.h

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

