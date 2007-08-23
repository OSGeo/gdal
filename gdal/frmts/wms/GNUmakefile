
include ../../GDALmake.opt

OBJ	=	cache.o dataset.o datawindow.o gdalhttp.o md5.o minidriver.o minidriver_wms.o rasterband.o stuff.o wmsdriver.o



CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) $(CURL_INC)

default:	$(OBJ)

clean:
	rm -f *.o $(OBJ) $(O_OBJ)

install-obj:	$(O_OBJ)
