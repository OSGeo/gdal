GDAL_ROOT	=	..

include ../GDALmake.opt

OBJ	=	ogrgeometryfactory.o ogrpoint.o ogrcurve.o ogrlinestring.o \
		ogrlinearring.o ogrpolygon.o ogrutils.o ogrgeometry.o \
		ogrgeometrycollection.o ogrmultipolygon.o ogrsurface.o \
		ogrmultipoint.o ogrmultilinestring.o \
		\
		ogrfeature.o ogrfeaturedefn.o ogrfielddefn.o \
		ogrspatialreference.o ogr_srsnode.o ogr_srs_proj4.o

LIBS	:=	$(CPL_LIB) $(LIBS) 

OGR_LIB  =	ogr.a
FRMTS_LIB = 	ogrsf_frmts/ogrsf_frmts.a

LINCLUDE =	-Iogrsf_frmts -I. -I$(GDAL_ROOT)/port

default:	$(OGR_LIB)

all:	sublibs test1 test2 test_ogrsf docs

clean:
	rm -f *.o test1 test2 
	(cd ogrsf_frmts; $(MAKE) clean)

very-clean: clean
	rm -rf html

test1:	test1.o $(OGR_LIB)
	$(CXX) $(LNK_FLAGS) test1.o $(OGR_LIB) $(LIBS) -o test1

test2:	test2.o $(OGR_LIB) $(FRMTS_LIB)
	$(CXX) $(LNK_FLAGS) test2.o $(FRMTS_LIB) $(OGR_LIB) \
						$(LIBS) -o test2

test_ogrsf:	test_ogrsf.o $(OGR_LIB) $(FRMTS_LIB)
	$(CXX) $(LNK_FLAGS) test_ogrsf.o $(FRMTS_LIB) $(OGR_LIB) \
						$(LIBS) -o test_ogrsf

ogrinfo:	ogrinfo.o $(OGR_LIB) $(FRMTS_LIB)
	$(CXX) $(LNK_FLAGS) ogrinfo.o $(FRMTS_LIB) $(OGR_LIB) \
						$(LIBS) -o ogrinfo

$(FRMTS_LIB):
	(cd ogrsf_frmts; $(MAKE))

sublibs:
	(cd ogrsf_frmts; $(MAKE))

docs:
	rm -rf html/*
	doxygen


$(OGR_LIB):	$(OBJ)
	ar r $(OGR_LIB) $?

%.o:	%.c
	$(CC) -c $(LINCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(LINCLUDE) $(CFLAGS) $*.cpp -o $*.o


