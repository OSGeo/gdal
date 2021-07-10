
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
        jquant2.o jutils.o jmemmgr.o jmemnobs.o \
	\
	jpgdataset.o vsidataio.o
XTRA_OPT	=	-Ilibjpeg
else
OBJ	=	jpgdataset.o vsidataio.o
XTRA_OPT	=
endif

ifeq ($(JPEG12_ENABLED),yes)
XTRA_OPT_12 	=	-Ilibjpeg12
XTRA_OPT := -DJPEG_DUAL_MODE_8_12 $(XTRA_OPT)
OBJ :=	jcapimin12.o jcapistd12.o jccoefct12.o jccolor12.o jcdctmgr12.o jchuff12.o \
        jcinit12.o jcmainct12.o jcmarker12.o jcmaster12.o jcomapi12.o jcparam12.o \
        jcphuff12.o jcprepct12.o jcsample12.o jctrans12.o jdapimin12.o jdapistd12.o \
        jdatadst12.o jdatasrc12.o jdcoefct12.o jdcolor12.o jddctmgr12.o jdhuff12.o \
        jdinput12.o jdmainct12.o jdmarker12.o jdmaster12.o jdmerge12.o jdphuff12.o \
        jdpostct12.o jdsample12.o jdtrans12.o jerror12.o jfdctflt12.o jfdctfst12.o \
        jfdctint12.o jidctflt12.o jidctfst12.o jidctint12.o jidctred12.o jquant112.o \
        jquant212.o jutils12.o jmemmgr12.o jmemnobs12.o \
        $(OBJ) jpgdataset_12.o vsidataio_12.o
endif

CPPFLAGS	:=	-iquote ../mem $(XTRA_OPT)  $(CPPFLAGS)  -DENABLE_LIBJPEG_NO_RETURN
# # Enable this to compile with -Wextra -Werror and get around the jump complaint.
# CPPFLAGS += -Wno-clobbered

default:	install-obj

$(O_OBJ): jpgdataset.cpp vsidataio.cpp

clean:
	rm -f *.o $(O_OBJ) libjpeg12/*.c libjpeg12/*.h

../o/%.$(OBJ_EXT):	libjpeg/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

../o/%.$(OBJ_EXT):	libjpeg12/%.c
	$(CC) -c $(XTRA_OPT_12) $(CPPFLAGS) $(CFLAGS) $< -o $@

all:	$(OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

import:
	mv libjpeg/jmorecfg.h libjpeg/jmorecfg.h.safe
	mv libjpeg/jconfig.h libjpeg/jconfig.h.safe
	(cd libjpeg; copymatch.sh ~/libjpeg *.c *.h)
	mv libjpeg/jmorecfg.h.safe libjpeg/jmorecfg.h
	mv libjpeg/jconfig.h.safe libjpeg/jconfig.h

libjpeg12/jcapimin12.c:	libjpeg/jcapimin.c
	cp libjpeg/*.h libjpeg12
	cp libjpeg12/jmorecfg.h.12 libjpeg12/jmorecfg.h
	for x in libjpeg/*.c ; do \
	  b=`basename $$x .c`; \
          cp $$x libjpeg12/$${b}12.c; \
        done
