
EXTRAFLAGS =	-I$(VCDIR)\ATL\Include \
		-I$(MSDASDK)\include\oledb -I.. /W2 \
		-I..\ogrsf_frmts \
		-I..\ogrsf_frmts\shape

GDAL_ROOT = 	..\..

!INCLUDE $(GDAL_ROOT)/nmake.opt

#OLE_LIBS =	$(MSDASDK)\lib\oledb\oledb.lib ole32.lib oleaut32.lib 
OLE_LIBS =	ole32.lib oleaut32.lib 
OGR_LIBS =	..\ogrsf_frmts\ogrsf_frmts.lib ..\ogroledb.lib ..\ogr.lib \
		$(GDAL_ROOT)\port\cpl.lib

LOC_OBJ =       SF.obj SFRS.obj sfutil.obj sftraceback.obj

default:	SF.dll

SF.obj SFRS.obj: SF.h SFDS.h SFRS.h SFSess.h 

SF.res:	SF.rc SF.h
	rc.exe /l 0x1009 /foSF.res /d _DEBUG SF.rc

SF.dll:	SF.h $(LOC_OBJ) SF.res
	link /debug /dll /def:SF.def /out:SF.dll \
		$(LOC_OBJ) $(OGR_LIBS) $(OLE_LIBS) SF.res

clean:
	del *.obj *.exe *.dll SF.h SF_i.c SF.tlb SF.res
	del *.lib *.ilk *.exp *.pdb

SF.h:      SF.idl
	midl /tlb SF.tlb /h SF.h /Oicf /iid SF_i.c SF.idl

install:      SF.dll
	regsvr32 SF.dll
