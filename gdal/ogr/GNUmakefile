
include ../GDALmake.opt

include ./file.lst

INST_H_FILES	=	ogr_core.h ogr_feature.h ogr_geometry.h ogr_p.h \
		ogr_spatialref.h ogr_srs_api.h ogrsf_frmts/ogrsf_frmts.h \
		ogr_featurestyle.h ogr_api.h ogr_geocoding.h

ifeq ($(HAVE_GEOS),yes)
CPPFLAGS 	:=	-DHAVE_GEOS=1 $(GEOS_CFLAGS) $(CPPFLAGS)
endif

ifeq ($(OGR_ENABLED),yes)
CXXFLAGS 	:=	-DHAVE_MITAB $(CXXFLAGS)
endif

ifeq ($(HAVE_EXPAT),yes)
CPPFLAGS 	:=	-DHAVE_EXPAT $(EXPAT_INCLUDE) $(CPPFLAGS)
endif

ifeq ($(LIBZ_SETTING),internal)
ZLIB_XTRA_OPT =      -I../frmts/zlib
else
ZLIB_XTRA_OPT =
endif

CPPFLAGS	:=	-Iogrsf_frmts -Iogrsf_frmts/mem -I.  $(PROJ_INCLUDE) $(PROJ_FLAGS) $(CPPFLAGS) $(ZLIB_XTRA_OPT)

default:	lib

all:	lib

clean:
	rm -f html/*
	(cd ogrsf_frmts; $(MAKE) clean)
	$(RM) *.o

very-clean: clean
	rm -rf html rtf

lib:	sublibs $(OBJ:.o=.$(OBJ_EXT))

$(OBJ): ogr_feature.h ogr_geometry.h swq.h

obj:	$(OBJ)

ifeq ($(OGR_ENABLED),yes)

sublibs:
	(cd ogrsf_frmts; $(MAKE))

else

sublibs:
	(cd ogrsf_frmts/mitab; $(MAKE))
	(cd ogrsf_frmts/generic; $(MAKE))

endif

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

# The sed substition below workarounds a bug with gcc 4.1 -O2 (checked on 64bit platforms)
# that produces buggy compiled code.
# Seen on gcc 4.1.2-27ubuntu1 (Ubuntu 10.04) (not the default compiler) and gcc-4.1.2-48.el5 (CentOS 5.5)
# (default compiler...)
# The memset isn't necessary at all with a non-buggy compiler, but I've found
# that it helps gcc 4.1 generating correct code here...
parser:
	bison -p swq -d -oswq_parser.cpp swq_parser.y
	sed "s/yytype_int16 yyssa\[YYINITDEPTH\];/yytype_int16 yyssa[YYINITDEPTH]; \/\* workaround bug with gcc 4.1 -O2 \*\/ memset(yyssa, 0, sizeof(yyssa));/" < swq_parser.cpp > swq_parser.cpp.tmp
	mv swq_parser.cpp.tmp swq_parser.cpp

osr_cs_wkt_parser:
	bison --no-lines -p osr_cs_wkt_ -d -oosr_cs_wkt_parser.c osr_cs_wkt_grammar.y
