
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	oci_utils.o ogrocisession.o ogrocistatement.o \
		ogrocidriver.o ogrocidatasource.o ogrocilayer.o \
		ogrocitablelayer.o ogrociselectlayer.o ogrocistringbuf.o \
		ogrociwritablelayer.o ogrociloaderlayer.o

CPPFLAGS	=	-I.. -I../.. $(OCI_INCLUDE)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)

fastload:	fastload.cpp
	$(CXX) $(CPPFLAGS) $(CFLAGS) fastload.cpp -L../../.. -lgdal.1.1 \
		-o fastload
