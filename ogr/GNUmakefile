GDAL_ROOT	=	..

include ../GDALmake.opt

OBJ	=	ogrgeometryfactory.o ogrpoint.o ogrcurve.o ogrlinestring.o \
		ogrlinearring.o ogrpolygon.o ogrutils.o ogrgeometry.o \
		ogrgeometrycollection.o ogrmultipolygon.o ogrsurface.o \
		ogrmultipoint.o ogrmultilinestring.o ogr_api.o \
		\
		ogrfeature.o ogrfeaturedefn.o ogrfeaturestyle.o ogrfielddefn.o\
		ogrspatialreference.o ogr_srsnode.o ogr_srs_proj4.o \
		ogr_fromepsg.o ogrct.o ogr_opt.o ogr_srs_esri.o \
		ogrfeaturequery.o swq.o ogr_srs_validate.o ogr_srs_xml.o \
		ograssemblepolygon.o ogr2gmlgeometry.o gml2ogrgeometry.o

INST_H_FILES	=	ogr_core.h ogr_feature.h ogr_geometry.h ogr_p.h \
		ogr_spatialref.h ogr_srs_api.h ogrsf_frmts/ogrsf_frmts.h \
		ogr_featurestyle.h ogr_api.h

OGR_LIB  =	ogr.a

FRMTS_LIB = 	ogrsf_frmts/ogrsf_frmts.a

LINCLUDE =	-Iogrsf_frmts -I. -I$(GDAL_ROOT)/port 

default:	sublibs lib apps

all:	sublibs apps test_ogrsf$(EXE) docs

ifeq ($(OGR_ENABLED),yes)

apps:	ogrinfo$(EXE) ogr2ogr$(EXE) ogrtindex$(EXE)

else

apps:

endif

clean:
	rm -f *.o ogrinfo$(EXE) ogr2ogr$(EXE) ogrtindex$(EXE) 
	rm -f sdts2mi$(EXE) testepsg$(EXE)
	rm -f test_ogrsf$(EXE) $(OGR_LIB)
	rm -f html/*
	(cd ogrsf_frmts; $(MAKE) clean)

very-clean: clean
	rm -rf html rtf

ogrinfo$(EXE):	ogrinfo.o  $(EXE_DEP_LIBS)
	$(CXX) $(LNK_FLAGS) ogrinfo.o $(CONFIG_LIBS) -o ogrinfo$(EXE)

test_ogrsf$(EXE):	test_ogrsf.o  $(EXE_DEP_LIBS)
	$(CXX) $(LNK_FLAGS) test_ogrsf.o $(CONFIG_LIBS) -o test_ogrsf$(EXE)

ogr2ogr$(EXE):	ogr2ogr.o  $(EXE_DEP_LIBS)
	$(CXX) $(LNK_FLAGS) ogr2ogr.o $(CONFIG_LIBS) -o ogr2ogr$(EXE)

sdts2mi$(EXE):	sdts2mi.o  $(EXE_DEP_LIBS)
	$(CXX) $(LNK_FLAGS) sdts2mi.o $(CONFIG_LIBS) -o sdts2mi$(EXE)

testepsg$(EXE):	testepsg.o  $(EXE_DEP_LIBS)
	$(CXX) $(LNK_FLAGS) testepsg.o $(CONFIG_LIBS) -o testepsg$(EXE)

ogrtindex$(EXE):	ogrtindex.o  $(EXE_DEP_LIBS)
	$(CXX) $(LNK_FLAGS) ogrtindex.o $(CONFIG_LIBS) -o ogrtindex$(EXE)

$(FRMTS_LIB):
	(cd ogrsf_frmts; $(MAKE))

lib:	$(OGR_LIB) 

ifeq ($(OGR_ENABLED),yes)

sublibs:
	(cd ogrsf_frmts; $(MAKE))

else

sublibs:

endif

docs:
	rm -rf html/*
	doxygen

install-docs:
	$(INSTALL_DIR) $(INST_DOCS)/ogr
	cp html/* $(INST_DOCS)/ogr

$(OGR_LIB):	$(OBJ)
	$(AR) r $(OGR_LIB) $?
	$(RANLIB) $(OGR_LIB)

gdalso:	$(GDAL_SLIB)

$(GDAL_SLIB):	$(GDALA_DEP)
	(cd ..; $(MAKE) check-lib)

%.o:	%.c
	$(CC) -c $(LINCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(LINCLUDE) $(CXXFLAGS) $*.cpp -o $*.o

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
	$(INSTALL) ogr2ogr $(INST_BIN)
	$(INSTALL) ogrinfo $(INST_BIN)
	$(INSTALL) ogrtindex $(INST_BIN)
	$(INSTALL_DATA) $(OGRFRMTS_LIB) $(INST_LIB)
else
install-full:
endif

install:	$(OGR_LIB) install-full
	for f in $(INST_H_FILES) ; \
	    do $(INSTALL_DATA) $$f $(INST_INCLUDE) ; \
	done
	$(INSTALL_DATA) $(OGR_LIB) $(INST_LIB)


