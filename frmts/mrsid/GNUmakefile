GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	mrsiddataset.o

ifeq ($(GEOTIFF_SETTING),internal)
GEOTIFF_INCLUDE	=	-I$(GDAL_ROOT)/frmts/gtiff/libgeotiff
endif

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) $(MRSID_INCLUDE) $(GEOTIFF_INCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(MRSID_INCLUDE) $(GEOTIFF_INCLUDE) $(CXXFLAGS) $*.cpp -o $*.o


install-obj:	$(foreach file,$(OBJ),../o/$(file))
