GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

VERSION =	0.1
DISTDIR =	iso8211lib-$(VERSION)
WEB_DIR =	/usr/local/WebDocuments/projects/iso8211

INCLUDE	=	-I. -I$(GDAL_ROOT)/port

OBJ =	ddfmodule.o ddfutils.o ddffielddefn.o ddfrecord.o ddffield.o \
	ddfsubfielddefn.o

default:	$(OBJ) 8211view

all:	8211dump 8211view testsuite docs

clean:
	rm -rf *.o 8211dump 8211view $(DISTDIR) $(DISTDIR).tar.gz html/*

dist-clean:	clean
	rm -rf $(DISTDIR)

%.o:	%.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDE) $*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $*.c -o $*.o


8211dump:	8211dump.o $(OBJ)
	$(CXX) $(CXX_ODFLAGS) 8211dump.o $(GDAL_ROOT)/port/*.o $(OBJ) $(LIBS) \
		-o 8211dump

8211view:	8211view.o $(OBJ)
	$(CXX) $(CXX_ODFLAGS) 8211view.o $(GDAL_ROOT)/port/*.o $(OBJ) $(LIBS) \
		-o 8211view

timetest:	timetest.o $(OBJ)
	$(CXX) $(CXX_ODFLAGS) timetest.o $(GDAL_ROOT)/port/*.o $(OBJ) $(LIBS) \
		-o timetest

docs:
	rm -rf html
	mkdir html
	doxygen
	rm html/index.html
	cp html/iso8211lib.html html/index.html

dist:	docs
	rm -rf $(DISTDIR)
	mkdir $(DISTDIR)
	mkdir $(DISTDIR)/html
	cp html/* $(DISTDIR)/html
	mkdir $(DISTDIR)/testdata
	cp testdata/*.* $(DISTDIR)/testdata
	cp teststream.sh teststream.out $(DISTDIR)
	autoconf
	cp *.cpp *.h configure Makefile.in $(DISTDIR)
	rm configure
	cp $(GDAL_ROOT)/port/{cpl_error*,cpl_port*,cpl_string*} $(DISTDIR)
	cp $(GDAL_ROOT)/port/{cpl_vsisimple.cpp,cpl_config.h.in} $(DISTDIR)
	cp $(GDAL_ROOT)/port/{cpl_vsi.h,cpl_conv*} $(DISTDIR)
	tar czf $(DISTDIR).tar.gz $(DISTDIR)

update-web:	dist docs
	cp html/* $(WEB_DIR)
	cp $(DISTDIR).tar.gz $(WEB_DIR)
	cp $(DISTDIR).tar.gz /home/ftp/pub/outgoing


test:	8211dump
	@./teststream.sh > t1.out
	@if test "`diff t1.out teststream.out`" = '' ; then \
	    echo "******* Stream 1 Succeeded *********"; \
	    rm t1.out; \
	else \
	    echo "******* Stream 1 Failed *********"; \
	    diff t1.out teststream.out; \
	fi
