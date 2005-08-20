# This makefile creates renames.i which provides
# mappings from C++ CamelCase names to ruby
# under_score names. To run this makefile:
#
#	make -f RenameMakeile.mk build 
#
# The makefile works by first generating swig bindings.  Based
# on the created bindings, it then generates %rename
# and %alias directives and saves them to a file
# called renames.i that is included by the main
# typemap_ruby.i file.  It then deletes the swig generated
# wrappers so that they can later be regenerated taking
# into account the new %rename and %alias directives.

BINDING = ruby
include ../SWIGmake.base

.PHONY: reset

reset:
	# First empty out the old renames file to get rid of any current mappings
	echo "" > renames.i
	# Now remove any wrapper files that currently exist
	rm -f *.c *.cpp
	
build: reset $(WRAPPERS)
	# Run Ruby code to rename methods and pipe the output to renames.i
	ruby rename_methods.rb --match="OGR(.*)" --replace="\1" > renames.i
	# Remove the swig wrappers
	rm -f *.c *.cpp
