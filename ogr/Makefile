
MSDASDK =	F:\MSDASDK

CFLAGS	=	-I$(MSDASDK)\include\oledb

OLE_LIBS =	$(MSDASDK)\lib\oledb\oledb.lib ole32.lib oleaut32.lib 

OBJ_OGR	=	ogrgeometryfactory.obj ogrpoint.obj ogrcurve.obj \
		ogrlinestring.obj ogrpolygon.obj ogrlinearring.obj
OBJ_OLEDB =	oledb_sup.obj oledbsuprowset.obj oledb_sf.obj

default:	test1.exe sfdump.exe

geometryidl.h: geometryidl.idl spatialreferenceidl.h
	midl geometryidl.idl

spatialreferenceidl.h: spatialreferenceidl.idl
	midl spatialreferenceidl.idl

sfdump.obj:	sfdump.cpp sfclsid.h geometryidl.h
	$(CC) /c $(CFLAGS) sfdump.cpp

test1.obj:	test1.cpp
	$(CC) /c $(CFLAGS) test1.cpp

ogrpoint.obj:	ogrpoint.cpp
	$(CC) /c $(CFLAGS) ogrpoint.cpp

ogrcurve.obj:	ogrcurve.cpp
	$(CC) /c $(CFLAGS) ogrcurve.cpp

ogrlinestring.obj:	ogrlinestring.cpp
	$(CC) /c $(CFLAGS) ogrlinestring.cpp

ogrlinearring.obj:	ogrlinearring.cpp
	$(CC) /c $(CFLAGS) ogrlinearring.cpp

ogrpolygon.obj:	ogrpolygon.cpp
	$(CC) /c $(CFLAGS) ogrpolygon.cpp

ogrgeometryfactory.obj:	ogrgeometryfactory.cpp
	$(CC) /c $(CFLAGS) ogrgeometryfactory.cpp

oledb_sup.obj:	oledb_sup.cpp
	$(CC) /c $(CFLAGS) oledb_sup.cpp

oledb_sf.obj:	oledb_sf.cpp
	$(CC) /c $(CFLAGS) oledb_sf.cpp

oledbsuprowset.obj:	oledbsuprowset.cpp
	$(CC) /c $(CFLAGS) oledbsuprowset.cpp

sfdump.exe:	sfdump.obj $(OBJ_OLEDB) $(OBJ_OGR) 
	cl sfdump.obj $(OBJ_OLEDB) $(OBJ_OGR) $(OLE_LIBS)

test1.exe:	test1.obj $(OBJ_OGR) 
	cl test1.obj $(OBJ_OGR) $(LIBS)

clean:
	rm *.obj *.exe
