
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	rawdataset.o ehdrdataset.o pauxdataset.o doq1dataset.o

ALL_C_FLAGS	=	$(GDAL_INCLUDE) $(CFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.c -o $*.o

install-obj:	$(foreach file,$(OBJ),../o/$(file))

../o/%.o:	%.o
	cp $*.o ../o

