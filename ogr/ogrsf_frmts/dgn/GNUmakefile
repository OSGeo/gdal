
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	dgnopen.o dgnread.o
OBJ_OGR =	ogrdgndriver.o ogrdgndatasource.o ogrdgnlayer.o

ALL_C_FLAGS	=	-I.. -I../.. -I$(GDAL_ROOT)/port $(CFLAGS)

LIBS	:=	../ogrsf_frmts.a ../../ogr.a $(CPL_LIB) $(LIBS) 


default:	$(OBJ) $(OBJ_OGR)

clean:
	rm -f *.o

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) $*.cpp -o $*.o
	ar r ../ogrsf_frmts.a $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) $*.c -o $*.o
	ar r ../ogrsf_frmts.a $*.o

dgndump:	dgndump.o $(OBJ)
	$(CXX) dgndump.o $(OBJ) $(CPL_LIB) -ldl -lm -o dgndump 


