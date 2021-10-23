

include ../../../GDALmake.opt

OGR_OBJ =	ogravcbindriver.o ogravcbindatasource.o ogravcbinlayer.o \
		ogravclayer.o ogravcdatasource.o \
		ogravce00layer.o ogravce00datasource.o ogravce00driver.o

AVC_OBJ	=	avc_bin.o avc_e00gen.o avc_e00parse.o \
		avc_e00read.o avc_mbyte.o avc_misc.o \
		avc_rawbin.o

OBJ =	$(AVC_OBJ) $(OGR_OBJ)

CPPFLAGS	:=	-iquote ../shape -iquote .. -iquote ../..  $(CPPFLAGS)

ifeq ($(RENAME_INTERNAL_SHAPELIB_SYMBOLS),yes)
CPPFLAGS 	:=	$(CPPFLAGS) -DRENAME_INTERNAL_SHAPELIB_SYMBOLS -DSHPAPI_CALL=
endif

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_avc.h avc.h
