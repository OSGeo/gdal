GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	fitsdataset.o

FITS_OPTS	=	


default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $(FITS_OPTS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) $(FITS_OPTS) $*.cpp -o $*.o


install-obj:	$(foreach file,$(OBJ),../o/$(file))
