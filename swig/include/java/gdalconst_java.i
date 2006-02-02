/******************************************************************************
 * $Id$
 *
 * Name:     gdalconst_java.i
 * Project:  GDAL SWIG Interface
 * Purpose:  Typemaps for Java bindings
 * Author:   Benjamin Collins, The MITRE Corporation
 *
 *
 * $Log$
 * Revision 1.1  2006/02/02 20:56:07  collinsb
 * Added Java specific typemap code
 *
 *
*/

%pragma(java) jniclasscode=%{
 static {
    try {
        System.loadLibrary("gdalconstjni");
    } catch (UnsatisfiedLinkError e) {
    	System.err.println("Native library load failed.");
    	System.err.println(e);
    	System.exit(1);
    }
 }
%}


%include typemaps_java.i
