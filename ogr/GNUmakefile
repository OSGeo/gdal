GDAL_ROOT	=	..

include ../GDALmake.opt

OBJ	=	ogrgeometryfactory.o ogrpoint.o ogrcurve.o ogrlinestring.o \
		ogrlinearring.o ogrpolygon.o ogrutils.o ogrgeometry.o \
		ogrgeometrycollection.o ogrmultipolygon.o ogrsurface.o \
		ogrmultipoint.o ogrmultilinestring.o \
		\
		ogrfeature.o ogrfeaturedefn.o ogrfielddefn.o 

LIBS	:=	$(CPL_LIB) $(LIBS)

default:	$(OBJ)

all:	test1 docs

clean:
	rm -f *.o test1

very-clean: clean
	rm -rf html

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CFLAGS) $*.cpp -o $*.o


test1:	$(OBJ) test1.o
	$(CXX) $(LNK_FLAGS) test1.o $(OBJ) $(LIBS) -o test1

test2:	$(OBJ) test2.o shape2ogr.o  
	$(CXX) $(LNK_FLAGS) test2.o $(OBJ) shape2ogr.o \
		../frmts/shapelib/shpopen.o \
		../frmts/shapelib/dbfopen.o \
						$(LIBS) -o test2


docs:
	rm -rf html/*
	doxygen
