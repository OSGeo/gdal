include ../../GDALmake.opt
include java.opt
CFLAGS += -DSWIG_NOARRAYS  # use for Swig 2.0.4 and earlier

all: build

BINDING = java
include ../SWIGmake.base

SHORT_NAME = $(subst _wrap,,$*)
SWIGARGS += -outdir "org/gdal/$(SHORT_NAME)" -package "org.gdal.$(SHORT_NAME)"

# Run "make ANDROID=yes" for Android JNI
ifeq ($(ANDROID),yes)
SWIGARGS += -DSWIGANDROID
endif

EXTRA_DIST = org

LINK = $(LD_SHARED)
LINK_EXTRAFLAGS =
OBJ_EXT = o
ifeq ($(HAVE_LIBTOOL),yes)
LINK = $(LD)
LINK_EXTRAFLAGS = -rpath $(INST_LIB) -no-undefined -version-info $(LIBGDAL_CURRENT):$(LIBGDAL_REVISION):$(LIBGDAL_AGE)
OBJ_EXT = lo
endif

makedir:
	mkdir -p org/gdal/gdal
	mkdir -p org/gdal/gdalconst
	mkdir -p org/gdal/ogr
	mkdir -p org/gdal/osr
	mkdir -p org/gdal/gnm

JAVA_MODULES = libgdalalljni.$(SO_EXT)
JAVA_OBJECTS = gdalconst_wrap.$(OBJ_EXT) gdal_wrap.$(OBJ_EXT) osr_wrap.$(OBJ_EXT) ogr_wrap.$(OBJ_EXT) gnm_wrap.$(OBJ_EXT)

clean:
	-rm -f ${JAVA_MODULES}
	-rm -f *.$(OBJ_EXT)
	-rm -f .libs/*.so
	-rm -f .libs/*.dylib
	-rm -f *.so
	-rm -f *.dylib

	ant clean

veryclean: clean
	-rm -f ${WRAPPERS}
	-rm -rf ${EXTRA_DIST}/*

generate: makedir ${WRAPPERS}

build: generate ${JAVA_OBJECTS} ${JAVA_MODULES}
ifeq ($(HAVE_LIBTOOL),yes)

	if [ -f ".libs/libgdalalljni.so" ] ; then \
		cp .libs/*.so . ; \
	fi

	echo "$(wildcard .libs/*.dylib)"

	if [ -f ".libs/libgdalalljni.dylib" ] ; then \
		cp .libs/*.dylib . ; \
	fi

endif # HAVE_LIBTOOL=yes
	ant

install: build
	if [ -f "libgdalalljni.so" ] ; then \
		for f in *.so; do $(INSTALL_LIB) $$f $(DESTDIR)$(INST_LIB) ; done ; \
	fi

	if [ -f "libgdalalljni.dylib" ] ; then \
		for f in *.dylib; do $(INSTALL_LIB) $$f $(DESTDIR)$(INST_LIB) ; done ; \
	fi

	ant maven ; \
	mkdir -p $(DESTDIR)${datarootdir}/java ; \
	echo $(DESTDIR)${datarootdir}/java ; \
	cp build/maven/* $(DESTDIR)${datarootdir}/java ; \


JAVA_RUN = java -Djava.library.path=. -cp gdal.jar:build/apps

test:
	-rm -rf tmp_test
	mkdir tmp_test
	cp  test_data/byte.tif tmp_test
	${JAVA_RUN} GDALOverviews tmp_test/byte.tif "NEAREST" 2 4
	${JAVA_RUN} gdalinfo -checksum tmp_test/byte.tif
	${JAVA_RUN} ogr2ogr tmp_test/out.shp test_data/poly.shp -progress -overwrite
	${JAVA_RUN} ogrinfo -ro -al tmp_test/out.shp
	${JAVA_RUN} ogrinfo -ro -al tmp_test/out.shp -fid 1
	${JAVA_RUN} ogr2ogr_new tmp_test/out2.shp test_data/poly.shp -progress -overwrite
	${JAVA_RUN} ogr2ogr_new tmp_test/out2.shp test_data/poly.shp -append
	${JAVA_RUN} ogrinfo -ro -al tmp_test/out2.shp
	${JAVA_RUN} OSRTransform
	${JAVA_RUN} gdalmajorobject
	${JAVA_RUN} GDALTestIO
	${JAVA_RUN} GDALContour -i 1 tmp_test/byte.tif tmp_test/contour.shp
	${JAVA_RUN} testgetpoints
	${JAVA_RUN} ogrtindex tmp_test/contour_index.shp tmp_test/contour.shp
	${JAVA_RUN} OSRTest

ifdef INCLUDE_GDAL_LIB
    LIB_GDAL=$(GDAL_LIB)
endif

$(JAVA_MODULES): $(JAVA_OBJECTS)
	$(LINK) $(LDFLAGS) $^ $(LIB_GDAL) $(CONFIG_LIBS) -o $@ $(LINK_EXTRAFLAGS)

# Do not remove -fno-strict-aliasing while SWIG generates weird code in upcast methods
# See http://trac.osgeo.org/gdal/changeset/16006
%.$(OBJ_EXT): %.cpp
	$(CXX) -fno-strict-aliasing $(GDAL_INCLUDE) $(CFLAGS) $(CPPFLAGS) $(JAVA_INCLUDE) -c $<

%.$(OBJ_EXT): %.cxx
	$(CXX) -fno-strict-aliasing $(GDAL_INCLUDE) $(CFLAGS) $(CPPFLAGS) $(JAVA_INCLUDE) -c $<

%.$(OBJ_EXT): %.c
	$(CC) -fno-strict-aliasing $(GDAL_INCLUDE) $(CFLAGS) $(CPPFLAGS) $(JAVA_INCLUDE) -c $<

.PHONY: all makedir clean veryclean generate build install test
