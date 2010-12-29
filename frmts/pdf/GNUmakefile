
OBJ	=	pdfdataset.o pdfio.o

include ../../GDALmake.opt

ifeq ($(POPPLER_HAS_OPTCONTENT),yes)
CPPFLAGS +=  -DPOPPLER_HAS_OPTCONTENT
endif

ifeq ($(POPPLER_BASE_STREAM_HAS_TWO_ARGS),yes)
CPPFLAGS +=  -DPOPPLER_BASE_STREAM_HAS_TWO_ARGS
endif

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) $(POPPLER_INC)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
