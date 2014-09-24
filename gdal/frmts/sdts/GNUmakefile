VERSION =	1_3
DISTDIR =	sdts_$(VERSION)
WEB_DIR =	/u/www/projects/sdts

SHPDIR		=	../shapelib
ISO8211DIR	=	../iso8211

include ../../GDALmake.opt

OBJ	=	sdtsiref.o sdtscatd.o sdtslinereader.o sdtslib.o \
		sdtspointreader.o sdtsattrreader.o sdtstransfer.o \
		sdtspolygonreader.o sdtsxref.o sdtsrasterreader.o \
		sdtsindexedreader.o

CPPFLAGS :=	-I$(ISO8211DIR) $(GDAL_INCLUDE) $(CPPFLAGS)

SDTSLIB	=	libsdts_al.a
LIBS	:=	$(SDTSLIB) $(ISO8211DIR)/libiso8211.a $(GDAL_LIB) $(LIBS)

default:	$(SDTSLIB) sdtsdataset.$(OBJ_EXT)

$(O_OBJ):       sdts_al.h

all:	$(SDTSLIB) sdts2shp

clean:	clean-dist
	rm -rf *.o sdts2shp html man $(SDTSLIB)

clean-dist:
	rm -rf $(DISTDIR) $(DISTDIR).zip $(DISTDIR).tar.gz

$(SDTSLIB):	$(OBJ:.o=.$(OBJ_EXT))
	ar r $(SDTSLIB) $?

sdts2shp:	sdts2shp.cpp $(SDTSLIB)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -I$(SHPDIR) sdts2shp.cpp \
		$(SHPDIR)/shpopen.$(OBJ_EXT) $(SHPDIR)/dbfopen.$(OBJ_EXT) \
		$(LIBS) -o sdts2shp

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT)) ../o/sdtsdataset.$(OBJ_EXT)

docs:
	rm -rf html
	mkdir html
	doxygen
	rm html/index.html
	cp html/sdts_al_main.html html/index.html

dist:	docs
	rm -rf $(DISTDIR)
	mkdir $(DISTDIR)
	mkdir $(DISTDIR)/html
	cp html/* $(DISTDIR)/html
	autoconf
	cp *.cpp *.h configure Makefile.in $(DISTDIR)
	cp makefile.vc.dist $(DISTDIR)/Makefile.vc
	rm $(DISTDIR)/sdtsdataset.cpp
	cp $(ISO8211DIR)/{*.cpp,*.h} $(DISTDIR)
	rm configure
	cp ../../port/{cpl_error*,cpl_port*,cpl_string*} $(DISTDIR)
	cp ../../port/{cpl_vsisimple.cpp,cpl_config.h.in} $(DISTDIR)
	cp ../../port/{cpl_vsi.h,cpl_conv.*,cpl_path.cpp} $(DISTDIR)
	cp ../../port/cpl_config.h.in $(DISTDIR)/cpl_config.h
	cp $(SHPDIR)/{shpopen.c,dbfopen.c,shapefil.h} $(DISTDIR)
	rm $(DISTDIR)/*.o
	tar czf $(DISTDIR).tar.gz $(DISTDIR)
	zip -r $(DISTDIR).zip $(DISTDIR)

update-web:	dist docs
	cp html/* $(WEB_DIR)
	cp $(DISTDIR).tar.gz $(DISTDIR).zip /u/ftp/pub/outgoing
