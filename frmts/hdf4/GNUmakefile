GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	hdf4dataset.o hdf4imagedataset.o


default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) $(HDF4_INCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(HDF4_INCLUDE) $(CXXFLAGS) $*.cpp -o $*.o


install-obj:	$(foreach file,$(OBJ),../o/$(file))
