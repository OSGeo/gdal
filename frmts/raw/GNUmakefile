
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	rawdataset.o ehdrdataset.o pauxdataset.o doq1dataset.o \
		doq2dataset.o mffdataset.o hkvdataset.o

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(CFLAGS) $(GDAL_INCLUDE) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(CXXFLAGS) $(GDAL_INCLUDE) $*.cpp -o $*.o

install-obj:	$(foreach file,$(OBJ),../o/$(file))

../o/%.o:	%.o
	cp $*.o ../o

