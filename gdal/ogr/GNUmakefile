
include ../GDALmake.opt

include ./file.lst

INST_H_FILES	=	ogr_core.h ogr_feature.h ogr_geometry.h ogr_p.h \
		ogr_spatialref.h ogr_srs_api.h ogrsf_frmts/ogrsf_frmts.h \
		ogr_featurestyle.h ogr_api.h ogr_geocoding.h

ifeq ($(HAVE_GEOS),yes)
CPPFLAGS 	:=	-DHAVE_GEOS=1 $(GEOS_CFLAGS) $(CPPFLAGS)
endif

CXXFLAGS 	:=	-DHAVE_MITAB $(CXXFLAGS)

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

# The sed substitution below workarounds a bug with gcc 4.1 -O2 (checked on 64bit platforms)
# that produces buggy compiled code.
# Seen on gcc 4.1.2-27ubuntu1 (Ubuntu 10.04) (not the default compiler) and gcc-4.1.2-48.el5 (CentOS 5.5)
# (default compiler...)
# The memset isn't necessary at all with a non-buggy compiler, but I've found
# that it helps gcc 4.1 generating correct code here...
parser:
	bison -p swq -d -oswq_parser.cpp swq_parser.y
	sed "s/yytype_int16 yyssa\[YYINITDEPTH\];/yytype_int16 yyssa[YYINITDEPTH]; \/\* workaround bug with gcc 4.1 -O2 \*\/ memset(yyssa, 0, sizeof(yyssa));/" < swq_parser.cpp | sed "s/\*yyssp = yystate/\*yyssp = (yytype_int16)yystate/" > swq_parser.cpp.tmp
	mv swq_parser.cpp.tmp swq_parser.cpp

# The sed hack is to please MSVC
osr_cs_wkt_parser:
	bison --no-lines -p osr_cs_wkt_ -d -oosr_cs_wkt_parser.c osr_cs_wkt_grammar.y
	sed "s/\*yyssp = yystate/\*yyssp = (yytype_int16)yystate/" < osr_cs_wkt_parser.c | sed "s/yyerrorlab:/#if 0\nyyerrorlab:/" | sed "s/yyerrlab1:/#endif\nyyerrlab1:/" | sed "s/for (yylen = 0; yystr\[yylen\]; yylen++)/for (yylen = 0; yystr != YY_NULL \&\& yystr\[yylen\]; yylen++)/"  > osr_cs_wkt_parser.c.tmp
	mv osr_cs_wkt_parser.c.tmp osr_cs_wkt_parser.c

osr_cs_wkt_parser.$(OBJ_EXT):   osr_cs_wkt_parser.c
	$(CC) $(GDAL_INCLUDE) $(CFLAGS) $(CPPFLAGS) $(NO_LOGICAL_OP_FLAG) -c -o $@ $<

swq_parser.$(OBJ_EXT):   swq_parser.cpp
	$(CXX) $(GDAL_INCLUDE) $(CXXFLAGS) $(CPPFLAGS) $(NO_LOGICAL_OP_FLAG) -c -o $@ $<
