
GDAL_ROOT = ../..

include $(GDAL_ROOT)/GDALmake.opt

all: build

BINDING = ruby
include ../SWIGmake.base

clean:
	-rm -rf gdal

veryclean: clean
	-rm -f $(WRAPPERS)

generate: ${WRAPPERS}

build: generate
	$(MAKE) -f RubyMakefile.mk build

install: 
	$(MAKE) -f RubyMakefile.mk install
