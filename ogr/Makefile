
MSDASDK =	F:\MSDASDK

EXTRAFLAGS =	-I$(MSDASDK)\include\oledb

GDAL_ROOT = 	..

!INCLUDE ..\nmake.opt

OLE_LIBS =	$(MSDASDK)\lib\oledb\oledb.lib ole32.lib oleaut32.lib 

ALL_LIBS =	$(OLE_LIBS) ..\port\*.obj $(LIBS)

OBJ_OGR	=	ogrgeometryfactory.obj ogrpoint.obj ogrcurve.obj \
		ogrlinestring.obj ogrpolygon.obj ogrlinearring.obj \
		ogrutils.obj ogrgeometry.obj 
OBJ_OLEDB =	oledb_sup.obj oledbsuprowset.obj oledb_sf.obj

default:	test1.exe sfdump.exe

geometryidl.h: geometryidl.idl spatialreferenceidl.h
	midl geometryidl.idl

spatialreferenceidl.h: spatialreferenceidl.idl
	midl spatialreferenceidl.idl

sfdump.exe:	geometryidl.h sfdump.obj $(OBJ_OLEDB) $(OBJ_OGR) 
	cl /Zi sfdump.obj $(OBJ_OLEDB) $(OBJ_OGR) $(ALL_LIBS)

test1.exe:	test1.obj $(OBJ_OGR) 
	cl test1.obj $(OBJ_OGR) $(ALL_LIBS)

clean:
	rm *.obj *.exe
