
include ../../GDALmake.opt

AIGOBJ  =	gridlib.o aigopen.o aigccitt.o
OBJ	=	aigdataset.o giodataset.o $(AIGOBJ)

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o aitest $(OBJ)

aitest:	aitest.o $(AIGOBJ)
	$(CXX) aitest.o $(AIGOBJ) ../../port/*.o $(CONFIG_LIBS) -o aitest

aigrid2tif:	aigrid2tif.o $(AIGOBJ)
	$(CXX) aigrid2tif.o $(AIGOBJ) ../../port/*.o $(LIBS) -ltiff \
		-o aigrid2tif

install-obj:	$(O_OBJ)

aitest-dist:
	rm -rf aitest_dist
	mkdir aitest_dist
	cp gridlib.c aitest.c aigrid.h aigopen.c aigccitt.c aitest_dist
	cp ../../port/cpl_{vsisimple,conv,error}.cpp aitest_dist
	cp ../../port/cpl_{port,conv,vsi,config,error}.h aitest_dist
	cp Makefile.dist aitest_dist/Makefile

