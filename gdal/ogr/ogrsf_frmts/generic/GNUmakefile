

include ../../../GDALmake.opt

OBJ	=	ogrsfdriverregistrar.o ogrlayer.o ogrdatasource.o \
		ogrsfdriver.o ogrregisterall.o ogr_gensql.o \
		ogr_attrind.o ogr_miattrind.o ogrlayerdecorator.o \
		ogrwarpedlayer.o ogrunionlayer.o ogrlayerpool.o \
		ogrmutexedlayer.o ogrmutexeddatasource.o \
		ogremulatedtransaction.o ogreditablelayer.o

CXXFLAGS :=     $(CXXFLAGS) $(SHADOW_WFLAGS) -DINST_DATA=\"$(INST_DATA)\"

CXXFLAGS        :=      $(WARN_EFFCPLUSPLUS) $(WARN_OLD_STYLE_CAST) $(CXXFLAGS)

CXXFLAGS :=     $(CXXFLAGS) -DGENERIC_ENABLED -DGEOJSON_ENABLED -DKML_ENABLED -DMEM_ENABLED -DMITAB_ENABLED -DVRT_ENABLED $(OGR_FORMATS_ENABLED_CFLAGS)

ifeq ($(HAVE_OGDI),yes)
CXXFLAGS :=	$(CXXFLAGS) -DOGDI_ENABLED
endif

ifeq ($(HAVE_OCI),yes)
CXXFLAGS :=	$(CXXFLAGS) -DOCI_ENABLED
endif

ifeq ($(HAVE_FGDB),yes)
CXXFLAGS :=	$(CXXFLAGS) -DFGDB_ENABLED
endif

ifeq ($(HAVE_OGR_PG),yes)
CXXFLAGS :=	$(CXXFLAGS) -DPG_ENABLED
endif

ifeq ($(HAVE_MYSQL),yes)
CXXFLAGS :=	$(CXXFLAGS) -DMYSQL_ENABLED
endif

ifeq ($(HAVE_INGRES),yes)
CXXFLAGS :=	$(CXXFLAGS) -DINGRES_ENABLED
endif

ifeq ($(PCIDSK_SETTING),internal)
CXXFLAGS :=	$(CXXFLAGS) -DPCIDSK_ENABLED
endif

ifeq ($(PCIDSK_SETTING),external)
CXXFLAGS :=	$(CXXFLAGS) -DPCIDSK_ENABLED
endif

ifeq ($(HAVE_FME),yes)
CXXFLAGS :=	$(CXXFLAGS) -DFME_ENABLED
endif

ifeq ($(HAVE_DODS),yes)
CXXFLAGS :=	$(CXXFLAGS) -DDODS_ENABLED
endif

ifeq ($(HAVE_SQLITE),yes)
CXXFLAGS :=	$(CXXFLAGS) -DSQLITE_ENABLED
endif

ifeq ($(HAVE_GRASS),yes)
CXXFLAGS :=	$(CXXFLAGS) -DGRASS_ENABLED
endif

ifeq ($(HAVE_LIBKML),yes)
CXXFLAGS :=	$(CXXFLAGS) -DLIBKML_ENABLED
endif

ifeq ($(HAVE_TEIGHA),yes)
CXXFLAGS :=	$(CXXFLAGS) -DDWG_ENABLED -DDGNV8_ENABLED
endif

ifeq ($(HAVE_IDB),yes)
CXXFLAGS :=	$(CXXFLAGS) -DIDB_ENABLED
endif

ifeq ($(HAVE_SOSI),yes)
CXXFLAGS :=	$(CXXFLAGS) -DSOSI_ENABLED
endif

ifeq ($(MDB_ENABLED),yes)
CXXFLAGS :=	$(CXXFLAGS) -DMDB_ENABLED
endif

ifeq ($(HAVE_FREEXL),yes)
CXXFLAGS :=	$(CXXFLAGS) -DXLS_ENABLED
endif

ifeq ($(HAVE_MONGODB),yes)
CXXFLAGS :=	$(CXXFLAGS) -DMONGODB_ENABLED
endif

ifeq ($(HAVE_MONGOCXXV3),yes)
CXXFLAGS :=	$(CXXFLAGS) -DMONGODBV3_ENABLED
endif

CPPFLAGS	:=	-iquote .. -iquote ../..  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	../../../GDALmake.opt ../../ogr_swq.h
