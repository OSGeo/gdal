
include ../../GDALmake.opt

ifneq (($KAKDIR),)
ifneq ($(HAVE_LIBTOOL),yes)
include $(GDAL_ROOT)/frmts/jp2kak/jp2kak.lst
endif
endif

OBJ	=	jp2kakdataset.o

KAKINC = -I$(KAKDIR)/coresys/common -I$(KAKDIR)/apps/compressed_io \
	-I$(KAKDIR)/apps/jp2 -I$(KAKDIR)/apps/image -I$(KAKDIR)/apps/args \
	-I$(KAKDIR)/apps/support -I$(KAKDIR)/apps/kdu_compress

APPOBJ  = $(KAK_OBJ)

INSTOBJ = $(foreach d,$(APPOBJ),../o/$(notdir $(d)))

#CXXFLAGS :=	$(CXXFLAGS) -DFILEIO_DEBUG

CPPFLAGS	:=	 $(KAKINC) -iquote . $(CPPFLAGS) -DKDU_X86_INTRINSICS

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
	rm -f $(INSTOBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
	cp $(APPOBJ) ../o

$(OBJ) $(O_OBJ): jp2kakdataset.h subfile_source.h
