GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ecwdataset.o

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) -DFRMT_ecw $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) -DFRMT_ecw $(CXXFLAGS) $*.cpp -o $*.o


install-obj:	$(foreach file,$(OBJ),../o/$(file))
