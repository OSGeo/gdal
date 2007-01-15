
GDAL_ROOT = 	..\..\..
OGR       =	..\..
SFOLEDB   =     ..

PROJ	=	smsf
DLLNAME	=	SoftmapSF.dll

# To switch between VStudio .NET, and VC++ 6 just switch which of the
# \ATL_{vc6,net} lines is uncommented.

EXTRAFLAGS =	-I$(SFOLEDB) \
#		-I$(SFOLEDB)\atl_net -DSUPPORT_ATL_NET \
		-I$(SFOLEDB)\atl_vc6 \
		-I$(MSDASDK)\include\oledb -I$(OGR) /W2 \
		-I$(OGR)\ogrsf_frmts -I$(VCDIR)\ATL\Include \
		-DCACHE_OGRDATASOURCE \
		-DSUPPORT_ADSK_GEOM_EXTENT \
#		-DSUPPORT_2D_FLATTEN

!INCLUDE $(GDAL_ROOT)/nmake.opt

OLE_LIBS =	ole32.lib oleaut32.lib 

!IFDEF DLLBUILD
OGR_LIBS =	..\..\..\gdal_i.lib $(LIBS)
!ELSE
OGR_LIBS =	$(OGR)\ogrsf_frmts\ogrsf_frmts.lib \
		$(OGR)\ogrsf_frmts\ogrsf_frmts_sup.lib \
		$(OGR)\ogroledb.lib $(OGR)\ogr.lib \
		$(GDAL_ROOT)\port\cpl.lib  $(OGDILIB) $(XERCES_LIB) $(OCI_LIB) 
!ENDIF

LOC_OBJ =       SF.obj SFRS.obj sfutil.obj sftraceback.obj sfregister.obj \
		ogrvirtualarray.obj swq.obj sfdscache.obj SFDS.obj SFSess.obj

default:	$(DLLNAME)

all:	$(DLLNAME) install

SF.obj SFRS.obj: SF.h $(SFOLEDB)/SFDS.h $(SFOLEDB)/SFRS.h \
		$(SFOLEDB)/SFSess.h 

$(PROJ).res:	$(PROJ).rc SF.h $(SFOLEDB)/sf_common.rc
	rc.exe /l 0x1009 /fo$(PROJ).res /I$(SFOLEDB) /d _DEBUG $(PROJ).rc

$(DLLNAME):	SF.h $(LOC_OBJ) $(PROJ).res
	link /dll /debug /def:$(SFOLEDB)/SF.def /out:$(DLLNAME) \
		$(LOC_OBJ) $(OGR_LIBS) $(OLE_LIBS) $(PROJ).res \
		$(LINKER_FLAGS)

clean:
	-del *.obj *.exe *.dll $(PROJ).res $(DLLNAME)
	-del *.lib *.ilk *.exp *.pdb SF.h SF.tlb SF_i.c
	-del sf.cpp sfutil.cpp sftraceback.cpp SFRS.cpp ogrvirtualarray.cpp

install:      $(DLLNAME)
	regsvr32 $(DLLNAME)

SF.h:      $(PROJ).idl
	midl /tlb SF.tlb /h SF.h /Oicf /iid SF_i.c $(PROJ).idl

sf.cpp:	$(SFOLEDB)\sf.cpp
	copy $(SFOLEDB)\sf.cpp

sfutil.cpp:	$(SFOLEDB)\sfutil.cpp
	copy $(SFOLEDB)\sfutil.cpp

sftraceback.cpp:	$(SFOLEDB)\sftraceback.cpp
	copy $(SFOLEDB)\sftraceback.cpp

SFRS.cpp:	$(SFOLEDB)\SFRS.cpp
	copy $(SFOLEDB)\SFRS.cpp

SFSess.cpp:	$(SFOLEDB)\SFSess.cpp
	copy $(SFOLEDB)\SFSess.cpp

SFDS.cpp: $(SFOLEDB)\SFDS.cpp
	copy $(SFOLEDB)\SFDS.cpp

SFDSCache.cpp:	$(SFOLEDB)\SFDSCache.cpp
	copy $(SFOLEDB)\SFDSCache.cpp

ogrvirtualarray.cpp:	$(SFOLEDB)\ogrvirtualarray.cpp
	copy $(SFOLEDB)\ogrvirtualarray.cpp

swq.c:	..\..\swq.c
	copy ..\..\swq.c


