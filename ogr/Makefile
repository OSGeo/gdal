
LINKFLAGS =	/Zi /MTd
EXTRAFLAGS =	-I$(MSDASDK)\inc \
		-I$(VCDIR)\ATL\Include -DWIN32 \
		-I. -Iogrsf_frmts -I..\frmts\gtiff\libgeotiff /W3

GDAL_ROOT = 	..

!INCLUDE ..\nmake.opt

OLE_LIBS =	oledb.lib ole32.lib oleaut32.lib 

ALL_LIBS =	$(OLE_LIBS) $(GDAL_ROOT)/port/cpl.lib $(OGDILIB) $(LIBS)

OGR_FRMTS =	ogrsf_frmts\ogrsf_frmts.lib ogrsf_frmts\ogrsf_frmts_sup.lib
OBJ_OGR	=	ogrgeometryfactory.obj ogrpoint.obj ogrcurve.obj \
		ogrlinestring.obj ogrpolygon.obj ogrlinearring.obj \
		ogrutils.obj ogrgeometry.obj ogrgeometrycollection.obj \
		ogrmultipolygon.obj ogrmultilinestring.obj ogr_opt.obj \
                ogrmultipoint.obj ogrfeature.obj ogrfeaturedefn.obj \
		ogrfielddefn.obj ogr_srsnode.obj ogrspatialreference.obj \
		ogr_srs_proj4.obj ogr_fromepsg.obj ogrct.obj swq.obj \
		ogrfeaturestyle.obj ogr_srs_esri.obj ogrfeaturequery.obj \
		ogr_srs_validate.obj

OBJ_OLEDB =	oledb_sup.obj \
		sfcenumerator.obj sfcdatasource.obj sfctable.obj

default:        ogr.lib ogrinfo.exe test_ogrsf.exe ogr2ogr.exe

frmts:
	cd ogrsf_frmts
	$(MAKE) 
	cd ..

oledb:	ogroledb.lib 
	cd sfcom_oledb
	$(MAKE)
	cd ..

all:	frmts default oledb sfcdump.exe test1.exe test2.exe

geometryidl.h: geometryidl.idl spatialreferenceidl.h
	midl geometryidl.idl

spatialreferenceidl.h: spatialreferenceidl.idl
	midl spatialreferenceidl.idl

ogr.lib:	$(OBJ_OGR)
	lib /out:ogr.lib $(OBJ_OGR)

ogroledb.lib:	$(OBJ_OLEDB)
	lib /out:ogroledb.lib $(OBJ_OLEDB)

sfcdump.exe:	geometryidl.h sfcdump.obj ogr.lib ogroledb.lib
	cl $(LINKFLAGS) sfcdump.obj ogroledb.lib ogr.lib $(ALL_LIBS) \
		 /link $(LINKER_FLAGS)

test1.exe:	test1.obj $(OBJ_OGR) 
	cl $(LINKFLAGS) test1.obj $(OBJ_OGR) $(ALL_LIBS) \
		 /link $(LINKER_FLAGS)

test2.exe:	test2.obj $(OBJ_OGR)
	cl $(LINKFLAGS) test2.obj $(OBJ_OGR) $(OGR_FRMTS) \
			$(ALL_LIBS) /link $(LINKER_FLAGS)

test3.exe:      test3.obj $(OBJ_OGR)
        cl $(LINKFLAGS) test3.obj $(OBJ_OGR) $(OGR_FRMTS) \
			$(ALL_LIBS) /link $(LINKER_FLAGS)

test_ogrsf.exe:      test_ogrsf.obj $(OBJ_OGR)
        cl $(LINKFLAGS) test_ogrsf.obj $(OBJ_OGR) $(OGR_FRMTS) \
			$(ALL_LIBS) /link $(LINKER_FLAGS)

test_rep.exe:      test_rep.obj $(OBJ_OGR)
        cl $(LINKFLAGS) test_rep.obj $(OBJ_OGR) $(OGR_FRMTS) \
			$(ALL_LIBS) /link $(LINKER_FLAGS)

ogrinfo.exe:      ogrinfo.obj $(OBJ_OGR)
        cl $(LINKFLAGS) ogrinfo.obj $(OBJ_OGR) $(OGR_FRMTS) \
			$(ALL_LIBS) /link $(LINKER_FLAGS)

ogr2ogr.exe:      ogr2ogr.obj $(OBJ_OGR)
        cl $(LINKFLAGS) ogr2ogr.obj $(OBJ_OGR) $(OGR_FRMTS) \
			$(ALL_LIBS) /link $(LINKER_FLAGS)

sdts2mi.exe:      sdts2mi.obj $(OBJ_OGR)
        cl $(LINKFLAGS) sdts2mi.obj $(OBJ_OGR) $(OGR_FRMTS) \
			$(ALL_LIBS) /link $(LINKER_FLAGS)

clean:
	-del *.obj
        -del *.exe
	-del *.lib
	-del *.pdb

allclean:	clean
	cd ogrsf_frmts
	$(MAKE) clean
	cd ..\sfcom_oledb
	$(MAKE) clean
	cd ..

