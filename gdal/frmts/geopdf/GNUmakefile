
OBJ	=	geopdfdataset.o

include ../../GDALmake.opt

ifeq ($(POPPLER_HAS_OPTCONTENT),yes)
CPPFLAGS +=  -DPOPPLER_HAS_OPTCONTENT
endif

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) $(POPPLER_INC)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
