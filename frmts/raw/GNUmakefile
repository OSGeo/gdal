
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	rawdataset.o ehdrdataset.o pauxdataset.o doq1dataset.o \
		doq2dataset.o mffdataset.o hkvdataset.o pnmdataset.o \
		fujibasdataset.o envidataset.o gscdataset.o fastdataset.o \
		atlsci_spheroid.o btdataset.o

CPPFLAGS	+=	$(GDAL_INCLUDE)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
