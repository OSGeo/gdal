VERSION =	1_0
DISTDIR =	gxf3_$(VERSION)
WEB_DIR =	/u/www/projects/gxf
SHARED_LIB =	gdal_GXF.so

GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

GXFOBJ	=	gxfopen.o gxf_proj4.o gxf_ogcwkt.o
OBJ	=	gxfdataset.o $(GXFOBJ)

CPPFLAGS	=	$(GDAL_INCLUDE)

default:	$(OBJ)

clean:
	rm -f *.o

gxftest:	gxftest.o $(GXFOBJ)
	$(CXX) gxftest.o $(GXFOBJ) $(GDAL_ROOT)/port/*.o $(LIBS) -o gxftest

install-obj:	$(O_OBJ)

$(SHARED_LIB):	$(OBJ)
	$(LD_SHARED) $(OBJ) $(GDAL_SLIB_LINK) $(LIBS) -o $(SHARED_LIB) 

install-lib:	$(SHARED_LIB)
	cp $(SHARED_LIB) $(GDAL_ROOT)/lib

docs:
	rm -rf html
	mkdir html
	doxygen

update-web:	dist
	cp html/* $(WEB_DIR)
	cp $(DISTDIR).tar.gz /u/ftp/pub/outgoing
	cp $(DISTDIR).zip /u/ftp/pub/outgoing


dist:	docs
	rm -rf $(DISTDIR)
	mkdir $(DISTDIR)
	mkdir $(DISTDIR)/html
	cp html/* $(DISTDIR)/html
	autoconf
	cp *.c *.h configure Makefile.in $(DISTDIR)
	cp makefile.vc.dist $(DISTDIR)/makefile.vc
	rm configure
	cp $(GDAL_ROOT)/port/cpl_conv.cpp $(DISTDIR)/cpl_conv.c
	cp $(GDAL_ROOT)/port/cpl_conv.h $(DISTDIR)
	cp $(GDAL_ROOT)/port/cpl_string.cpp $(DISTDIR)/cpl_string.c
	cp $(GDAL_ROOT)/port/cpl_string.h $(DISTDIR)
	cp $(GDAL_ROOT)/port/cpl_vsisimple.cpp $(DISTDIR)/cpl_vsisimple.c
	cp $(GDAL_ROOT)/port/cpl_vsi.h $(DISTDIR)
	cp $(GDAL_ROOT)/port/cpl_error.cpp $(DISTDIR)/cpl_error.c
	cp $(GDAL_ROOT)/port/cpl_error.h $(DISTDIR)
	cp $(GDAL_ROOT)/port/cpl_port.h $(DISTDIR)
	cp $(GDAL_ROOT)/port/cpl_config.h $(DISTDIR)
	cp $(GDAL_ROOT)/port/cpl_config.h.in $(DISTDIR)
	rm -f $(DISTDIR)/*.o
	tar czf $(DISTDIR).tar.gz $(DISTDIR)
	zip -r $(DISTDIR).zip $(DISTDIR)

