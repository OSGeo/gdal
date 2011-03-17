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

RUBY_MODULES_LIN = gdal.so ogr.so gdalconst.so osr.so  # Linux, Solaris, ...
RUBY_MODULES_MAC = gdal.bundle ogr.bundle gdalconst.bundle osr.bundle # Darwin

RUBY_INCLUDE_DIR := $(shell ruby -rrbconfig -e "puts Config::CONFIG['rubyhdrdir'] || Config::CONFIG['archdir']")
RUBY_ARCH_INCLUDE_DIR := $(shell ruby -rrbconfig -e "puts Config::CONFIG['rubyhdrdir'] + '/' + Config::CONFIG['arch'] unless Config::CONFIG['rubyhdrdir'].nil?")
RUBY_LIB_DIR := $(shell ruby -rrbconfig -e "puts Config::CONFIG['libdir']")
RUBY_SO_NAME := $(shell ruby -rrbconfig -e "puts Config::CONFIG['RUBY_SO_NAME']")
RUBY_EXTENSIONS_DIR := $(shell ruby -rrbconfig -e "puts Config::CONFIG['sitearchdir']")
INSTALL_DIR := $(RUBY_EXTENSIONS_DIR)/gdal

ifeq ($(RUBY_ARCH_INCLUDE_DIR),)
# For Ruby < 1.9
RUBY_INCLUDE = -I$(RUBY_INCLUDE_DIR)
else
# For Ruby 1.9
RUBY_INCLUDE = -I$(RUBY_INCLUDE_DIR) -I$(RUBY_ARCH_INCLUDE_DIR)
endif

ifeq ("$(shell uname -s)", "Darwin")
RUBY_MODULES=$(RUBY_MODULES_MAC)
LDFLAGS += -Xcompiler -bundle -L$(RUBY_LIB_DIR)
RUBY_LIB := -l$(RUBY_SO_NAME)
else
RUBY_MODULES=$(RUBY_MODULES_LIN)
LDFLAGS += -Xcompiler -shared -L$(RUBY_LIB_DIR)
RUBY_LIB := -l$(RUBY_SO_NAME)
endif

build: $(RUBY_MODULES)

clean:
	rm -f *.so
	rm -f *.bundle
	rm -f *.o
	rm -f *.lo
	
veryclean: clean
	rm -f *_wrap.cpp

$(INSTALL_DIR):
	mkdir -p $(DESTDIR)$(INSTALL_DIR)

install: $(INSTALL_DIR)
	for i in $(RUBY_MODULES) ; do $(INSTALL) $$i $(DESTDIR)$(INSTALL_DIR) ; done

$(RUBY_MODULES_MAC): %.bundle: %_wrap.o
	$(LD) $(LDFLAGS) $(LIBS) $(GDAL_SLIB_LINK) $(RUBY_LIB) $< -o $@

$(RUBY_MODULES_LIN): %.so: %_wrap.o
	$(LD) $(LDFLAGS) $(LIBS) $(GDAL_SLIB_LINK) $(RUBY_LIB) $< -o $@

%.o: %.cpp
	$(CXX) $(CFLAGS) $(GDAL_INCLUDE) $(RUBY_INCLUDE)  -c $<

%.o: %.cxx
	$(CXX) $(CFLAGS) $(GDAL_INCLUDE) $(RUBY_INCLUDE) -c $<

%.o: %.c
	$(CC) $(CFLAGS) $(GDAL_INCLUDE)  $(RUBY_INCLUDE) -c $<
