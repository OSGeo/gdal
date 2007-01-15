
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
XTRA_OPT	=	-Ilibjpeg
else
OBJ	=	jpgdataset.o vsidataio.o
XTRA_OPT	=	
endif

CPPFLAGS	:=	$(XTRA_OPT) $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

../o/%.o:	libjpeg/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

all:	$(OBJ)

install-obj:	$(O_OBJ)
