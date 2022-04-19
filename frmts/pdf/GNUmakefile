include ../../GDALmake.opt

OBJ	=	pdfdataset.o pdfio.o pdfobject.o pdfcreatecopy.o ogrpdflayer.o pdfwritabledataset.o pdfreadvectors.o pdfcreatefromcomposition.o

PLUGIN_DL =	gdal_PDF.so

ifeq ($(MACOSX_FRAMEWORK),yes)
PLUGIN_DL = gdal_PDF.dylib
LDFLAGS += -Wl,-undefined -Wl,dynamic_lookup
LD_SHARED = $(LD) -bundle
endif

ifeq ($(HAVE_POPPLER),yes)
# Poppler 2022.1 requires c++17
ifeq ($(shell test $(POPPLER_MAJOR_VERSION) -gt 21; echo $$?),0)
CXX := $(subst -std=c++11,,${CXX})
CXX := $(subst -std=c++14,,${CXX})
CXX := ${CXX} -std=c++17
endif
CPPFLAGS +=  -DHAVE_POPPLER -DPOPPLER_MAJOR_VERSION=$(POPPLER_MAJOR_VERSION) -DPOPPLER_MINOR_VERSION=$(POPPLER_MINOR_VERSION)
endif

ifeq ($(HAVE_PODOFO),yes)
CPPFLAGS +=  -DHAVE_PODOFO
endif

ifeq ($(HAVE_PDFIUM),yes)
CPPFLAGS +=  -DHAVE_PDFIUM
CXX := $(subst -std=c++11,-std=c++14,${CXX})
# -Wzero-as-null-pointer-constant  isn't compatible of pdfium headers
# /home/even/pdfium/install/include/pdfium/third_party/base/numerics/safe_conversions.h: In instantiation of ‘Dst pdfium::base::checked_cast(Src) [with Dst = int; Src = long unsigned int]’:
# /home/even/pdfium/install/include/pdfium/core/include/fxcrt/fx_string.h:43:26:   required from here
# /home/even/pdfium/install/include/pdfium/third_party/base/numerics/safe_conversions.h:29:3: erreur: invalid cast from type ‘std::nullptr_t’ to type ‘volatile char*’
#    CHECK(IsValueInRangeForNumericType<Dst>(value));
CXX := $(subst -Wzero-as-null-pointer-constant,,${CXX})
endif

$(O_OBJ):       pdfobject.h pdfio.h pdfcreatecopy.h pdfcreatefromcomposition.h gdal_pdf.h ../../ogr/ogrsf_frmts/mem/ogr_mem.h

CPPFLAGS	:=	 -iquote ../vrt -iquote ../mem -iquote ../../ogr/ogrsf_frmts/mem $(CPPFLAGS) $(POPPLER_INC) $(PODOFO_INC) $(PDFIUM_INC)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
	rm -f $(PLUGIN_DL)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

plugin: $(PLUGIN_DL)

$(PLUGIN_DL):  $(OBJ)
	$(LD_SHARED) $(OBJ) ../../ogr/ogrsf_frmts/o/ogrmemdatasource.o ../../ogr/ogrsf_frmts/o/ogrmemdriver.o ../../ogr/ogrsf_frmts/o/ogrmemlayer.o $(LDFLAGS) -o $(PLUGIN_DL) $(PDFIUM_PLUGIN_LIB) $(POPPLER_PLUGIN_LIB) $(PODOFO_PLUGIN_LIB) $(CONFIG_LIBS)
