
include ../GDALmake.opt

# The BINDINGS variable in GDALmake.opt contains a list of language
# bindings to be built and installed by default.

# The ALL_BINDINGS variable defines all the possible bindings.  It's used
# in the dist and clean definitions.
ALL_BINDINGS = python php csharp perl java
# ruby

default: generate

# The dist target should generate all the wrapper files for all supported
# language bindings.  This should be called by the top level distribution
# maker script prior to bundling up the source.  It eliminates the installer
# from having the proper version of swig installed.
dist: generate

clean:
	for dir in ${BINDINGS}; do (cd $$dir; $(MAKE) clean) || exit; done

veryclean:
	for dir in ${ALL_BINDINGS}; do (cd $$dir; $(MAKE) veryclean) || exit; done

generate:
	for dir in ${ALL_BINDINGS}; do (cd $$dir; $(MAKE) generate) || exit; done

build:
	for dir in ${BINDINGS}; do (cd $$dir; $(MAKE) build) || exit; done

install:
	for dir in ${BINDINGS}; do (cd $$dir; $(MAKE) install) || exit; done
