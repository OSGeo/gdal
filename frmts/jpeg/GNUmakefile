
include ../../GDALmake.opt

ifeq ($(JPEG_SETTING),internal)
OBJ	=	\
	jcapimin.o jcapistd.o jccoefct.o jccolor.o jcdctmgr.o jchuff.o \
        jcinit.o jcmainct.o jcmarker.o jcmaster.o jcomapi.o jcparam.o \
        jcphuff.o jcprepct.o jcsample.o jctrans.o jdapimin.o jdapistd.o \
        jdatadst.o jdatasrc.o jdcoefct.o jdcolor.o jddctmgr.o jdhuff.o \
        jdinput.o jdmainct.o jdmarker.o jdmaster.o jdmerge.o jdphuff.o \
        jdpostct.o jdsample.o jdtrans.o jerror.o jfdctflt.o jfdctfst.o \
        jfdctint.o jidctflt.o jidctfst.o jidctint.o jidctred.o jquant1.o \
        jquant2.o jutils.o jmemmgr.o jmemansi.o \
	\
	jpgdataset.o vsidataio.o
XTRA_OPT	=	-Ilibjpeg -DDEFAULT_MAX_MEM=500000000L
else
OBJ	=	jpgdataset.o vsidataio.o
XTRA_OPT	=	
endif

ifeq ($(LIBZ_SETTING),internal) 
  XTRA_OPT :=   $(XTRA_OPT) -I../zlib 
endif

CPPFLAGS	:=	$(XTRA_OPT) $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

../o/%.$(OBJ_EXT):	libjpeg/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

all:	$(OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

import:
	mv libjpeg/jmorecfg.h libjpeg/jmorecfg.h.safe
	mv libjpeg/jconfig.h libjpeg/jconfig.h.safe
	(cd libjpeg; copymatch.sh ~/libjpeg *.c *.h)
	mv libjpeg/jmorecfg.h.safe libjpeg/jmorecfg.h
	mv libjpeg/jconfig.h.safe libjpeg/jconfig.h

