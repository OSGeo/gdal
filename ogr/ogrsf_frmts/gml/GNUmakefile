
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	gmlpropertydefn.o gmlfeatureclass.o gmlfeature.o gmlreader.o \
		gmlhandler.o gmlreadstate.o trstring.o

ALL_C_FLAGS	=	-I/usr/local/include/xercesc \
	-I.. -I../.. -I$(GDAL_ROOT)/port \
	$(filter-out -Wall,$(CFLAGS))
XERCESLIB =	-lxerces-c1_6_0 -lpthread



default:	$(OBJ)

clean:
	rm -f *.o gmldump

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) $*.cpp -o $*.o
#	ar r ../ogrsf_frmts.a $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) $*.c -o $*.o
#	ar r ../ogrsf_frmts.a $*.o

gmlview:	gmlview.o $(OBJ)
	$(CXX) $(LNK_FLAGS) gmlview.o $(OBJ) $(XERCESLIB) $(CPL_LIB) \
		-o gmlview

