
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	vrtdataset.o vrtrasterband.o vrtdriver.o vrtsources.o \
		vrtfilters.o vrtsourcedrasterband.o vrtrawrasterband.o \
		vrtwarped.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)

$(OBJ) $(O_OBJ):	vrtdataset.h

vwtest:	vwtest.o
	$(LD) vwtest.o -L../.. -lgdal -o vwtest

