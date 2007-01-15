	GDAL Visual Basic 6 Bindings
	============================

Preliminary notes on VB6 bindings. 

A VB6 project should include the following files from this directory for
GDAL access:

  GDAL.bas: Base GDAL access.
  GDALCore.bas: External DLL entry point declarations and a few private
                support functions. 
  GDALDriver.cls
  GDALDataset.cls
  GDALRasterBand.cls
  GDALColorTable.cls

A small (and not very fancy) test application demonstrating some of the use
of the GDAL VB6 classes is available in the "test" directory.

Quick Test Run
--------------

1) VB6 needs to be launched in such a way that it can find the gdal12vb.dll.
   This could be accomplished by having VB6 run in the same directory as the
   DLL, or by copying the DLL into windows\system32 or by editing GDALCore.bas
   and changing all occurances of "gdal12vb.dll" to a full path to the DLL. 

   But for the test app to run, you need to have the sample files in the
   current directory, so you might as well just make a shortcut copy of your
   VB6 launch icon, and set the current working directory to this directory.

2) Launch VB6. 

3) Open test\GDALVB6Test project. 

4) Hit F5 to run it. 

5) Select Tools->List Drivers menu item to get a list of configured drivers.
   If this works, the DLL has been found, and calls to it are working fine.

6) File->Exit, and F5 to restart.  This gives a clear form again.

7) Tools->Read Test: This reads a bunch of info from the test file, and
   dumps it in the form.  Shows access to metadata, color table, etc. 

8) exit and restart to clear screen.

9) Tools->Create Test: This creates an out_create.tif file using explicit
   creation and copies metadata, georeferencing and actual image data to 
   it "manually".  This shows most of the setting API is working OK. 

10) Tools->CreateCopy: Simplier option to create out.tif, demonstrates 
    simple file copying. 


Sample App Notes
----------------

The sample app is very simple, partly because I know very little about
writing VB apps, and partly because it is only intended to exercise the
API a bit.  Note, the only thing that works in the file menu is the 
File->Exit. 

To understand how things work, feel free to review the menu callbacks in 
VB6Test.frm.  Most have hardcoded filenames. 


