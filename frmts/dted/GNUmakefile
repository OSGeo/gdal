
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	dted_api.o dteddataset.o

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(foreach file,$(OBJ),../o/$(file))

%.o:	%.c
	$(CC) -c $(CFLAGS) $(GDAL_INCLUDE) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(CXXFLAGS) $(GDAL_INCLUDE) $*.cpp -o $*.o


../o/%.o:	%.o
	cp $*.o ../o

