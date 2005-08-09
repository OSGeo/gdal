# This makefile generates 4 Ruby extensions - one each for
# gdal, ogr, osr, and gdalconstants.  There are two important
# things to note:
#
# * It makes multiple calls to Ruby to discover the Ruby version,
#	location of header files, libraries, etc.  Thus Ruby must
#	be on your path.
#
# * By convention Ruby method names are lower case with underscores,
#	thus methods such as "GetFieldName" need to be mapped to
#	"get_field_name."  The makefile automatically does this by:
#	
#	1. Calls the RenameMakefile which blanks out the renames.i 
#	   SWIG file.  It then runs SWIG to generate wrapper files.
#	   Then it runs a Ruby script to translate methods names, 
#	   writes %rename directives to renames.i, and then deletes
#	   the wrapper files.
#	2. This makefile then generates the wrappers and builds
#	   the Ruby extensions as shared libraries.


GDAL_ROOT = ../..
RUBY = ruby

include ../../GDALmake.opt

BINDING = ruby
include ../SWIGmake.base

clean:
	-rm -rf gdal

veryclean: clean
	-rm -f $(WRAPPERS)

renames:
	$(MAKE) -f RenameMakefile.mk all

generate: $(WRAPPERS)

build: generate
	$(MAKE) -f RubyMakefile.mk build

install: 
	$(MAKE) -f RubyMakefile.mk install
