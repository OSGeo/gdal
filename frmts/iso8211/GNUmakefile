
include ../../GDALmake.opt

VERSION =	1.4
DISTDIR =	iso8211lib-$(VERSION)
WEB_DIR =	/u/www/projects/iso8211

CPPFLAGS	:=	-I. -I../../port $(CPPFLAGS)

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

dist:	docs
	rm -rf $(DISTDIR)
	mkdir $(DISTDIR)
	mkdir $(DISTDIR)/html
	cp html/* $(DISTDIR)/html
	autoconf
	cp *.cpp *.h configure Makefile.in $(DISTDIR)
	rm configure
	cp ../../port/{cpl_error{.h,.cpp},cpl_port.h,cpl_string.{h,cpp}} $(DISTDIR)
	cp ../../port/{cpl_vsisimple.cpp,cpl_config.h.in} $(DISTDIR)
	cp ../../port/{cpl_multiproc.cpp,cpl_multiproc.h} $(DISTDIR)
	cp ../../port/{cpl_vsi.h,cpl_conv.{cpp,h},cpl_path.cpp} $(DISTDIR)
	cp ../../port/cpl_{vsil.cpp,vsi_mem.cpp,vsil_win32.cpp} $(DISTDIR)
	cp ../../port/cpl_{vsil_unix_stdio_64.cpp,dir.cpp} $(DISTDIR)
	cp ../../port/cpl_{multiproc.{cpp,h},vsi_private.h} $(DISTDIR)
	cp ../../port/cpl_config.h.vc $(DISTDIR)
	tar czf $(DISTDIR).tar.gz $(DISTDIR)
	zip -r $(DISTDIR).zip $(DISTDIR)

update-web:	dist docs
	cp html/* $(WEB_DIR)
	cp $(DISTDIR).tar.gz $(DISTDIR).zip $(WEB_DIR)


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
