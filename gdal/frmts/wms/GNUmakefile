
include ../../GDALmake.opt

OBJ	=	cache.o dataset.o gdalhttp.o md5.o minidriver.o rasterband.o stuff.o wmsdriver.o \
		minidriver_wms.o minidriver_tileservice.o minidriver_worldwind.o


CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) $(CURL_INC)

default:	$(OBJ)

clean:
	rm -f *.o $(OBJ) $(O_OBJ)

install-obj:	$(O_OBJ)
