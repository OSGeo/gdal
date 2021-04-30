
include ../../GDALmake.opt

#SOURCES = VRC.cpp VRCutils.cpp VRCthirtysix.cpp VRHV.cpp png_crc.c
#SOURCES = VRC.cpp VRCutils.cpp VRCthirtysix.cpp  png_crc.c
#OBJ = $(patsubst %.c, %.o, $(patsubst %.cpp, %.o, $(SOURCES)))
OBJ = VRC.o VRCutils.o VRCthirtysix.o  png_crc.o

PLUGIN_DL =	gdal_VRC.so

ifeq ($(MACOSX_FRAMEWORK),yes)
PLUGIN_DL = gdal_VRC.dylib
LDFLAGS += -Wl,-undefined -Wl,dynamic_lookup
LD_SHARED = $(LD) -bundle
endif

$(O_OBJ):       VRC.h  VRCutils.h  png_crc.h

CPPFLAGS	:=	 -I../vrt -I../mem -I../../ogr/ogrsf_frmts/mem $(CPPFLAGS) $(POPPLER_INC) $(PODOFO_INC) $(PDFIUM_INC) -DDO_NOT_USE_DEBUG_BOOL

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
	rm -f $(PLUGIN_DL)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

plugin: $(PLUGIN_DL)

$(PLUGIN_DL):  $(OBJ)
	$(LD_SHARED) $(OBJ) ../../ogr/ogrsf_frmts/o/ogrmemdatasource.o ../../ogr/ogrsf_frmts/o/ogrmemdriver.o ../../ogr/ogrsf_frmts/o/ogrmemlayer.o $(LDFLAGS) -o $(PLUGIN_DL) $(PDFIUM_PLUGIN_LIB) $(POPPLER_PLUGIN_LIB) $(PODOFO_PLUGIN_LIB) $(CONFIG_LIBS)

# ---------------------------
VRC_OPTS	=

CPPFLAGS	:=	 $(VRC_OPTS) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

# include ../GDALmake.opt
include GDALmake.opt

# gdal looks for the plugin in GDAL_DRIVER_PATH 

#OBJ       = viewrangerdataset.o
#PLUGIN_SO = gdal_viewranger.so
#OBJ       = VRC.o VRCutils.o VRCthirtysix.o VRHV.o png_crc.o
PLUGIN_SO = gdal_VRC.so gdal_VRHV.so

PLUGIN_PATH = $(prefix)/lib/gdalplugins/$(GDAL_VERSION_MAJOR).$(GDAL_VERSION_MINOR)
FILES   =       PNG_band VRC VRCthirtysix VRHV
#OBJ     =       $(addsuffix .o, $(FILES))
LO_O_OBJ        =       $(addsuffix .lo,$(basename $(O_OBJ)))

SOURCES = VRC.cpp VRCutils.cpp VRCthirtysix.cpp VRHV.cpp png_crc.c
OBJS= $(patsubst %.c, %.o, $(patsubst %.cpp, %.o, $(SOURCES)))
GFANLOGS= $(patsubst %.c, %.gccfan, $(patsubst %.cpp, %.gccfan, $(SOURCES)))
# GFAN rule used to generate .o files, which we don't really want
# GFANOBJS= $(patsubst %, %.o, $(GFANLOGS))

ifeq ($(PNG_SETTING),internal) 
XTRA_OPT        :=      $(XTRA_OPT) -I../png/libpng -DINTERNAL_PNG
endif

ifeq ($(LIBZ_SETTING),internal) 
XTRA_OPT        :=      $(XTRA_OPT) -I../zlib
endif

EXTRA_LIBS := -lpng -lz -lefence
#CFLAGS := $(GDAL_INCLUDE) -fPIC -DFRMT_viewranger $(CXXFLAGS) $(EXTRA_CFLAGS) -g -O3 -fstack-protector
CFLAGS := $(GDAL_INCLUDE) -fPIC -DFRMT_viewranger $(EXTRA_CFLAGS) -g -O3 -fstack-protector
# CFLAGS +=  -Wpedantic
# CFLAGS += -D_FORTIFY_SOURCE=2
CXXFLAGS += $(GDAL_INCLUDE) -DFRMT_viewranger
CXXFLAGS += -g -O3

#ifeq ($(CXX), clang++)
ifneq (, $(findstring llvm/,$(CC)))
    # Clang LLVM compiler
    #EXTRA_LIBS += -lLLVMInstrumentation
    #CXXFLAGS += -fsanitize=integer
    #CXXFLAGS += -fsanitize=signed-integer-overflow
    #CXXFLAGS += -fsanitize=unsigned-integer-overflow
    # EXTRA_LIBS += -lubsan
    # llvm linker
    LDFLAGS += -fPIC -v -fuse-ld=lld -shared
endif

ifeq (, $(findstring c++-analyzer,$(CXX)))
    # EXTRA_LIBS += -L/usr/lib64/llvm -L/usr/lib/llvm-6.0/lib -lLLVMInstrumentation
    # CXXFLAGS += -std=c++11
endif

ifneq (, $(findstring gcc/,$(LOADEDMODULES)))
    ifeq (, $(findstring gcc/4,$(LOADEDMODULES)))
        # eg LOADEDMODULES=use.own:gdal/2.2.4
        # These require gcc version >= 4.9.1
        #EXTRA_LIBS += -lasan
        #CFLAGS += -fsanitize=address
        # These requires version >= 4.8
        #CFLAGS += -fstack-check
        #CXXFLAGS += -std=c++11 # Not available in g++ v4
        # Does GCC support these yet ?
        #EXTRA_LIBS += -lubsan
        #CXXFLAGS += -fsanitize=integer
        #CXXFLAGS += -fsanitize=unsigned-integer-overflow
        #CXXFLAGS += -fsanitize=signed-integer-overflow
	else
		CXXFLAGS += -std=c++11
    endif
    CXXFLAGS += -Wall -Wextra -Werror
    # CXXFLAGS += -DFRMT_viewranger
    CXXFLAGS += -Wno-unknown-pragmas #pragma clang diagnostic not known to gcc
endif

#ifneq (, $(findstring clang,$(CC)))
ifneq (, $(findstring clang,$(CXX)))
	ifeq (, $(findstring c++-analyzer,$(CXX)))

        # For SL6 clang v3 (v3.4.2)
		CXXFLAGS += -std=c++11

        # enabling cfi for clang
        # https://blog.trailofbits.com/2016/10/17/lets-talk-about-cfi-clang-edition/
        # Debug
        # CXXFLAGS += -fvisibility=hidden -flto -fno-sanitize-trap=all -fsanitize=cfi
        # Release
        # CXXFLAGS += -fvisibility=hidden -flto -fsanitize=cfi

        CXXFLAGS += -Wall -Wextra
        CXXFLAGS += -Werror

        # gcc/g++ doesn't know this one
		CXXFLAGS += -Weverything

        # so that I can use older compilers - eg on SL6
		CXXFLAGS += -Wno-error=unknown-warning-option

		CXXFLAGS += -Wno-error=format-pedantic # 3 in png2ppm
		CXXFLAGS += -Wno-format-pedantic
	    #CXXFLAGS += -Wno-format-pedantic # 3 in png2ppm
	    #CXXFLAGS += -Wno-error=c++98-compat # 13 in png2ppm
		CXXFLAGS += -Wno-c++98-compat # 13 in png2ppm
		CXXFLAGS += -Wno-error=c++98-compat-pedantic # 1 in png2ppm
		CXXFLAGS += -Wno-c++98-compat-pedantic # 1 in png2ppm
	#	CXXFLAGS += -Wno-error-unreachable-code-break # 1 in png2ppm
        #CXXFLAGS += -Wno-unreachable-code-break # 1 in png2ppm

        # With -Werror -Wno-c++17-extensions I can use [[fallthrough]] and -std=c++11
        # CXXFLAGS += -Wc++17-extensions
	endif
endif

#CPPCHECK=cppcheck --suppress=missingIncludeSystem --template=cppcheck1 --safe-functions --safe-classes
#CPPCHECK=cppcheck $(GDAL_INCLUDE) --suppress=missingIncludeSystem --template=cppcheck1 --safe-functions --safe-classes
#CPPCHECK=cppcheck --suppress=missingInclude --template=cppcheck1 --safe-functions --safe-classes
CPPCHECK=cppcheck -DFRMT_viewranger -DDO_PNG_STUFF --suppress=missingInclude --template=cppcheck1 -Unullptr -Uoverride
#CPPCHECK=cppcheck --template=cppcheck1 --safe-functions --safe-classes

#default:	$(OBJ:.o=.$(OBJ_EXT))
#default:	plugin png2ppm
default:	plugin

BUGHUNTTARGETS=VRC.bughunt VRCthirtysix.bughunt VRCutils.bughunt cppcheck_harness.bughunt png_crc.bughunt VRHV.bughunt

printenv:
	echo CC=$(CC)
	echo CFLAGS=$(CFLAGS)
	echo CPPFLAGS=$(CPPFLAGS)
	echo CXX=$(CXX)
	echo CXXFLAGS=$(CXXFLAGS)
	echo EXTRA_LIBS=$(EXTRA_LIBS)
	printenv | sort
	@echo findstring: $(findstring gcc/,$(LOADEDMODULES))
	@echo findstring: $(findstring gcc/4,$(LOADEDMODULES))

bughuntclean:
	rm -f $(BUGHUNTTARGETS)

clean:
	rm -f *.o
	# rm -f *.so

# Cle
install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

# $(OBJ) $(O_OBJ)::

plugin: $(PLUGIN_SO)

#$(PLUGIN_SO):   $(OBJ)
#	$(LD_SHARED) $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS) $(EXTRA_LIBS) -o $(PLUGIN_SO)
gdal_VRC.so : VRC.o VRCutils.o VRCthirtysix.o png_crc.o
	$(LD_SHARED) $(LNK_FLAGS) $^ $(CONFIG_LIBS) $(EXTRA_LIBS) -o $@


gdal_VRHV.so : VRHV.o VRCutils.o
	$(LD_SHARED) $(LNK_FLAGS) $^ $(CONFIG_LIBS) $(EXTRA_LIBS) -o $@

png2ppm : png2ppm.o png_crc.o
	$(LD_SHARED) $(LNK_FLAGS) png2ppm.o png_crc.o $(EXTRA_LIBS) -o $@
