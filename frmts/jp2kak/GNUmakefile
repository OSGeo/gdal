GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	jp2kakdataset.o

KAKINC = -I$(KAKDIR)/coresys/common -I$(KAKDIR)/apps/compressed_io \
	-I$(KAKDIR)/apps/jp2 -I$(KAKDIR)/apps/image -I$(KAKDIR)/apps/args 

APPOBJ  = $(KAKDIR)/apps/make/args.o $(KAKDIR)/apps/make/image_in.o \
	$(KAKDIR)/apps/make/image_out.o $(KAKDIR)/apps/make/jp2.o \
	$(KAKDIR)/apps/make/mj2.o $(KAKDIR)/apps/make/palette.o \
	$(KAKDIR)/apps/make/roi_sources.o 

#CXXFLAGS :=	$(CXXFLAGS) -DFILEIO_DEBUG

CPPFLAGS	:=	$(GDAL_INCLUDE) $(KAKINC) -I. $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)
	cp $(APPOBJ) ../o

$(OBJ) $(O_OBJ):	jp2kak_roi.h subfile_source.h dbg_file_source.h
