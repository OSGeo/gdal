
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

AIGOBJ  =	gridlib.o aigopen.o
OBJ	=	aigdataset.o giodataset.o $(AIGOBJ)

default:	$(OBJ)

clean:
	rm -f *.o aitest

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) $*.cpp -o $*.o


aitest:	aitest.o $(AIGOBJ)
	$(CXX) aitest.o $(AIGOBJ) $(GDAL_ROOT)/port/*.o $(LIBS) -o aitest

install-obj:	$(foreach file,$(OBJ),../o/$(file))

../o/%.o:	%.o
	cp $*.o ../o


aitest-dist:
	rm -rf aitest_dist
	mkdir aitest_dist
	cp gridlib.c aitest.c aigrid.h aigopen.c aitest_dist
	cp $(GDAL_ROOT)/port/cpl_{vsisimple,conv,error}.cpp aitest_dist
	cp $(GDAL_ROOT)/port/cpl_{port,conv,vsi,config,error}.h aitest_dist
	cp Makefile.dist aitest_dist/Makefile

