
include ../../GDALmake.opt

SUBDIRS-yes	:= generic shape ntf sdts tiger s57 mitab dgn gml avc rec mem \
		   vrt csv
SUBDIRS-$(HAVE_OGDI)	+= ogdi
SUBDIRS-$(HAVE_OCI)	+= oci
SUBDIRS-$(HAVE_OGR_PG)	+= pg
SUBDIRS-$(HAVE_MYSQL)	+= mysql
SUBDIRS-$(HAVE_FME)	+= fme
SUBDIRS-$(HAVE_DODS)	+= dods
SUBDIRS-$(ODBC_SETTING)	+= odbc pgeo
SUBDIRS-$(HAVE_SQLITE)	+= sqlite
SUBDIRS-$(HAVE_GRASS)	+= grass
SUBDIRS-$(HAVE_XERCES)	+= ili
SUBDIRS-$(HAVE_DWGDIRECT) += dxfdwg
SUBDIRS-$(HAVE_SDE)     += sde

default:	$(foreach d,$(SUBDIRS-yes),$(d)-target)

clean:	$(foreach d,$(SUBDIRS-yes),$(d)-clean)
	rm -f o/*.o
	$(RM) o/*.lo

