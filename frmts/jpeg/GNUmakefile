GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

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
	jpgdataset.o
else
OBJ	=	jpgdataset.o
endif

XTRA_OPT	=	

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $(XTRA_OPT) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) $(XTRA_OPT) $*.cpp -o $*.o


all:	$(OBJ)

install-obj:	$(foreach file,$(OBJ),../o/$(file))

../o/%.o:	%.o
	cp $*.o ../o
