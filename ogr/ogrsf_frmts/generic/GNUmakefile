
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ogrsfdriverregistrar.o ogrlayer.o ogrdatasource.o \
		ogrsfdriver.o ogrregisterall.o

ifeq ($(OGDI_FLAG),ENABLED)
CFLAGS   :=	$(CFLAGS) -DOGDI_ENABLED
CXXFLAGS :=	$(CXXFLAGS) -DOGDI_ENABLED
endif

ifeq ($(HAVE_OGR_PG),yes)
CFLAGS   :=	$(CFLAGS) -DPGENABLED
CXXFLAGS :=	$(CXXFLAGS) -DPG_ENABLED
endif

ALL_C_FLAGS	=	-I.. -I../.. -I$(GDAL_ROOT)/port $(CFLAGS)



default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) -DINST_DATA=\"$(INST_DATA)\" $*.cpp -o $*.o
	ar r ../ogrsf_frmts.a $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) $*.c -o $*.o
	ar r ../ogrsf_frmts.a $*.o

