
include ../../GDALmake.opt

AIGOBJ  =	gridlib.o aigopen.o aigccitt.o
OBJ	=	aigdataset.o $(AIGOBJ)

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) -I../../ogr/ogrsf_frmts/avc

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o aitest$(EXE) $(OBJ) $(O_OBJ)

aitest$(EXE):	aitest.$(OBJ_EXT)
	$(LD) $(LDFLAGS) aitest.$(OBJ_EXT) $(CONFIG_LIBS) -o aitest$(EXE)

aigrid2tif$(EXE):	aigrid2tif.$(OBJ_EXT)
	$(LD) $(LDFLAGS) aigrid2tif.$(OBJ_EXT) $(CONFIG_LIBS) -o aigrid2tif$(EXE)

$(OBJ) $(O_OBJ):	aigrid.h

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

aitest-dist:
	rm -rf aitest_dist
	mkdir aitest_dist
	cp gridlib.c aitest.c aigrid.h aigopen.c aigccitt.c aitest_dist
	cp ../../port/cpl_{vsisimple,conv,error}.cpp aitest_dist
	cp ../../port/cpl_{port,conv,vsi,config,error}.h aitest_dist
	cp Makefile.dist aitest_dist/Makefile

