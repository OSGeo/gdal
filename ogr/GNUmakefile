
include ../GDALmake.opt

include ./file.lst

INST_H_FILES	=	ogr_core.h ogr_feature.h ogr_geometry.h ogr_p.h \
		ogr_spatialref.h ogr_srs_api.h ogrsf_frmts/ogrsf_frmts.h \
		ogr_featurestyle.h ogr_api.h ogr_geocoding.h ogr_swq.h ogr_recordbatch.h

ifeq ($(HAVE_GEOS),yes)
CPPFLAGS 	:=	-DHAVE_GEOS=1 $(GEOS_CFLAGS) $(CPPFLAGS)
endif

ifeq ($(HAVE_SFCGAL),yes)
CPPFLAGS 	:=	-DHAVE_SFCGAL=1 $(SFCGAL_CFLAGS) $(CPPFLAGS)
endif

CXXFLAGS 	:=	-DHAVE_MITAB $(CXXFLAGS)

ifeq ($(HAVE_EXPAT),yes)
CPPFLAGS 	:=	-DHAVE_EXPAT $(EXPAT_INCLUDE) $(CPPFLAGS)
endif

ifeq ($(HAVE_XERCES),yes)
CPPFLAGS 	:=	-DHAVE_XERCES $(XERCES_INCLUDE) $(CPPFLAGS)
endif

ifeq ($(LIBZ_SETTING),internal)
ZLIB_XTRA_OPT =      -I../frmts/zlib
else
ZLIB_XTRA_OPT =
endif

CPPFLAGS	:=	-iquote ogrsf_frmts -iquote ogrsf_frmts/mem -iquote ogrsf_frmts/geojson $(JSON_INCLUDE) -iquote .  $(PROJ_INCLUDE) $(PROJ_FLAGS) $(CPPFLAGS) $(ZLIB_XTRA_OPT)

CXXFLAGS        :=      $(WARN_EFFCPLUSPLUS) $(WARN_OLD_STYLE_CAST) $(CXXFLAGS)

default:	lib

all:	lib

clean:
	rm -f html/*
	(cd ogrsf_frmts; $(MAKE) clean)
	$(RM) *.o
	$(RM) -f test_geo_utils$(EXE)

very-clean: clean
	rm -rf html rtf

lib:	sublibs $(OBJ:.o=.$(OBJ_EXT))

$(OBJ): ogr_feature.h ogr_geometry.h ogr_swq.h

obj:	$(OBJ)

sublibs:
	(cd ogrsf_frmts; $(MAKE))

docs:
	echo "This target does not exist anymore. Use top-level"

install-docs:
	echo "This target does not exist anymore. Use top-level"

gdalso:	$(GDAL_SLIB)

$(GDAL_SLIB):
	(cd ..; $(MAKE) check-lib)

web-update:	docs
	echo "This target does not exist anymore. Use top-level"

install:
	for f in $(INST_H_FILES) ; \
	    do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_INCLUDE) ; \
	done

test_geo_utils$(EXE):	test_geo_utils.$(OBJ_EXT) ogr_geo_utils.$(OBJ_EXT)
	$(LD) $(LDFLAGS) test_geo_utils.$(OBJ_EXT) ogr_geo_utils.$(OBJ_EXT) $(CONFIG_LIBS) -o test_geo_utils$(EXE)

# The sed substitution below workarounds a bug with gcc 4.1 -O2 (checked on 64bit platforms)
# that produces buggy compiled code.
# Seen on gcc 4.1.2-27ubuntu1 (Ubuntu 10.04) (not the default compiler) and gcc-4.1.2-48.el5 (CentOS 5.5)
# (default compiler...)
# The memset isn't necessary at all with a non-buggy compiler, but I've found
# that it helps gcc 4.1 generating correct code here...
parser:
	bison -p swq -d -oswq_parser.cpp swq_parser.y
	sed -i.bak "s/yytype_int16 yyssa\[YYINITDEPTH\];/yytype_int16 yyssa[YYINITDEPTH]; \/\* workaround bug with gcc 4.1 -O2 \*\/ memset(yyssa, 0, sizeof(yyssa));/" swq_parser.cpp
	sed -i.bak "s/\*yyssp = yystate/\*yyssp = static_cast<yytype_int16>(yystate)/" swq_parser.cpp
	sed -i.bak "s/            \/\* Fall through\.  \*\//            CPL_FALLTHROUGH/" swq_parser.cpp
	sed -i.bak "s/#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)/#define YYSIZE_MAXIMUM static_cast<YYSIZE_T>(-1)/" swq_parser.cpp
	sed -i.bak "s/(unsigned int) (YYX)/static_cast<unsigned int>(YYX)/" swq_parser.cpp
	sed -i.bak "s/(char \*) YYSTACK_ALLOC (yymsg_alloc)/static_cast<char *>(YYSTACK_ALLOC (yymsg_alloc))/" swq_parser.cpp
	sed -i.bak "s/(union yyalloc \*) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize))/static_cast<union yyalloc *>(YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize)))/" swq_parser.cpp
	sed -i.bak "s/const char \*yyformat = YY_NULLPTR;/const char *yyformat = \"\";/" swq_parser.cpp
	rm swq_parser.cpp.bak

swq_parser.$(OBJ_EXT):   swq_parser.cpp
	$(CXX) $(GDAL_INCLUDE) $(CXXFLAGS) $(CPPFLAGS) $(NO_LOGICAL_OP_FLAG) -c -o $@ $<
