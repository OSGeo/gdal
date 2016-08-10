include ../../GDALmake.opt

OBJ	=	pdfdataset.o pdfio.o pdfobject.o pdfcreatecopy.o ogrpdflayer.o pdfwritabledataset.o pdfreadvectors.o

PLUGIN_DL =	gdal_PDF.so

ifeq ($(MACOSX_FRAMEWORK),yes)
PLUGIN_DL = gdal_PDF.dylib
LDFLAGS += -Wl,-undefined -Wl,dynamic_lookup -stdlib=libstdc++
CPPFLAGS += -stdlib=libstdc++
LD_SHARED = $(LD) -bundle
endif

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

ifeq ($(HAVE_PDFIUM),yes)
CPPFLAGS +=  -DHAVE_PDFIUM
endif

$(O_OBJ):       pdfobject.h pdfio.h pdfcreatecopy.h gdal_pdf.h ../../ogr/ogrsf_frmts/mem/ogr_mem.h

CPPFLAGS	:=	 -I../vrt -I../mem -I../../ogr/ogrsf_frmts/mem $(CPPFLAGS) $(POPPLER_INC) $(PODOFO_INC) $(PDFIUM_INC)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
	rm -f $(PLUGIN_DL)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

plugin: $(PLUGIN_DL)

$(PLUGIN_DL):  $(OBJ)
	$(LD_SHARED) $(OBJ) ../../ogr/ogrsf_frmts/o/ogrmemdatasource.o ../../ogr/ogrsf_frmts/o/ogrmemdriver.o ../../ogr/ogrsf_frmts/o/ogrmemlayer.o $(LDFLAGS) $(CONFIG_LIBS) -o $(PLUGIN_DL) $(PDFIUM_PLUGIN_LIB) $(POPPLER_PLUGIN_LIB) $(PODOFO_PLUGIN_LIB)
