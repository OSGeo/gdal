
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ogrsfdriverregistrar.o ogrlayer.o ogrdatasource.o \
		ogrsfdriver.o ogrregisterall.o ogr_gensql.o \
		ogr_attrind.o ogr_miattrind.o

ifeq ($(HAVE_OGDI),yes)
CFLAGS   :=	$(CFLAGS) -DOGDI_ENABLED
CXXFLAGS :=	$(CXXFLAGS) -DOGDI_ENABLED
endif

ifeq ($(HAVE_OCI),yes)
CFLAGS   :=	$(CFLAGS) -DOCI_ENABLED
CXXFLAGS :=	$(CXXFLAGS) -DOCI_ENABLED
endif

ifeq ($(HAVE_OGR_PG),yes)
CFLAGS   :=	$(CFLAGS) -DPG_ENABLED
CXXFLAGS :=	$(CXXFLAGS) -DPG_ENABLED
endif

ifeq ($(HAVE_FME),yes)
CFLAGS   :=	$(CFLAGS) -DFME_ENABLED
CXXFLAGS :=	$(CXXFLAGS) -DFME_ENABLED
endif

ifeq ($(ODBC_SETTING),yes)
CFLAGS   :=	$(CFLAGS) -DODBC_ENABLED
CXXFLAGS :=	$(CXXFLAGS) -DODBC_ENABLED
endif

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)
