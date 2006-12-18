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
#	"get_field_name."  This is done by running the separate
#	RenamesMakefile.mk script.

BINDING = ruby
GDAL_ROOT = ../..
RUBY = ruby

include $(GDAL_ROOT)/GDALmake.opt

RUBY_MODULES = gdal.so ogr.so gdalconst.so osr.so
RUBY_INCLUDE_DIR := $(shell ruby -rrbconfig -e "puts Config::CONFIG['archdir']")
RUBY_LIB_DIR := $(shell ruby -rrbconfig -e "puts Config::CONFIG['libdir']")
RUBY_SO_NAME := $(shell ruby -rrbconfig -e "puts Config::CONFIG['RUBY_SO_NAME']")
RUBY_EXTENSIONS_DIR := $(shell ruby -rrbconfig -e "puts Config::CONFIG['sitearchdir']")
INSTALL_DIR := $(RUBY_EXTENSIONS_DIR)/gdal

RUBY_INCLUDE = -I$(RUBY_INCLUDE_DIR)
LDFLAGS += -shared -L$(RUBY_LIB_DIR)
RUBY_LIB := -l$(RUBY_SO_NAME)

build: $(RUBY_MODULES)

clean:
	rm -f *.so
	rm -f *.o
	rm -f *.lo
	
veryclean: clean
	rm -frd $(INSTALL_DIR)

$(INSTALL_DIR):
	mkdir -v $(DESTDIR)$(INSTALL_DIR)

install: $(INSTALL_DIR)
	$(INSTALL) $(RUBY_MODULES) $(DESTDIR)$(INSTALL_DIR) 

$(RUBY_MODULES): %.so: %_wrap.o
	$(LD) $(LDFLAGS) $(LIBS) $(GDAL_SLIB_LINK) $(RUBY_LIB) $< -o $@

%.o: %.cpp
	$(CXX) $(CFLAGS) $(GDAL_INCLUDE) $(RUBY_INCLUDE)  -c $<

%.o: %.cxx
	$(CXX) $(CFLAGS) $(GDAL_INCLUDE) $(RUBY_INCLUDE) -c $<

%.o: %.c
	$(CC) $(CFLAGS) $(GDAL_INCLUDE)  $(RUBY_INCLUDE) -c $<
