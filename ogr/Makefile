
VSTUDIO =       F:\VisualStudio
MSDASDK =       C:\MSDASDK

EXTRAFLAGS =	-I$(VSTUDIO)\VC98\ATL\Include \
		-I$(MSDASDK)\include\oledb \
		-I. -Iogrsf_frmts

GDAL_ROOT = 	..

!INCLUDE ..\nmake.opt

OLE_LIBS =	oledb.lib ole32.lib oleaut32.lib 

ALL_LIBS =	$(OLE_LIBS) ..\port\*.obj $(LIBS)

OBJ_OGR	=	ogrgeometryfactory.obj ogrpoint.obj ogrcurve.obj \
		ogrlinestring.obj ogrpolygon.obj ogrlinearring.obj \
		ogrutils.obj ogrgeometry.obj ogrgeometrycollection.obj \
		ogrmultipolygon.obj ogrmultilinestring.obj \
                ogrmultipoint.obj ogrfeature.obj ogrfeaturedefn.obj \
		ogrfielddefn.obj ogr_srsnode.obj ogrspatialreference.obj

OBJ_OLEDB =	oledb_sup.obj oledbsuprowset.obj oledb_sf.obj \
		sfcenumerator.obj sfcdatasource.obj sfctable.obj

default:        test1.exe sfdump.exe shape2ogr.obj sfcdump.exe test2.exe

geometryidl.h: geometryidl.idl spatialreferenceidl.h
	midl geometryidl.idl

spatialreferenceidl.h: spatialreferenceidl.idl
	midl spatialreferenceidl.idl

sfdump.exe:	geometryidl.h sfdump.obj $(OBJ_OLEDB) $(OBJ_OGR) 
	cl /Zi sfdump.obj $(OBJ_OLEDB) $(OBJ_OGR) $(ALL_LIBS)

sfcdump.exe:	geometryidl.h sfcdump.obj $(OBJ_OLEDB) $(OBJ_OGR) 
	cl /Zi sfcdump.obj $(OBJ_OLEDB) $(OBJ_OGR) $(ALL_LIBS)

test1.exe:	test1.obj $(OBJ_OGR) 
	cl test1.obj $(OBJ_OGR) $(ALL_LIBS)

test2.exe:	test2.obj $(OBJ_OGR) 
	cl /Zi test2.obj $(OBJ_OGR) ogrsf_frmts\ogrsf_frmts.lib \
			$(ALL_LIBS)

clean:
	del *.obj
        del *.exe
