default:
	cd generic
	$(MAKE) $*
	cd ..\shape 
	$(MAKE) $*
	cd ..\ntf
	$(MAKE) $*
	cd ..\sdts
	$(MAKE) $*
	cd ..\tiger
	$(MAKE) $*
	cd ..\s57
	$(MAKE) $*
	cd ..\mitab
	$(MAKE) $*
	cd ..
	lib /out:ogrsf_frmts.lib generic\*.obj shape\*.obj ntf\*.obj \
				 sdts\*.obj s57\*.obj tiger\*.obj \
				 mitab\*.obj \
				 ..\..\frmts\iso8211\*.obj \
				 ..\..\frmts\sdts\sdtsattrreader.obj \
				 ..\..\frmts\sdts\sdtscatd.obj \
				 ..\..\frmts\sdts\sdtsiref.obj \
				 ..\..\frmts\sdts\sdtslib.obj \
				 ..\..\frmts\sdts\sdtslinereader.obj \
				 ..\..\frmts\sdts\sdtspointreader.obj \
				 ..\..\frmts\sdts\sdtspolygonreader.obj \
				 ..\..\frmts\sdts\sdtsrasterreader.obj \
				 ..\..\frmts\sdts\sdtstransfer.obj \
				 ..\..\frmts\sdts\sdtsindexedreader.obj \
				 ..\..\frmts\sdts\sdtsxref.obj

clean:
	del ogrsf_frmts.lib
	cd generic 
	$(MAKE) clean
	cd ..\shape 
	$(MAKE) clean
	cd ..\ntf
	$(MAKE) clean
	cd ..\sdts
	$(MAKE) clean
	cd ..\tiger
	$(MAKE) clean
	cd ..\s57
	$(MAKE) clean
	cd ..\mitab
	$(MAKE) clean
