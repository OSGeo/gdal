
LINKFLAGS =	/Zi /MTd
EXTRAFLAGS =	-I$(MSDASDK)\inc \
		-I"$(VCDIR)\ATL\Include" -DWIN32 \
		-I. -Iogrsf_frmts -I..\frmts\gtiff\libgeotiff \
		$(PROJ_FLAGS) $(PROJ_INCLUDE) $(GEOS_CFLAGS)

GDAL_ROOT = 	..

!INCLUDE ..\nmake.opt

OLE_LIBS =	oledb.lib ole32.lib oleaut32.lib 

ALL_LIBS =	$(OLE_LIBS) $(GDAL_ROOT)/port/cpl.lib $(EXTERNAL_LIBS) $(LIBS)

OGR_FRMTS =	ogrsf_frmts\ogrsf_frmts.lib ogrsf_frmts\ogrsf_frmts_sup.lib
OBJ_OGR	=	ogrgeometryfactory.obj ogrpoint.obj ogrcurve.obj ogr_api.obj \
		ogrlinestring.obj ogrpolygon.obj ogrlinearring.obj \
		ogrutils.obj ogrgeometry.obj ogrgeometrycollection.obj \
		ogrmultipolygon.obj ogrmultilinestring.obj ogr_opt.obj \
                ogrmultipoint.obj ogrfeature.obj ogrfeaturedefn.obj \
		ogrfielddefn.obj ogr_srsnode.obj ogrspatialreference.obj \
		ogr_srs_proj4.obj ogr_fromepsg.obj ogrct.obj swq.obj \
		ogrfeaturestyle.obj ogr_srs_esri.obj ogrfeaturequery.obj \
		ogr_srs_validate.obj ogr_srs_xml.obj ograssemblepolygon.obj \
		ogr2gmlgeometry.obj gml2ogrgeometry.obj ogr_srs_pci.obj \
		ogr_srs_usgs.obj ogr_srs_dict.obj ogr_srs_panorama.obj

OBJ_OLEDB =	oledb_sup.obj \
		sfcenumerator.obj sfcdatasource.obj sfctable.obj

!IFDEF DLLBUILD
LINKARGS =	..\gdal_i.lib $(LIBS)
DEPLIBS	=	..\gdal_i.lib
!ELSE
LINKARGS =	$(OBJ_OGR) $(OGR_FRMTS) $(ALL_LIBS)
DEPLIBS	=	$(OGR_FRMTS)
!ENDIF

default:        ogr.lib ogrinfo.exe test_ogrsf.exe ogr2ogr.exe ogrtindex.exe

ogrsf_frmts\ogrsf_frmts.lib:

sublibs:	frmts

frmts:
	cd ogrsf_frmts
	$(MAKE) 
	cd ..

oledb:	ogroledb.lib 
	cd sfcom_oledb
	$(MAKE)
	cd ..

all:	frmts default oledb sfcdump.exe

geometryidl.h: geometryidl.idl spatialreferenceidl.h
	midl geometryidl.idl

spatialreferenceidl.h: spatialreferenceidl.idl
	midl spatialreferenceidl.idl

ogr.lib:	$(OBJ_OGR)
	lib /out:ogr.lib $(OBJ_OGR)

ogroledb.lib:	$(OBJ_OLEDB)
	lib /out:ogroledb.lib $(OBJ_OLEDB)

sfcdump.exe:	geometryidl.h sfcdump.obj ogr.lib ogroledb.lib
	cl $(LINKFLAGS) sfcdump.obj ogroledb.lib $(LINKARGS) \
		/link $(LINKER_FLAGS)

test1.exe:	test1.obj $(OBJ_OGR) $(DEPLIBS)
	cl $(LINKFLAGS) test1.obj $(OBJ_OGR) $(ALL_LIBS) \
		 /link $(LINKER_FLAGS)

test2.exe:	test2.obj $(OBJ_OGR) $(DEPLIBS)
	cl $(LINKFLAGS) test2.obj $(OBJ_OGR) $(OGR_FRMTS) \
			$(ALL_LIBS) /link $(LINKER_FLAGS)

test3.exe:      test3.obj $(OBJ_OGR) $(DEPLIBS)
        cl $(LINKFLAGS) test3.obj $(OBJ_OGR) $(OGR_FRMTS) \
			$(ALL_LIBS) /link $(LINKER_FLAGS)

test_ogrsf.exe:      test_ogrsf.obj $(OBJ_OGR) $(DEPLIBS)
        cl $(LINKFLAGS) test_ogrsf.obj $(OBJ_OGR) $(OGR_FRMTS) \
			$(ALL_LIBS) /link $(LINKER_FLAGS)

test_rep.exe:      test_rep.obj $(OBJ_OGR) $(DEPLIBS)
        cl $(LINKFLAGS) test_rep.obj $(OBJ_OGR) $(OGR_FRMTS) \
			$(ALL_LIBS) /link $(LINKER_FLAGS)

testepsg.exe:      testepsg.obj $(OBJ_OGR) $(DEPLIBS)
        cl $(LINKFLAGS) testepsg.obj $(LINKARGS) /link $(LINKER_FLAGS)

ogrinfo.exe:      ogrinfo.obj $(OBJ_OGR) $(DEPLIBS)
        cl $(LINKFLAGS) ogrinfo.obj $(LINKARGS) /link $(LINKER_FLAGS)

ogr2ogr.exe:      ogr2ogr.obj $(OBJ_OGR) $(DEPLIBS)
        cl $(LINKFLAGS) ogr2ogr.obj $(LINKARGS) /link $(LINKER_FLAGS)

ogrtindex.exe:      ogrtindex.obj $(OBJ_OGR) $(DEPLIBS)
        cl $(LINKFLAGS) ogrtindex.obj $(SETARGV) $(LINKARGS) /link $(LINKER_FLAGS)

sdts2mi.exe:      sdts2mi.obj $(OBJ_OGR) $(DEPLIBS)
        cl $(LINKFLAGS) sdts2mi.obj $(OBJ_OGR) $(OGR_FRMTS) \
			$(ALL_LIBS) /link $(LINKER_FLAGS)

install:	default
	copy ogrinfo.exe $(BINDIR)
	copy ogr2ogr.exe $(BINDIR)
	copy ogrtindex.exe $(BINDIR)
	-if exist ogrinfo.exe.manifest xcopy /y /r /d /f *.exe.manifest $(BINDIR)

clean:
	-del *.obj
	-del *.exe
	-del *.exe.manifest
	-del *.lib
	-del *.pdb
	-del *.ilk

allclean:	clean
	cd ogrsf_frmts
	$(MAKE) clean
	cd ..\sfcom_oledb
	$(MAKE) clean
	cd ..

