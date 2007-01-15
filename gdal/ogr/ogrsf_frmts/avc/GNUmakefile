

include ../../../GDALmake.opt

OGR_OBJ =	ogravcbindriver.o ogravcbindatasource.o ogravcbinlayer.o \
		ogravclayer.o ogravcdatasource.o \
		ogravce00layer.o ogravce00datasource.o 

AVC_OBJ	=	avc_bin.o avc_binwr.o avc_e00gen.o avc_e00parse.o \
		avc_e00write.o avc_e00read.o avc_mbyte.o avc_misc.o \
		avc_rawbin.o 

OBJ =	$(AVC_OBJ) $(OGR_OBJ)

CPPFLAGS	:=	-I../shape -I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)


default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_avc.h avc.h 
