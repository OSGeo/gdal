
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

CORE_OBJ =	gmlpropertydefn.o gmlfeatureclass.o gmlfeature.o gmlreader.o

OGR_OBJ =	ogrgmldriver.o ogrgmldatasource.o ogrgmllayer.o

OBJ =	$(CORE_OBJ) $(OGR_OBJ)

ALL_C_FLAGS	=	$(XERCES_INCLUDE) -I.. -I../.. -I$(GDAL_ROOT)/port \
	$(filter-out -Wall,$(CFLAGS))

ifeq ($(HAVE_XERCES),yes)
CORE_OBJ :=	$(CORE_OBJ) gmlreadstate.o gmlhandler.o trstring.o
ALL_C_FLAGS :=  -DHAVE_XERCES=1 $(ALL_C_FLAGS)
else
ALL_C_FLAGS :=  -DHAVE_XERCES=0 $(ALL_C_FLAGS)
endif

# By default, XML validation is disabled.  Uncomment the following line to
# enable XML schema validation in the parser.
#ALL_C_FLAGS :=  -DOGR_GML_VALIDATION=1 $(ALL_C_FLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o gmlview

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) $*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) $*.c -o $*.o

gmlview:	gmlview.cpp $(CORE_OBJ)
	$(CXX) $(ALL_C_FLAGS) $(LNK_FLAGS) \
		gmlview.cpp $(CORE_OBJ) \
		../ogrsf_frmts.a ../../ogr.a $(CPL_LIB) $(LIBS) \
		-o gmlview


xtest:	xtest.cpp
	$(CXX) $(ALL_C_FLAGS) $(LNK_FLAGS) xtest.cpp \
		$(LIBS) -o xtest

