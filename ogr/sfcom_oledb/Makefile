
EXTRAFLAGS =	-I$(VCDIR)\ATL\Include \
		-I$(MSDASDK)\include\oledb -I.. /W2 \
		-I..\ogrsf_frmts 

GDAL_ROOT = 	..\..

!INCLUDE $(GDAL_ROOT)/nmake.opt

OLE_LIBS =	ole32.lib oleaut32.lib 


default:	SF.h

SF.h:      SF.idl
	midl /tlb SF.tlb /h SF.h /Oicf /iid SF_i.c SF.idl

clean:
	del SF.h SF_i.c SF.tlb

