GDAL_ROOT	=	..

include ../GDALmake.opt

OBJ	=	ogrgeometryfactory.o ogrpoint.o ogrcurve.o ogrlinestring.o \
		ogrlinearring.o ogrpolygon.o ogrutils.o ogrgeometry.o \
		ogrgeometrycollection.o ogrmultipolygon.o ogrsurface.o \
		ogrmultipoint.o ogrmultilinestring.o \
		\
		ogrfeature.o ogrfeaturedefn.o ogrfielddefn.o \
		ogrspatialreference.o ogr_srsnode.o ogr_srs_proj4.o \
		ogr_fromepsg.o ogrct.o

INST_H_FILES	=	ogr_core.h ogr_feature.h ogr_geometry.h ogr_p.h \
		ogr_spatialref.h ogr_srs_api.h ogrsf_frmts/ogrsf_frmts.h

OGR_LIB  =	ogr.a

LIBS	:=	$(OGR_LIB) $(CPL_LIB) $(LIBS) 

FRMTS_LIB = 	ogrsf_frmts/ogrsf_frmts.a

LINCLUDE =	-Iogrsf_frmts -I. -I$(GDAL_ROOT)/port 

default:	$(OGR_LIB)

all:	sublibs test1 test2 test_ogrsf ogrinfo ogr2ogr docs

clean:
	rm -f *.o test1 test2 
	(cd ogrsf_frmts; $(MAKE) clean)

very-clean: clean
	rm -rf html

test1:	test1.o $(OGR_LIB)
	$(CXX) $(LNK_FLAGS) test1.o $(LIBS) -o test1

test2:	test2.o $(OGR_LIB) $(FRMTS_LIB)
	$(CXX) $(LNK_FLAGS) test2.o $(FRMTS_LIB) $(LIBS) -o test2

test_ogrsf:	test_ogrsf.o $(OGR_LIB) $(FRMTS_LIB)
	$(CXX) $(LNK_FLAGS) test_ogrsf.o $(FRMTS_LIB) $(LIBS) -o test_ogrsf

ogrinfo:	ogrinfo.o $(OGR_LIB) $(FRMTS_LIB)
	$(CXX) $(LNK_FLAGS) ogrinfo.o $(FRMTS_LIB) $(LIBS) -o ogrinfo

ogr2ogr:	ogr2ogr.o $(OGR_LIB) $(FRMTS_LIB)
	$(CXX) $(LNK_FLAGS) ogr2ogr.o $(FRMTS_LIB) $(LIBS) -o ogr2ogr

sdts2mi:	sdts2mi.o $(OGR_LIB) $(FRMTS_LIB)
	$(CXX) $(LNK_FLAGS) sdts2mi.o $(FRMTS_LIB) $(LIBS) -o sdts2mi

testepsg:	testepsg.o $(OGR_LIB) $(FRMTS_LIB)
	$(CXX) $(LNK_FLAGS) testepsg.o $(FRMTS_LIB) $(LIBS) -o testepsg

$(FRMTS_LIB):
	(cd ogrsf_frmts; $(MAKE))

lib:	$(OGR_LIB) 

sublibs:
	(cd ogrsf_frmts; $(MAKE))

docs:
	rm -rf html/*
	doxygen


$(OGR_LIB):	$(OBJ)
	ar r $(OGR_LIB) $?

%.o:	%.c
	$(CC) -c $(LINCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(LINCLUDE) $(CXXFLAGS) $*.cpp -o $*.o

web-update:	docs
	cp html/* /u/www/projects/opengis/ogrhtml

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

install:	$(OGR_LIB) ogrinfo ogr2ogr
	cp $(INST_H_FILES) $(INST_INCLUDE)
	cp $(OGR_LIB) $(INST_LIB)
	cp ogrinfo ogr2ogr $(INST_BIN)


