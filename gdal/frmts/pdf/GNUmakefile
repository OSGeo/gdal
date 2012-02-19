
OBJ	=	pdfdataset.o pdfio.o pdfobject.o pdfcreatecopy.o

include ../../GDALmake.opt

ifeq ($(HAVE_POPPLER),yes)
CPPFLAGS +=  -DUSE_POPPLER
endif

ifeq ($(POPPLER_HAS_OPTCONTENT),yes)
CPPFLAGS +=  -DPOPPLER_HAS_OPTCONTENT
endif

ifeq ($(POPPLER_BASE_STREAM_HAS_TWO_ARGS),yes)
CPPFLAGS +=  -DPOPPLER_BASE_STREAM_HAS_TWO_ARGS
endif

ifeq ($(POPPLER_0_20_OR_LATER),yes)
CPPFLAGS +=  -DPOPPLER_0_20_OR_LATER
endif

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) $(POPPLER_INC) $(PODOFO_INC)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
