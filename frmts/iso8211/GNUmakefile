GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

VERSION =	1.3
DISTDIR =	iso8211lib-$(VERSION)
WEB_DIR =	/u/www/projects/iso8211

CPPFLAGS	:=	-I. -I$(GDAL_ROOT)/port $(CPPFLAGS)

ISOLIB = libiso8211.a
OBJ =	ddfmodule.o ddfutils.o ddffielddefn.o ddfrecord.o ddffield.o \
	ddfsubfielddefn.o

default:	$(ISOLIB)

all:	$(ISOLIB) 8211dump 8211view mkcatalog docs

clean:
	rm -rf *.o 8211dump 8211view $(DISTDIR) $(DISTDIR).tar.gz html/* \
		$(ISOLIB)

dist-clean:	clean
	rm -rf $(DISTDIR)

$(ISOLIB):	$(OBJ)
	$(AR) r $(ISOLIB) $?
	$(RANLIB) $(ISOLIB)

8211dump:	8211dump.o $(ISOLIB)
	$(CXX) $(CXX_ODFLAGS) 8211dump.o \
		$(ISOLIB) $(GDAL_LIB) $(LIBS) -o 8211dump

8211view:	8211view.o $(ISOLIB)
	$(CXX) $(CXX_ODFLAGS) 8211view.o \
		$(ISOLIB) $(GDAL_LIB) $(LIBS) -o 8211view

timetest:	timetest.o $(ISOLIB)
	$(CXX) $(CXX_ODFLAGS) timetest.o \
		$(ISOLIB) $(GDAL_LIB) $(LIBS) -o timetest

upd_test:	upd_test.o $(ISOLIB)
	$(CXX) $(CXX_ODFLAGS) upd_test.o \
		$(ISOLIB) $(GDAL_LIB) $(LIBS) -o upd_test

mkcatalog:	mkcatalog.o $(ISOLIB)
	$(CXX) $(CXX_ODFLAGS) mkcatalog.o \
		$(ISOLIB) $(GDAL_LIB) $(LIBS) -o mkcatalog

docs:
	rm -rf html
	mkdir html
	doxygen
	rm html/index.html
	cp html/ISO8211Lib.html html/index.html

dist:	docs
	rm -rf $(DISTDIR)
	mkdir $(DISTDIR)
	mkdir $(DISTDIR)/html
	cp html/* $(DISTDIR)/html
#	mkdir $(DISTDIR)/testdata
#	cp testdata/*.* $(DISTDIR)/testdata
#	cp teststream.sh teststream.out $(DISTDIR)
	autoconf
	cp *.cpp *.h configure Makefile.in $(DISTDIR)
	rm configure
	cp $(GDAL_ROOT)/port/{cpl_error*,cpl_port*,cpl_string*} $(DISTDIR)
	cp $(GDAL_ROOT)/port/{cpl_vsisimple.cpp,cpl_config.h.in} $(DISTDIR)
	cp $(GDAL_ROOT)/port/{cpl_vsi.h,cpl_conv.*} $(DISTDIR)
	cp $(GDAL_ROOT)/port/cpl_config.h.vc $(DISTDIR)
	rm $(DISTDIR)/*.o
	tar czf $(DISTDIR).tar.gz $(DISTDIR)
	zip -r $(DISTDIR).zip $(DISTDIR)

update-web:	dist docs
	cp html/* $(WEB_DIR)
	cp $(DISTDIR).tar.gz $(DISTDIR).zip $(WEB_DIR)
	cp $(DISTDIR).tar.gz $(DISTDIR).zip /u/ftp/pub/outgoing


test:	8211dump
	@./teststream.sh > t1.out
	@if test "`diff t1.out teststream.out`" = '' ; then \
	    echo "******* Stream 1 Succeeded *********"; \
	    rm t1.out; \
	else \
	    echo "******* Stream 1 Failed *********"; \
	    diff t1.out teststream.out; \
	fi

install-obj:	$(O_OBJ)
