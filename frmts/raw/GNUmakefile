

include ../../GDALmake.opt

OBJ	=	rawdataset.o ehdrdataset.o pauxdataset.o doq1dataset.o \
		doq2dataset.o mffdataset.o hkvdataset.o pnmdataset.o \
		fujibasdataset.o envidataset.o gscdataset.o fastdataset.o \
		atlsci_spheroid.o btdataset.o landataset.o cpgdataset.o \
		idadataset.o ndfdataset.o dipxdataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)

$(OBJ) $(O_OBJ):	rawdataset.h

install:
	$(INSTALL_DATA) rawdataset.h $(INST_INCLUDE)
