
MSDASDK =	F:\MSDASDK
VSTUDIO =       F:\VisualStudio

EXTRAFLAGS =	-I$(VSTUDIO)\VC98\ATL\Include \
		-I$(MSDASDK)\include\oledb -I.. /W2 \
		-I..\..\frmts\shapelib

GDAL_ROOT = 	..\..

!INCLUDE $(GDAL_ROOT)/nmake.opt

#OLE_LIBS =	$(MSDASDK)\lib\oledb\oledb.lib ole32.lib oleaut32.lib 
OLE_LIBS =	ole32.lib oleaut32.lib 

LOC_OBJ	=	SF.obj SFRS.obj

#SHR_OBJ =	..\ogrgeometryfactory.obj ..\ogrpoint.obj ..\ogrcurve.obj \
#		..\ogrlinestring.obj ..\ogrpolygon.obj ..\ogrlinearring.obj \
#		..\oledb_sup.obj ..\ogrgeometry.obj ..\ogrutils.obj 

SHR_OBJ =	..\..\frmts\shapelib\dbfopen.obj \
		..\..\frmts\shapelib\shpopen.obj

default:	SF.dll

SF.res:	SF.rc SF.h
	rc.exe /l 0x1009 /foSF.res /d _DEBUG SF.rc

SF.dll:	SF.h $(LOC_OBJ) SF.res
	link /debug /dll /def:SF.def /out:SF.dll \
		$(LOC_OBJ) $(SHR_OBJ) $(OLE_LIBS) SF.res

clean:
	rm -f *.obj *.exe *.dll SF.h SF_i.c SF.tlb SF.res

SF.h:      SF.idl
	midl /tlb SF.tlb /h SF.h /0icf /iid SF_i.c SF.idl
