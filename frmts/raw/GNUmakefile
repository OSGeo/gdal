
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	rawdataset.o ehdrdataset.o pauxdataset.o doq1dataset.o \
		doq2dataset.o mffdataset.o hkvdataset.o pnmdataset.o \
		fujibasdataset.o envidataset.o gscdataset.o fastdataset.o \
		atlsci_spheroid.o

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) $*.cpp -o $*.o

install-obj:	$(foreach file,$(OBJ),../o/$(file))
