
include ../../GDALmake.opt

ifneq (($KAKDIR),)
ifneq ($(HAVE_LIBTOOL),yes)
include $(GDAL_ROOT)/frmts/jp2kak/jp2kak.lst
endif
endif

OBJ	=	jp2kakdataset.o

KAKINC = -isystem $(KAKDIR)/coresys/common -isystem $(KAKDIR)/apps/compressed_io \
	-isystem $(KAKDIR)/apps/jp2 -isystem $(KAKDIR)/apps/image -isystem $(KAKDIR)/apps/args \
	-isystem $(KAKDIR)/apps/support -isystem $(KAKDIR)/apps/kdu_compress

APPOBJ  = $(KAK_OBJ)

INSTOBJ = $(foreach d,$(APPOBJ),../o/$(notdir $(d)))

#CXXFLAGS :=	$(CXXFLAGS) -DFILEIO_DEBUG

CPPFLAGS	:=	 $(KAKINC) -I. $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
	rm -f $(INSTOBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
	cp $(APPOBJ) ../o

$(OBJ) $(O_OBJ):	subfile_source.h
