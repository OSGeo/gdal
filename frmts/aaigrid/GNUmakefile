GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	aaigriddataset.o

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(CFLAGS) $(GDAL_INCLUDE) $(XTRA_OPT) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(CXXFLAGS) $(GDAL_INCLUDE) $(XTRA_OPT) $*.cpp -o $*.o


all:	$(OBJ)

install-obj:	$(foreach file,$(OBJ),../o/$(file))

../o/%.o:	%.o
	cp $*.o ../o

