
include ../../GDALmake.opt

AIGOBJ  =	gridlib.o aigopen.o aigccitt.o
OBJ	=	aigdataset.o $(AIGOBJ)

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) -I../../ogr/ogrsf_frmts/avc

default:	$(OBJ)

clean:
	rm -f *.o aitest$(EXE) $(OBJ) $(O_OBJ)

aitest$(EXE):	aitest.o
	$(LD) $(LDFLAGS) aitest.o $(CONFIG_LIBS) -o aitest$(EXE)

aigrid2tif$(EXE):	aigrid2tif.o
	$(LD) $(LDFLAGS) aigrid2tif.o $(CONFIG_LIBS) -o aigrid2tif$(EXE)

install-obj:	$(O_OBJ)

aitest-dist:
	rm -rf aitest_dist
	mkdir aitest_dist
	cp gridlib.c aitest.c aigrid.h aigopen.c aigccitt.c aitest_dist
	cp ../../port/cpl_{vsisimple,conv,error}.cpp aitest_dist
	cp ../../port/cpl_{port,conv,vsi,config,error}.h aitest_dist
	cp Makefile.dist aitest_dist/Makefile

