
include ../GDALmake.opt

include ./file.lst

INST_H_FILES	=	ogr_core.h ogr_feature.h ogr_geometry.h ogr_p.h \
		ogr_spatialref.h ogr_srs_api.h ogrsf_frmts/ogrsf_frmts.h \
		ogr_featurestyle.h ogr_api.h

ifeq ($(HAVE_GEOS),yes)
CPPFLAGS 	:=	-DHAVE_GEOS=1 $(GEOS_INC) $(CPPFLAGS)
endif
ifeq ($(HAVE_GEOS),c_api)
CPPFLAGS 	:=	-DHAVE_GEOS=1 -DGEOS_C_API=1 $(GEOS_INC) $(CPPFLAGS)
endif

CPPFLAGS	:=	-Iogrsf_frmts -I. $(GDAL_INCLUDE) $(PROJ_INCLUDE) $(PROJ_FLAGS) $(CPPFLAGS)

default:	lib apps

all:	sublibs apps test_ogrsf$(EXE) docs

ifeq ($(OGR_ENABLED),yes)

apps:	ogrinfo$(EXE) ogr2ogr$(EXE) ogrtindex$(EXE)

else

apps:

endif

clean:
	$(RM) *.o ogrinfo$(EXE) ogr2ogr$(EXE) ogrtindex$(EXE) 
	$(RM) sdts2mi$(EXE) testepsg$(EXE)
	$(RM) test_ogrsf$(EXE)
	rm -f html/*
	(cd ogrsf_frmts; $(MAKE) clean)

very-clean: clean
	rm -rf html rtf

ogrinfo$(EXE):	ogrinfo.o  $(EXE_DEP_LIBS)
	$(LD) $(LNK_FLAGS) ogrinfo.o $(CONFIG_LIBS) -o ogrinfo$(EXE)

test_ogrsf$(EXE):	test_ogrsf.o  $(EXE_DEP_LIBS)
	$(LD) $(LNK_FLAGS) test_ogrsf.o $(CONFIG_LIBS) -o test_ogrsf$(EXE)

ogr2ogr$(EXE):	ogr2ogr.o  $(EXE_DEP_LIBS)
	$(LD) $(LNK_FLAGS) ogr2ogr.o $(CONFIG_LIBS) -o ogr2ogr$(EXE)

sdts2mi$(EXE):	sdts2mi.o  $(EXE_DEP_LIBS)
	$(LD) $(LNK_FLAGS) sdts2mi.o $(CONFIG_LIBS) -o sdts2mi$(EXE)

testepsg$(EXE):	testepsg.o  $(EXE_DEP_LIBS)
	$(LD) $(LNK_FLAGS) testepsg.o $(CONFIG_LIBS) -o testepsg$(EXE)

ogrtindex$(EXE):	ogrtindex.o  $(EXE_DEP_LIBS)
	$(LD) $(LNK_FLAGS) ogrtindex.o $(CONFIG_LIBS) -o ogrtindex$(EXE)

lib:	sublibs $(OBJ)

obj:	$(OBJ)

ifeq ($(OGR_ENABLED),yes)

sublibs:
	(cd ogrsf_frmts; $(MAKE))

else

sublibs:

endif

docs:
	rm -rf html/*
	doxygen
	cp ogrsf_frmts/*/drv_*.html html
	cp ogrsf_frmts/ogr_formats.html html
	cp ogr_feature_style.html html

install-docs:
	$(INSTALL_DIR) $(DESTDIR)$(INST_DOCS)/ogr
	cp html/* $(DESTDIR)$(INST_DOCS)/ogr
	cp ../doc/ogr/*.gif $(DESTDIR)$(INST_DOCS)/ogr

gdalso:	$(GDAL_SLIB)

$(GDAL_SLIB):
	(cd ..; $(MAKE) check-lib)

web-update:	docs
	cp html/* /u/www/projects/opengis/ogrhtml
	cp ogrsf_frmts/*/drv_*.html /u/www/projects/opengis/ogrhtml
	cp ogrsf_frmts/ogr_formats.html /u/www/projects/opengis/ogrhtml

test:	reg-test reg-test2

reg-test:
	@rm -f s.out
	@./reg_test.sh > s.out
	@if test "`diff s.out reg_test.log`" = '' ; then \
	    echo "******* Results Match Expected *********"; \
	    grep FAILURE s.out; \
	    rm s.out; \
	else \
	    echo "******* Results Don't Match Expected *********"; \
	    diff s.out reg_test.log; \
	fi

reg-test2:
	@rm -f s2.out
	@./reg_test2.sh > s2.out
	@if test "`diff s2.out reg_test2.log`" = '' ; then \
	    echo "******* Results Match Expected *********"; \
	    grep FAILURE s2.out; \
	    rm s2.out; \
	else \
	    echo "******* Results Don't Match Expected *********"; \
	    diff s2.out reg_test2.log; \
	fi

ifeq ($(OGR_ENABLED),yes)
install-full:	apps
	$(INSTALL) ogr2ogr $(DESTDIR)$(INST_BIN)
	$(INSTALL) ogrinfo $(DESTDIR)$(INST_BIN)
	$(INSTALL) ogrtindex $(DESTDIR)$(INST_BIN)
else
install-full:
endif

install:	install-full
	for f in $(INST_H_FILES) ; \
	    do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_INCLUDE) ; \
	done
