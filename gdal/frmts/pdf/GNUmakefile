include ../../GDALmake.opt

OBJ	=	pdfdataset.o pdfio.o pdfobject.o pdfcreatecopy.o ogrpdflayer.o pdfwritabledataset.o pdfreadvectors.o

ifeq ($(HAVE_POPPLER),yes)
CPPFLAGS +=  -DHAVE_POPPLER
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

ifeq ($(POPPLER_0_23_OR_LATER),yes)
CPPFLAGS +=  -DPOPPLER_0_23_OR_LATER
endif

ifeq ($(HAVE_PODOFO),yes)
CPPFLAGS +=  -DHAVE_PODOFO
endif

$(O_OBJ):       pdfobject.h pdfio.h pdfcreatecopy.h gdal_pdf.h

CPPFLAGS	:=	 $(CPPFLAGS) $(POPPLER_INC) $(PODOFO_INC) -I../vrt -I../mem -I../../ogr/ogrsf_frmts/mem

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
