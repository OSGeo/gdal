
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OGR_OBJ =	ogravcbindriver.o ogravcbindatasource.o ogravcbinlayer.o \
		ogravclayer.o ogravcdatasource.o

AVC_OBJ	=	avc_bin.o avc_binwr.o avc_e00gen.o avc_e00parse.o \
		avc_e00write.o avc_e00read.o avc_mbyte.o avc_misc.o \
		avc_rawbin.o 

OBJ =	$(AVC_OBJ) $(OGR_OBJ)

ALL_C_FLAGS	=	-I../shape -I.. -I../.. -I$(GDAL_ROOT)/port $(CFLAGS)


default:	$(O_OBJ)

clean:
	rm -f *.o

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) $*.cpp -o $*.o
	ar r ../ogrsf_frmts.a $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) $*.c -o $*.o
	ar r ../ogrsf_frmts.a $*.o

