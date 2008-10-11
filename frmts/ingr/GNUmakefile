GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	IntergraphDataset.o IntergraphBand.o IngrTypes.o JpegHelper.o

CPPFLAGS	:=	-I../gtiff/libtiff $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

$(OBJ) $(O_OBJ):        IngrTypes.h IntergraphBand.h IntergraphDataset.h JpegHelper.h

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)

