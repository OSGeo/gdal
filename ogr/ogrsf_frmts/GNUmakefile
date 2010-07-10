
include ../../GDALmake.opt

SUBDIRS-yes	:= \
	generic avc bna csv dgn geojson gml gmt mem kml \
	mitab ntf gpx rec s57 sdts shape tiger vrt \
	geoconcept xplane georss gtm dxf pgdump gpsbabel \
	sua openair pds

SUBDIRS-$(HAVE_DODS)	+= dods
SUBDIRS-$(HAVE_DWGDIRECT) += dxfdwg
SUBDIRS-$(HAVE_FME)	+= fme
SUBDIRS-$(HAVE_GRASS)	+= grass
SUBDIRS-$(HAVE_IDB)	+= idb
SUBDIRS-$(HAVE_XERCES)	+= ili
SUBDIRS-$(HAVE_NAS)	+= nas
SUBDIRS-$(HAVE_MYSQL)	+= mysql
SUBDIRS-$(ODBC_SETTING)	+= odbc pgeo
SUBDIRS-$(HAVE_OGDI)	+= ogdi
SUBDIRS-$(HAVE_OCI)	+= oci
SUBDIRS-$(HAVE_OGR_PG)	+= pg
SUBDIRS-$(HAVE_SQLITE)	+= sqlite
SUBDIRS-$(HAVE_SDE)     += sde
SUBDIRS-$(HAVE_INGRES)  += ingres
SUBDIRS-$(HAVE_VFK)     += vfk
SUBDIRS-$(HAVE_LIBKML)  += libkml

ifeq ($(PCIDSK_SETTING),internal)
SUBDIRS-yes  += pcidsk
endif
ifeq ($(PCIDSK_SETTING),external)
SUBDIRS-yes  += pcidsk
endif

default:	$(foreach d,$(SUBDIRS-yes),$(d)-target)

clean:	$(foreach d,$(SUBDIRS-yes),$(d)-clean)
	rm -f o/*.o
	$(RM) o/*.lo

