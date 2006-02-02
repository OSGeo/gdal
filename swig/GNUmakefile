GDAL_ROOT	=	..

include ../GDALmake.opt

# The BINDINGS variable in GDALmake.opt contains a list of language
# bindings to be built and installed by default.

# The ALL_BINDINGS variable defines all the possible bindings.  It's used
# in the dist and clean definitions.
ALL_BINDINGS = python php csharp ruby perl java

default: generate

# The dist target should generate all the wrapper files for all supported
# language bindings.  This should be called by the top level distribution
# maker script prior to bundling up the source.  It eliminates the installer
# from having the proper version of swig installled.
dist: generate

clean:
	for dir in ${BINDINGS}; do (cd $$dir; make clean ); done

veryclean:
	for dir in ${ALL_BINDINGS}; do (cd $$dir; make veryclean ); done

generate:
	for dir in ${ALL_BINDINGS}; do (cd $$dir; make generate ); done

build:
	for dir in ${BINDINGS}; do (cd $$dir; make build ); done

install:
	for dir in ${BINDINGS}; do (cd $$dir; make install ); done
