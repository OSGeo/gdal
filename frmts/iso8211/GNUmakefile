GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

VERSION =	0.1
DISTDIR =	iso8211lib-$(VERSION)
WEB_DIR =	/usr/local/WebDocuments/projects/iso8211

INCLUDE	=	-I. -I$(GDAL_ROOT)/port

OBJ =	ddfmodule.o ddfutils.o ddffielddefn.o ddfrecord.o ddffield.o \
	ddfsubfielddefn.o

default:	$(OBJ) 8211dump

all:	8211dump docs

clean:
	rm -rf *.o 8211dump 8211view $(DISTDIR) $(DISTDIR).tar.gz html/*

%.o:	%.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDE) $*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $*.c -o $*.o


8211dump:	8211dump.o $(OBJ)
	$(CXX) $(CXX_ODFLAGS) 8211dump.o $(GDAL_ROOT)/port/*.o $(OBJ) $(LIBS) -o 8211dump

8211view:	8211view.o $(OBJ)
	$(CXX) $(CXX_ODFLAGS) 8211view.o $(GDAL_ROOT)/port/*.o $(OBJ) $(LIBS) -o 8211view

timetest:	timetest.o $(OBJ)
	$(CXX) $(CXX_ODFLAGS) timetest.o $(GDAL_ROOT)/port/*.o $(OBJ) $(LIBS) -o timetest

docs:
	doxygen
	rm html/index.html
	cp html/iso8211lib.html html/index.html

configure:	configure.in
	autoconf

dist:	docs configure
	rm -rf $(DISTDIR)
	mkdir $(DISTDIR)
	mkdir $(DISTDIR)/html
	cp html/* $(DISTDIR)/html
	cp *.cpp *.h configure Makefile.in $(DISTDIR)
	cp $(GDAL_ROOT)/port/{cpl_error*,cpl_port*,cpl_string*} $(DISTDIR)
	cp $(GDAL_ROOT)/port/{cpl_vsisimple.cpp,cpl_config.h.in} $(DISTDIR)
	cp $(GDAL_ROOT)/port/{cpl_vsi.h,cpl_conv*} $(DISTDIR)
	tar czf $(DISTDIR).tar.gz $(DISTDIR)

update-web:	dist docs
	cp html/* $(WEB_DIR)
	cp $(DISTDIR).tar.gz $(WEB_DIR)
	cp $(DISTDIR).tar.gz /home/ftp/pub/outgoing
