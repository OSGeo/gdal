
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	oci_utils.o ogrocisession.o ogrocistatement.o \
		ogrocidriver.o ogrocidatasource.o ogrocilayer.o \
		ogrocitablelayer.o ogrociselectlayer.o 

ALL_C_FLAGS	=	-I.. -I../.. -I$(GDAL_ROOT)/port \
		$(OCI_INCLUDE) $(CFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) $*.cpp -o $*.o
	ar r ../ogrsf_frmts.a $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) $*.c -o $*.o
	ar r ../ogrsf_frmts.a $*.o

