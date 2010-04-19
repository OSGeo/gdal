
include GDALmake.opt

GDAL_OBJ	=	$(GDAL_ROOT)/frmts/o/*.o \
			$(GDAL_ROOT)/gcore/*.o \
			$(GDAL_ROOT)/port/*.o \
			$(GDAL_ROOT)/alg/*.o

ifeq ($(OGR_ENABLED),yes)
GDAL_OBJ += $(GDAL_ROOT)/ogr/ogrsf_frmts/o/*.o
endif

include ./ogr/file.lst
GDAL_OBJ += $(addprefix ./ogr/,$(OBJ))

LIBGDAL-yes	:=	$(GDAL_LIB)
LIBGDAL-$(HAVE_LD_SHARED)	+=	$(GDAL_SLIB)
# override if we are using libtool
LIBGDAL-$(HAVE_LIBTOOL)	:= $(LIBGDAL)

default:	lib-target apps-target swig-target

lib-target:	check-lib;

force-lib:
	$(AR) r $(GDAL_LIB) $(GDAL_OBJ)
	$(RANLIB) $(GDAL_LIB)
	$(LD_SHARED) $(GDAL_SLIB_SONAME) $(GDAL_OBJ) $(GDAL_LIBS) $(LDFLAGS) $(LIBS) \
		-o $(GDAL_SLIB)

$(GDAL_LIB):	$(GDAL_OBJ) GDALmake.opt
	rm -f libgdal.a
	$(AR) r $(GDAL_LIB) $(GDAL_OBJ)
	$(RANLIB) $(GDAL_LIB)

$(GDAL_SLIB):	$(GDAL_OBJ) $(GDAL_LIB)
	$(LD_SHARED) $(GDAL_SLIB_SONAME) $(GDAL_OBJ) $(GDAL_LIBS) $(LDFLAGS) $(LIBS) \
		-o $(GDAL_SLIB)

$(LIBGDAL):	$(GDAL_OBJ:.o=.lo)
	$(LD) $(LDFLAGS) $(LIBS) -o $@ $(GDAL_OBJ:.o=.lo) \
	    -rpath $(INST_LIB) \
	    -no-undefined \
	    -version-info $(LIBGDAL_CURRENT):$(LIBGDAL_REVISION):$(LIBGDAL_AGE)
ifeq ($(MACOSX_FRAMEWORK),yes)
	install_name_tool -id ${OSX_VERSION_FRAMEWORK_PREFIX}/GDAL .libs/libgdal.dylib
endif

check-lib:	port-target core-target frmts-target ogr-target
	$(MAKE) $(LIBGDAL-yes)

port-target:
	(cd port; $(MAKE))

ogr-target:
	(cd ogr; $(MAKE) lib )

core-target:
	(cd gcore; $(MAKE))
	(cd alg; $(MAKE))

frmts-target:
	(cd frmts; $(MAKE))

ogr-all:
	(cd ogr; $(MAKE) all)

apps-target:	lib-target
	(cd apps; $(MAKE))

ifeq ($(BINDINGS),)
swig-target: ;
else
swig-target:    swig-modules;
endif

swig-modules:
	(cd swig; $(MAKE) build)

clean:	lclean
	(cd port; $(MAKE) clean)
	(cd ogr; $(MAKE) clean)
	(cd gcore; $(MAKE) clean)
	(cd frmts; $(MAKE) clean)
	(cd alg; $(MAKE) clean)
	(cd apps; $(MAKE) clean)
ifneq ($(BINDINGS),)
	(cd swig; $(MAKE) clean)
endif



lclean:
	rm -f *.a *.so config.log config.cache html/*.*
	$(RM) *.la

distclean:	dist-clean

dist-clean:	clean
	rm -f GDALmake.opt port/cpl_config.h config.cache config.status
	rm -f libtool
	rm -rf autom4te.cache

config:	configure
	./configure

configure:	configure.in aclocal.m4
	autoconf

GDALmake.opt:	GDALmake.opt.in config.status
	./config.status

docs:
	(cd ogr; $(MAKE) docs)
	(cd html; rm -f *.*)
# Generate translated docs. Should go first, because index.html page should
# be overwritten with the main one later
	doxygen -w html html/header.html html/footer.html html/stylesheet.css
	sed -e 's,iso-8859-1,utf-8,g' html/header.html > html/header_ru.html
	cp html/header.html html/header_br.html
	(cat Doxyfile ; echo "HTML_HEADER=html/header_ru.html"; echo "INPUT=doc/ru"; echo "OUTPUT_LANGUAGE=Russian") | doxygen -
	(cd doc/br ; doxygen ; cp html/*.* ../../html/. )
# Generate HTML docs
	doxygen Doxyfile
	cp data/gdalicon.png html
	cp doc/images/*.* html
	cp doc/grid/*.png html
	cp frmts/*.html frmts/*/frmt_*.html html
	cp frmts/wms/frmt_wms_*.xml html

man:
# Generate man pages
	(cat Doxyfile ; echo "ENABLED_SECTIONS=man"; echo "INPUT=apps swig/python/scripts"; echo "FILE_PATTERNS=*.cpp *.dox"; echo "GENERATE_HTML=NO"; echo "GENERATE_MAN=YES") | doxygen -

all:	default ogr-all

install-docs:
	(cd ogr; $(MAKE) install-docs)
	$(INSTALL_DIR) $(DESTDIR)$(INST_DOCS)/gdal
	cp html/*.* $(DESTDIR)$(INST_DOCS)/gdal

install-man:
	$(INSTALL_DIR) $(DESTDIR)$(INST_MAN)/man1
	for f in $(wildcard man/man1/*.1) ; do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_MAN)/man1 ; done

web-update:	docs
	cp html/*.* $(INST_HTML)
	(cd ogr; make web-update)

install:	default install-actions

install-actions: install-lib
	$(INSTALL_DIR) $(DESTDIR)$(INST_BIN)
	$(INSTALL_DIR) $(DESTDIR)$(INST_DATA)
	$(INSTALL_DIR) $(DESTDIR)$(INST_INCLUDE)
ifeq ($(MACOSX_FRAMEWORK),yes)
	$(INSTALL_DIR) $(DESTDIR)${OSX_VERSION_FRAMEWORK_PREFIX}/PlugIns
	$(INSTALL_DIR) $(DESTDIR)${OSX_VERSION_FRAMEWORK_PREFIX}/unix/lib
	ln -sfh Versions/Current/unix $(DESTDIR)${OSX_FRAMEWORK_PREFIX}/unix
	ln -sfh ../Programs $(DESTDIR)${OSX_VERSION_FRAMEWORK_PREFIX}/unix/bin
	ln -sfh ../Headers $(DESTDIR)${OSX_VERSION_FRAMEWORK_PREFIX}/unix/include
	ln -sf ../../GDAL $(DESTDIR)${OSX_VERSION_FRAMEWORK_PREFIX}/unix/lib/libgdal.dylib
	ln -sfh $(GDAL_VERSION_MAJOR).$(GDAL_VERSION_MINOR) $(DESTDIR)${OSX_FRAMEWORK_PREFIX}/Versions/Current
	ln -sfh Versions/Current/Headers $(DESTDIR)${OSX_FRAMEWORK_PREFIX}/Headers
	ln -sfh Versions/Current/Programs $(DESTDIR)${OSX_FRAMEWORK_PREFIX}/Programs
	ln -sfh Versions/Current/Resources $(DESTDIR)${OSX_FRAMEWORK_PREFIX}/Resources
	ln -sfh Versions/Current/GDAL $(DESTDIR)${OSX_FRAMEWORK_PREFIX}/GDAL
endif
	(cd port; $(MAKE) install)
	(cd gcore; $(MAKE) install)
	(cd frmts; $(MAKE) install)
	(cd alg; $(MAKE) install)
	(cd ogr; $(MAKE) install)
	(cd apps; $(MAKE) install)
ifneq ($(BINDINGS),)
	(cd swig; $(MAKE) install)
endif
	for f in LICENSE.TXT data/*.* ; do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_DATA) ; done
	$(LIBTOOL_FINISH) $(INST_LIB)


ifeq ($(HAVE_LIBTOOL),yes)

install-lib:
	$(INSTALL_DIR) $(DESTDIR)$(INST_LIB)
	for f in $(LIBGDAL-yes) ; do $(INSTALL_LIB) $$f $(DESTDIR)$(INST_LIB) ; done
ifeq ($(MACOSX_FRAMEWORK),yes)
	if [ -f "$(DESTDIR)$(INST_LIB)/libgdal.$(GDAL_VERSION_MAJOR).$(LIBGDAL_AGE).$(LIBGDAL_REVISION).dylib" ] ; then mv -f $(DESTDIR)$(INST_LIB)/libgdal.$(GDAL_VERSION_MAJOR).$(LIBGDAL_AGE).$(LIBGDAL_REVISION).dylib $(DESTDIR)${OSX_VERSION_FRAMEWORK_PREFIX}/GDAL ; fi
	if [ -f "$(DESTDIR)$(INST_LIB)/libgdal.$(GDAL_VERSION_MAJOR).$(LIBGDAL_AGE).dylib" ] ; then mv -f $(DESTDIR)$(INST_LIB)/libgdal.$(GDAL_VERSION_MAJOR).$(LIBGDAL_AGE).dylib $(DESTDIR)${OSX_VERSION_FRAMEWORK_PREFIX}/GDAL ; fi
	if [ -f "$(DESTDIR)$(INST_LIB)/libgdal.$(GDAL_VERSION_MAJOR).dylib" ] ; then mv -f $(DESTDIR)$(INST_LIB)/libgdal.$(GDAL_VERSION_MAJOR).dylib $(DESTDIR)${OSX_VERSION_FRAMEWORK_PREFIX}/GDAL ; fi
	if [ -f "$(DESTDIR)$(INST_LIB)/libgdal.dylib" ] ; then mv -f $(DESTDIR)$(INST_LIB)/libgdal.dylib $(DESTDIR)${OSX_VERSION_FRAMEWORK_PREFIX}/GDAL ; fi
	rm -f $(DESTDIR)$(INST_LIB)/libgdal.$(GDAL_VERSION_MAJOR).$(LIBGDAL_AGE).$(LIBGDAL_REVISION).dylib
	rm -f $(DESTDIR)$(INST_LIB)/libgdal.$(GDAL_VERSION_MAJOR).$(LIBGDAL_AGE).dylib
	rm -f $(DESTDIR)$(INST_LIB)/libgdal.$(GDAL_VERSION_MAJOR).dylib
	rm -f $(DESTDIR)$(INST_LIB)/libgdal.dylib
	rm -f $(DESTDIR)$(INST_LIB)/libgdal.la
endif

else

ifeq ($(HAVE_LD_SHARED),yes)

GDAL_SLIB_B	=	$(notdir $(GDAL_SLIB))

install-lib:

	$(INSTALL_DIR) $(DESTDIR)$(INST_LIB)
ifeq ($(MACOSX_FRAMEWORK),yes)
	$(INSTALL_LIB) $(GDAL_SLIB) $(DESTDIR)$(INST_LIB)/GDAL
else
	rm -f $(DESTDIR)$(INST_LIB)/$(GDAL_SLIB_B)
	rm -f $(DESTDIR)$(INST_LIB)/$(GDAL_SLIB_B).$(GDAL_VERSION_MAJOR)
	rm -f $(DESTDIR)$(INST_LIB)/$(GDAL_SLIB_B).$(GDAL_VER)
	$(INSTALL_LIB) $(GDAL_SLIB) $(DESTDIR)$(INST_LIB)/$(GDAL_SLIB_B).$(GDAL_VER)
	(cd $(DESTDIR)$(INST_LIB) ; \
	 ln -s $(GDAL_SLIB_B).$(GDAL_VER) $(GDAL_SLIB_B).$(GDAL_VERSION_MAJOR))
	(cd $(DESTDIR)$(INST_LIB) ; \
	 ln -s $(GDAL_SLIB_B).$(GDAL_VERSION_MAJOR) $(GDAL_SLIB_B))
endif

else

install-lib:
	$(INSTALL_DIR) $(DESTDIR)$(INST_LIB)
	$(INSTALL_LIB) $(GDAL_LIB) $(DESTDIR)$(INST_LIB)

endif # HAVE_LD_SHARED=no 


endif # HAVE_LIBTOOL=no 

