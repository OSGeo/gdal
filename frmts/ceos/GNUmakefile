
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ceosopen.o ceosdataset.o


default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) $*.cpp -o $*.o


ceostest:	ceostest.o ceosopen.o
	$(CXX) ceostest.o ceosopen.o $(GDAL_ROOT)/port/*.o $(LIBS) -o ceostest

install-obj:	$(foreach file,$(OBJ),../o/$(file))
