
EXTRAFLAGS =	-I$(VCDIR)\ATL\Include \
		-I$(MSDASDK)\include\oledb -I.. /W2 \
		-I..\ogrsf_frmts 

GDAL_ROOT = 	..\..

!INCLUDE $(GDAL_ROOT)/nmake.opt
all:
	cd softmap
	$(MAKE) 
	cd ..

clean:
	cd softmap
	$(MAKE) clean
	cd ..
