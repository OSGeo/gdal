
include ../../GDALmake.opt

ifeq ($(OPENJPEG_TRUNK),yes)
OBJ	=	openjpegdataset.o
else
OBJ	=	openjpegdataset_opjbranchv2.o
endif

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f $(OBJ) $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
