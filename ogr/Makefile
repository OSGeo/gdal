
EXTRAFLAGS =	-I$(VCDIR)\ATL\Include \
		-I$(MSDASDK)\include\oledb \
		-I. -Iogrsf_frmts -I..\frmts\gtiff\libgeotiff /W3

GDAL_ROOT = 	..

!INCLUDE ..\nmake.opt

OLE_LIBS =	oledb.lib ole32.lib oleaut32.lib 

ALL_LIBS =	$(OLE_LIBS) $(GDAL_ROOT)/port/cpl.lib $(LIBS)

OBJ_OGR	=	ogrgeometryfactory.obj ogrpoint.obj ogrcurve.obj \
		ogrlinestring.obj ogrpolygon.obj ogrlinearring.obj \
		ogrutils.obj ogrgeometry.obj ogrgeometrycollection.obj \
		ogrmultipolygon.obj ogrmultilinestring.obj \
                ogrmultipoint.obj ogrfeature.obj ogrfeaturedefn.obj \
		ogrfielddefn.obj ogr_srsnode.obj ogrspatialreference.obj \
		ogr_srs_proj4.obj ogr_fromepsg.obj ogrct.obj

OBJ_OLEDB =	oledb_sup.obj \
		sfcenumerator.obj sfcdatasource.obj sfctable.obj

default:        test1.exe sfcdump.exe test2.exe ogrinfo.exe test_ogrsf.exe

frmts:
	cd ogrsf_frmts
	$(MAKE) 
	cd ..

oledb:
	cd sfcom_oledb
	$(MAKE)
	cd ..

all:	frmts default oledb

geometryidl.h: geometryidl.idl spatialreferenceidl.h
	midl geometryidl.idl

spatialreferenceidl.h: spatialreferenceidl.idl
	midl spatialreferenceidl.idl

ogr.lib:	$(OBJ_OGR)
	lib /out:ogr.lib $(OBJ_OGR)

ogroledb.lib:	$(OBJ_OLEDB)
	lib /out:ogroledb.lib $(OBJ_OLEDB)

sfcdump.exe:	geometryidl.h sfcdump.obj ogr.lib ogroledb.lib
	cl sfcdump.obj ogroledb.lib ogr.lib $(ALL_LIBS)

test1.exe:	test1.obj $(OBJ_OGR) 
	cl test1.obj $(OBJ_OGR) $(ALL_LIBS)

test2.exe:	test2.obj $(OBJ_OGR)
	cl test2.obj $(OBJ_OGR) ogrsf_frmts\ogrsf_frmts.lib \
			$(ALL_LIBS)

test3.exe:      test3.obj $(OBJ_OGR)
        cl test3.obj $(OBJ_OGR) ogrsf_frmts\ogrsf_frmts.lib \
			$(ALL_LIBS)

test_ogrsf.exe:      test_ogrsf.obj $(OBJ_OGR)
        cl test_ogrsf.obj $(OBJ_OGR) ogrsf_frmts\ogrsf_frmts.lib \
			$(ALL_LIBS)

ogrinfo.exe:      ogrinfo.obj $(OBJ_OGR)
        cl ogrinfo.obj $(OBJ_OGR) ogrsf_frmts\ogrsf_frmts.lib \
			$(ALL_LIBS)

sdts2mi.exe:      sdts2mi.obj $(OBJ_OGR)
        cl sdts2mi.obj $(OBJ_OGR) ogrsf_frmts\ogrsf_frmts.lib \
			$(ALL_LIBS)

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

