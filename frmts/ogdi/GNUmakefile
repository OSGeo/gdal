GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ogdidataset.o

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(CFLAGS) $(GDAL_INCLUDE) $(OGDI_INCLUDE) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(CXXFLAGS) $(GDAL_INCLUDE) $(OGDI_INCLUDE) $*.cpp -o $*.o

install-obj:	$(foreach file,$(OBJ),../o/$(file))

../o/%.o:	%.o
	cp $*.o ../o
