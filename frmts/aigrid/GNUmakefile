
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

AIGOBJ  =	gridlib.o aigopen.o aigccitt.o
OBJ	=	aigdataset.o giodataset.o $(AIGOBJ)
CPPFLAGS	=	$(GDAL_INCLUDE)

default:	$(OBJ)

clean:
	rm -f *.o aitest

aitest:	aitest.o $(AIGOBJ)
	$(CXX) aitest.o $(AIGOBJ) $(GDAL_ROOT)/port/*.o -ldl -lm -o aitest

aigrid2tif:	aigrid2tif.o $(AIGOBJ)
	$(CXX) aigrid2tif.o $(AIGOBJ) $(GDAL_ROOT)/port/*.o $(LIBS) -ltiff \
		-o aigrid2tif

install-obj:	$(foreach file,$(OBJ),../o/$(file))

aitest-dist:
	rm -rf aitest_dist
	mkdir aitest_dist
	cp gridlib.c aitest.c aigrid.h aigopen.c aigccitt.c aitest_dist
	cp $(GDAL_ROOT)/port/cpl_{vsisimple,conv,error}.cpp aitest_dist
	cp $(GDAL_ROOT)/port/cpl_{port,conv,vsi,config,error}.h aitest_dist
	cp Makefile.dist aitest_dist/Makefile

