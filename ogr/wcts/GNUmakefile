GDAL_ROOT	=	../..

include ../../GDALmake.opt

LINCLUDE =	-I../ogrsf_frmts -I.. -I$(GDAL_ROOT)/port -DHAVE_CURL

WCTS_OBJ	=	ogrwcts.o
CURLLIB = -lcurl

BIN	=	ogrwcts

default:	$(BIN)

clean:
	rm -f *.o ogrinfo$(EXE) ogr2ogr$(EXE) ogrtindex$(EXE) 
	rm -f sdts2mi$(EXE) testepsg$(EXE)
	rm -f test_ogrsf$(EXE) $(OGR_LIB)
	(cd ogrsf_frmts; $(MAKE) clean)

$(BIN):	$(WCTS_OBJ)
	$(CXX) $(LNK_FLAGS) $(WCTS_OBJ) $(CONFIG_LIBS) $(CURLLIB) -o $(BIN)

%.o:	%.c
	$(CC) -c $(LINCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(LINCLUDE) $(CXXFLAGS) $*.cpp -o $*.o

