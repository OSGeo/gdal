
GDAL_ROOT = 	..\..\..
OGR       =	..\..
SFOLEDB   =     ..

PROJ	=	smsf
DLLNAME	=	SoftmapSF.dll

EXTRAFLAGS =	-I$(SFOLEDB) \
		-I$(VCDIR)\ATL\Include \
		-I$(MSDASDK)\include\oledb -I$(OGR) -I$(SFOLEDB) /W2 \
		-I$(OGR)\ogrsf_frmts 


!INCLUDE $(GDAL_ROOT)/nmake.opt

OLE_LIBS =	ole32.lib oleaut32.lib 
OGR_LIBS =	$(OGR)\ogrsf_frmts\ogrsf_frmts.lib \
		$(OGR)\ogroledb.lib $(OGR)\ogr.lib \
		$(GDAL_ROOT)\port\cpl.lib

LOC_OBJ =       SF.obj SFRS.obj sfutil.obj sftraceback.obj sfregister.obj

default:	$(DLLNAME)

all:	$(DLLNAME) install

SF.obj SFRS.obj: $(SFOLEDB)/SF.h $(SFOLEDB)/SFDS.h $(SFOLEDB)/SFRS.h \
		$(SFOLEDB)/SFSess.h 

$(PROJ).res:	$(PROJ).rc $(SFOLEDB)/SF.h $(SFOLEDB)/sf_common.rc
	rc.exe /l 0x1009 /fo$(PROJ).res /I$(SFOLEDB) /d _DEBUG $(PROJ).rc

$(DLLNAME):	$(SFOLEDB)/SF.h $(LOC_OBJ) $(PROJ).res
	link /dll /def:$(SFOLEDB)/SF.def /out:$(DLLNAME) \
		$(LOC_OBJ) $(OGR_LIBS) $(OLE_LIBS) $(PROJ).res

clean:
	del *.obj *.exe *.dll $(PROJ).res $(DLLNAME)
	del *.lib *.ilk *.exp *.pdb
	del sf.cpp sfutil.cpp sftraceback.cpp SFRS.cpp

install:      $(DLLNAME)
	regsvr32 $(DLLNAME)


sf.cpp:	$(SFOLEDB)\sf.cpp
	copy $(SFOLEDB)\sf.cpp

sfutil.cpp:	$(SFOLEDB)\sfutil.cpp
	copy $(SFOLEDB)\sfutil.cpp

sftraceback.cpp:	$(SFOLEDB)\sftraceback.cpp
	copy $(SFOLEDB)\sftraceback.cpp

SFRS.cpp:	$(SFOLEDB)\SFRS.cpp
	copy $(SFOLEDB)\SFRS.cpp



