
MSDASDK =	F:\MSDASDK

EXTRAFLAGS =	-I$(MSDASDK)\include\oledb -I.. /W2

GDAL_ROOT = 	..\..

!INCLUDE $(GDAL_ROOT)/nmake.opt


OLE_LIBS =	$(MSDASDK)\lib\oledb\oledb.lib ole32.lib oleaut32.lib 

LOC_OBJ	=	ogrcomgeometryfactory.obj ogrcomutils.obj ogrcomwks.obj \
		ogrcomclassfactory.obj ogrcomgeom_server.obj \
		ogrcompoint.obj ogrcomlinestring.obj ogrcompolygon.obj

SHR_OBJ	=	..\ogrgeometryfactory.obj ..\ogrpoint.obj ..\ogrcurve.obj \
		..\ogrlinestring.obj ..\ogrpolygon.obj ..\ogrlinearring.obj \
		..\oledb_sup.obj ..\ogrgeometry.obj ..\ogrutils.obj \
		..\ogrgeometrycollection.obj ..\ogrmultipolygon.obj \
		..\ogrmultipoint.obj ..\ogrmultilinestring.obj

default:	ogrcomgeometry.dll $(LOC_OBJ) testclient.exe ogrcomgeometry.tlb

ogrcomgeometry.dll:	$(LOC_OBJ) ogrcomgeometry.tlb
	link /debug /dll /def:ogrcomgeometry.def /out:ogrcomgeometry.dll \
		$(LOC_OBJ) $(SHR_OBJ) $(GDAL_ROOT)/port/*.obj $(OLE_LIBS)

.cpp.obj:	
	$(CC) /c $(CFLAGS) $*.cpp

testclient.exe:	testclient.obj 
	cl /Zi testclient.obj $(SHR_OBJ) $(GDAL_ROOT)/port/*.obj $(OLE_LIBS)

clean:
	rm -f *.obj *.exe *.dll

ogrcomgeometry.tlb:	ogrcomgeometry.idl
	midl /I.. ogrcomgeometry.idl

