
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	dted_api.o dteddataset.o dted_create.o dted_ptstream.o

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(foreach file,$(OBJ),../o/$(file))

%.o:    %.c
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:    %.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) $*.cpp -o $*.o

dted_test:	dted_test.o $(OBJ)
	$(CXX) -o dted_test dted_test.o dted_api.o dted_create.o dted_ptstream.o \
		 ../../port/cpl.a -lm 
