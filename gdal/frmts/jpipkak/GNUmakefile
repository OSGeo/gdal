
include ../../GDALmake.opt

OBJ	=	jpipkakdataset.o kdu_cache.o

KAKINC = -I$(KAKDIR)/coresys/common -I$(KAKDIR)/apps/compressed_io \
	-I$(KAKDIR)/apps/jp2 -I$(KAKDIR)/apps/image -I$(KAKDIR)/apps/args \
	-I$(KAKDIR)/apps/support -I$(KAKDIR)/apps/kdu_compress \
	-I$(KAKDIR)/apps/caching_sources

APPOBJ  = $(KAK_OBJ)

INSTOBJ = $(foreach d,$(APPOBJ),../o/$(notdir $(d)))

#CXXFLAGS :=	$(CXXFLAGS) -DFILEIO_DEBUG

CPPFLAGS	:=	 $(KAKINC) -I. $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

# kdu_cache.cpp does not appear to be built by default by the Kakadu 
# apps makefiles, so we copy it and build it ourselves.  Perhaps this 
# won't be needed with newer versions of Kakadu.  Tested with 6.2.
kdu_cache.cpp:	  $(KAKDIR)/apps/caching_sources/kdu_cache.cpp
	echo "#include \"cpl_port.h\"" > kdu_cache_wrapper.h
	echo "#ifdef HAVE_GCC_SYSTEM_HEADER" >> kdu_cache_wrapper.h
	echo "#pragma GCC system_header" >> kdu_cache_wrapper.h
	echo "#endif" >> kdu_cache_wrapper.h
	cat $(KAKDIR)/apps/caching_sources/kdu_cache.cpp >> kdu_cache_wrapper.h
	echo "#include \"kdu_cache_wrapper.h\"" > kdu_cache.cpp

clean:
	rm -f *.o $(O_OBJ)
	rm -f $(INSTOBJ)
	rm -f kdu_cache.cpp
	rm -f kdu_cache_wrapper.h

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))


