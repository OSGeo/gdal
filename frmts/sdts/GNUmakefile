VERSION =	1.0
DISTDIR =	sdts-$(VERSION)
WEB_DIR =	/u/www/projects/sdts

SHPDIR		=	../shapelib
ISO8211DIR	=	../iso8211
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	sdtsiref.o sdtscatd.o sdtslinereader.o sdtslib.o \
		sdtspointreader.o sdtsattrreader.o sdtstransfer.o \
		sdtspolygonreader.o sdtsxref.o sdtsrasterreader.o \
		sdtsindexedreader.o

ALL_C_FLAGS =	-I$(ISO8211DIR) $(GDAL_INCLUDE) $(CFLAGS)
ALL_CXX_FLAGS =	-I$(ISO8211DIR) $(GDAL_INCLUDE) $(CXXFLAGS)
SDTSLIB	=	libsdts_al.a
LIBS	:=	$(SDTSLIB) $(ISO8211DIR)/libiso8211.a $(CPL_LIB) $(LIBS)



default:	$(SDTSLIB) sdtsdataset.o

all:	sdts2ascii sdts2shp

clean:
	rm -rf *.o sdts2ascii 8211dump $(DISTDIR) html man $(DISTDIR).tar.gz

%.o:	%.cpp
	$(CXX) -c $(ALL_CXX_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.c -o $*.o

$(SDTSLIB):	$(OBJ)
	ar r $(SDTSLIB) $?

sdts2ascii:	sdts2ascii.o $(SDTSLIB)
	$(CXX) sdts2ascii.o $(LIBS) -o sdts2ascii

sdtsinfo:	sdtsinfo.o $(SDTSLIB)
	$(CXX) sdtsinfo.o $(LIBS) -o sdtsinfo

sdts2shp:	sdts2shp.cpp $(SDTSLIB)
	$(CXX) $(ALL_C_FLAGS) -I$(SHPDIR) sdts2shp.cpp \
		$(SHPDIR)/shpopen.o $(SHPDIR)/dbfopen.o \
		$(LIBS) -o sdts2shp

install-obj:	$(foreach file,$(OBJ),../o/$(file)) ../o/sdtsdataset.o

../o/%.o:	%.o
	cp $*.o ../o

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
	cp $(ISO8211DIR)/{*.cpp,*.h} $(DISTDIR)
	rm configure
	cp $(GDAL_ROOT)/port/{cpl_error*,cpl_port*,cpl_string*} $(DISTDIR)
	cp $(GDAL_ROOT)/port/{cpl_vsisimple.cpp,cpl_config.h.in} $(DISTDIR)
	cp $(GDAL_ROOT)/port/{cpl_vsi.h,cpl_conv.*} $(DISTDIR)
	cp $(SHPDIR)/{shpopen.c,dbfopen.c,shapefil.h} $(DISTDIR)
	rm $(DISTDIR)/*.o
	tar czf $(DISTDIR).tar.gz $(DISTDIR)

update-web:	dist docs
	cp html/* $(WEB_DIR)
	cp $(DISTDIR).tar.gz $(WEB_DIR)
	cp $(DISTDIR).tar.gz /u/ftp/pub/outgoing
