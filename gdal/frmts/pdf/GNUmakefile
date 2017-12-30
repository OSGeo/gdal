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

ifeq ($(POPPLER_0_58_OR_LATER),yes)
CPPFLAGS +=  -DPOPPLER_0_58_OR_LATER
endif

ifeq ($(HAVE_PODOFO),yes)
CPPFLAGS +=  -DHAVE_PODOFO
endif

ifeq ($(HAVE_PDFIUM),yes)
CPPFLAGS +=  -DHAVE_PDFIUM
# -Wzero-as-null-pointer-constant  isn't compatible of pdfium headers
# /home/even/pdfium/install/include/pdfium/third_party/base/numerics/safe_conversions.h: In instantiation of ‘Dst pdfium::base::checked_cast(Src) [with Dst = int; Src = long unsigned int]’:
# /home/even/pdfium/install/include/pdfium/core/include/fxcrt/fx_string.h:43:26:   required from here
# /home/even/pdfium/install/include/pdfium/third_party/base/numerics/safe_conversions.h:29:3: erreur: invalid cast from type ‘std::nullptr_t’ to type ‘volatile char*’
#    CHECK(IsValueInRangeForNumericType<Dst>(value));
CXX := $(subst -Wzero-as-null-pointer-constant,,${CXX})
endif

$(O_OBJ):       pdfobject.h pdfio.h pdfcreatecopy.h gdal_pdf.h ../../ogr/ogrsf_frmts/mem/ogr_mem.h

CPPFLAGS	:=	 -I../vrt -I../mem -I../../ogr/ogrsf_frmts/mem $(CPPFLAGS) $(POPPLER_INC) $(PODOFO_INC) $(PDFIUM_INC) -DDO_NOT_USE_DEBUG_BOOL

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
	rm -f $(PLUGIN_DL)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

plugin: $(PLUGIN_DL)

$(PLUGIN_DL):  $(OBJ)
	$(LD_SHARED) $(OBJ) ../../ogr/ogrsf_frmts/o/ogrmemdatasource.o ../../ogr/ogrsf_frmts/o/ogrmemdriver.o ../../ogr/ogrsf_frmts/o/ogrmemlayer.o $(LDFLAGS) $(CONFIG_LIBS) -o $(PLUGIN_DL) $(PDFIUM_PLUGIN_LIB) $(POPPLER_PLUGIN_LIB) $(PODOFO_PLUGIN_LIB)
