default:
	cd generic
	$(MAKE) $*
	cd ..\shape 
	$(MAKE) $*
	cd ..
	lib /out:ogrsf_frmts.lib generic\*.obj shape\*.obj

clean:
	del ogrsf_frmts.lib
	cd generic 
	$(MAKE) clean
	cd ..\shape 
	$(MAKE) clean
