

include ../../GDALmake.opt
include java.opt

all: build

BINDING = java
include ../SWIGmake.base

SHORT_NAME = $(subst _wrap,,$*)
SWIGARGS += -outdir "org/gdal/$(SHORT_NAME)" -package "org.gdal.$(SHORT_NAME)"

EXTRA_DIST = org

.PHONY: makedir
makedir:
	mkdir -p org/gdal/gdal
	mkdir -p org/gdal/gdalconst
	mkdir -p org/gdal/ogr
	mkdir -p org/gdal/osr


JAVA_MODULES = libgdaljni.so libogrjni.so libgdalconstjni.so libosrjni.so

clean:
	-rm -f ${JAVA_MODULES}
	-rm *.o *.lo

veryclean: clean
	-rm -f ${WRAPPERS}
	-rm -rf ${EXTRA_DIST}

generate: makedir ${WRAPPERS}
  
build: generate ${JAVA_MODULES}
	cp ./.libs/*.so ./
	ant

install: generate
	@echo "No installation to be done"


$(JAVA_MODULES): lib%jni.so: %_wrap.o
	$(LD) -shared $(LDFLAGS) $(CONFIG_LIBS) $< -o $@

%.o: %.cpp
	$(CXX) -fPIC $(CFLAGS) $(GDAL_INCLUDE) $(JAVA_INCLUDE) -c $<

%.o: %.cxx
	$(CXX) -fPIC $(CFLAGS) $(GDAL_INCLUDE) $(JAVA_INCLUDE) -c $<

%.o: %.c
	$(CC) -fPIC $(CFLAGS) $(GDAL_INCLUDE) $(JAVA_INCLUDE) -c $<

