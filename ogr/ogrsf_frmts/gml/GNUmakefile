

include ../../../GDALmake.opt

CORE_OBJ =	gmlpropertydefn.o gmlfeatureclass.o gmlfeature.o gmlreader.o \
		parsexsd.o

OGR_OBJ =	ogrgmldriver.o ogrgmldatasource.o ogrgmllayer.o

OBJ =	$(CORE_OBJ) $(OGR_OBJ)

CPPFLAGS :=	-I.. -I../.. $(GDAL_INCLUDE) $(XERCES_INCLUDE) $(CPPFLAGS)
#CFLAGS	:=	$(filter-out -Wall,$(CFLAGS))

ifeq ($(HAVE_XERCES),yes)
CORE_OBJ :=	$(CORE_OBJ) gmlreadstate.o gmlhandler.o trstring.o
CPPFLAGS +=  -DHAVE_XERCES=1
else
CPPFLAGS +=  -DHAVE_XERCES=0
endif

# By default, XML validation is disabled.  Uncomment the following line to
# enable XML schema validation in the parser.
#CPPFLAGS +=  -DOGR_GML_VALIDATION=1

default:	$(O_OBJ)

clean:
	rm -f *.o gmlview $(O_OBJ)

gmlview:	gmlview.cpp $(CORE_OBJ)
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(LNK_FLAGS) \
		gmlview.cpp $(CORE_OBJ) $(GDAL_LIB) $(LIBS) \
		-o gmlview


