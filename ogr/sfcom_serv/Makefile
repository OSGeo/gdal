
MSDASDK =	F:\MSDASDK

CFLAGS	=	/Zi -I$(MSDASDK)\include\oledb -I..

OLE_LIBS =	$(MSDASDK)\lib\oledb\oledb.lib ole32.lib oleaut32.lib 

LOC_OBJ	=	ogrcomgeometryfactory.obj \
		ogrcomclassfactory.obj ogrcomgeom_server.obj \
		ogrcompoint.obj ogrcomlinestring.obj ogrcompolygon.obj

SHR_OBJ	=	..\ogrgeometryfactory.obj ..\ogrpoint.obj ..\ogrcurve.obj \
		..\ogrlinestring.obj ..\ogrpolygon.obj ..\ogrlinearring.obj \
		..\oledb_sup.obj

default:	ogrcomgeometry.dll $(LOC_OBJ) testclient.exe 

ogrcomgeometry.dll:	$(LOC_OBJ)
	link /debug /dll /def:ogrcomgeometry.def /out:ogrcomgeometry.dll \
		$(LOC_OBJ) $(SHR_OBJ) $(OLE_LIBS)

ogrcomclassfactory.obj:	ogrcomclassfactory.cpp
	$(CC) /c $(CFLAGS) ogrcomclassfactory.cpp

ogrcomgeometryfactory.obj:	ogrcomgeometryfactory.cpp
	$(CC) /c $(CFLAGS) ogrcomgeometryfactory.cpp

ogrcomgeometrytmpl.obj:	ogrcomgeometrytmpl.cpp
	$(CC) /c $(CFLAGS) ogrcomgeometrytmpl.cpp

ogrcompoint.obj:	ogrcompoint.cpp
	$(CC) /c $(CFLAGS) ogrcompoint.cpp

ogrcomlinestring.obj:	ogrcomlinestring.cpp
	$(CC) /c $(CFLAGS) ogrcomlinestring.cpp

ogrcompolygon.obj:	ogrcompolygon.cpp
	$(CC) /c $(CFLAGS) ogrcompolygon.cpp

ogrcomgeom_server.obj:	ogrcomgeom_server.cpp
	$(CC) /c $(CFLAGS) ogrcomgeom_server.cpp

testclient.obj:	testclient.cpp
	$(CC) /c $(CFLAGS) testclient.cpp

testclient.exe:	testclient.obj 
	cl /Zi testclient.obj $(SHR_OBJ) $(OLE_LIBS)

clean:
	rm -f *.obj *.exe *.dll
