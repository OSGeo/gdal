
include ../../GDALmake.opt

OBJ =	gdalwmscache.o gdalwmsdataset.o gdalwmsrasterband.o \
	gdalhttp.o minidriver.o \
	wmsutils.o wmsdriver.o minidriver_wms.o \
	minidriver_tileservice.o minidriver_worldwind.o \
	minidriver_tms.o minidriver_tiled_wms.o wmsmetadataset.o \
	minidriver_virtualearth.o minidriver_arcgis_server.o \
	minidriver_iip.o minidriver_mrf.o minidriver_ogcapimaps.o \
	minidriver_ogcapicoverage.o

CPPFLAGS	:=	 $(CPPFLAGS) -DHAVE_CURL $(CURL_INC)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(OBJ) $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ):	gdalhttp.h minidriver_tileservice.h \
	minidriver_wms.h minidriver_worldwind.h minidriver_tms.h \
	minidriver_tiled_wms.h wmsdriver.h wmsmetadataset.h \
	minidriver_virtualearth.h minidriver_arcgis_server.h \
        minidriver_mrf.h
