
MSDASDK =	F:\MSDASDK

CFLAGS	=	-I$(MSDASDK)\include\oledb

OLE_LIBS =	$(MSDASDK)\lib\oledb\oledb.lib ole32.lib oleaut32.lib 

OBJ_OGR	=	ogrgeometryfactory.obj ogrpoint.obj ogrcurve.obj \
		ogrlinestring.obj 
OBJ_OLEDB =	oledb_sup.obj oledbsuprowset.obj

default:	test1.exe sfdump.exe

sfdump.obj:	sfdump.cpp
	$(CC) /c $(CFLAGS) sfdump.cpp

test1.obj:	test1.cpp
	$(CC) /c $(CFLAGS) test1.cpp

ogrpoint.obj:	ogrpoint.cpp
	$(CC) /c $(CFLAGS) ogrpoint.cpp

ogrcurve.obj:	ogrcurve.cpp
	$(CC) /c $(CFLAGS) ogrcurve.cpp

ogrlinestring.obj:	ogrlinestring.cpp
	$(CC) /c $(CFLAGS) ogrlinestring.cpp

ogrgeometryfactory.obj:	ogrgeometryfactory.cpp
	$(CC) /c $(CFLAGS) ogrgeometryfactory.cpp

oledb_sup.obj:	oledb_sup.cpp
	$(CC) /c $(CFLAGS) oledb_sup.cpp

oledbsuprowset.obj:	oledbsuprowset.cpp
	$(CC) /c $(CFLAGS) oledbsuprowset.cpp

sfdump.exe:	sfdump.obj $(OBJ_OLEDB) $(OBJ_OGR) 
	cl sfdump.obj $(OBJ_OLEDB) $(OBJ_OGR) $(OLE_LIBS)

test1.exe:	test1.obj $(OBJ_OGR) 
	cl test1.obj $(OBJ_OGR) $(LIBS)

clean:
	rm *.obj *.exe
