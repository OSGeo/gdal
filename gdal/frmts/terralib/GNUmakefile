GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	TerraLibDataset.o TerraLibRasterBand.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) -I$(TERRALIBDIR)\src\terralib\kernel

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)

