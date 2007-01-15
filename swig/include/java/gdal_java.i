/******************************************************************************
 * $Id$
 *
 * Name:     gdal_java.i
 * Project:  GDAL SWIG Interface
 * Purpose:  Typemaps for Java bindings
 * Author:   Benjamin Collins, The MITRE Corporation
 *
 *
 * $Log$
 * Revision 1.2  2006/02/16 17:21:12  collinsb
 * Updates to Java bindings to keep the code from halting execution if the native libraries cannot be found.
 *
 * Revision 1.1  2006/02/02 20:56:07  collinsb
 * Added Java specific typemap code
 *
 *
*/

%pragma(java) jniclasscode=%{
  private static boolean available = false;

  static {
    try {
      System.loadLibrary("gdaljni");
      available = true;
    } catch (UnsatisfiedLinkError e) {
      available = false;
      System.err.println("Native library load failed.");
      System.err.println(e);
    }
  }
  
  public static boolean isAvailable() {
    return available;
  }
%}


%extend GDALRasterBandShadow {
  CPLErr ReadRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *buf )
{

  return GDALRasterIO( self, GF_Read, xoff, yoff, xsize, ysize,
                                 buf, buf_xsize, buf_ysize,
                                 buf_type, 0, 0 );

}

  CPLErr WriteRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *buf )
{

  return GDALRasterIO( self, GF_Write, xoff, yoff, xsize, ysize,
                                 buf, buf_xsize, buf_ysize,
                                 buf_type, 0, 0 );

}
} /* extend */

%typemap(javaimports) GDALColorTable %{
/* imports for getIndexColorModel */
import java.awt.image.IndexColorModel;
import java.awt.Color;
%}

%typemap(javacode) GDALColorTable %{
/* convienance method */
  public IndexColorModel getIndexColorModel(int bits) {
    int size = GetCount();
    byte[] reds = new byte[size];
    byte[] greens = new byte[size];
    byte[] blues = new byte[size];
    byte[] alphas = new byte[size];

    Color entry = null;
    for(int i = 0; i < size; i++) {
      entry = GetColorEntry(i);
      reds[i] = (byte)entry.getRed();
      greens[i] = (byte)entry.getGreen();
      blues[i] = (byte)entry.getBlue();
      byte alpha = (byte)entry.getAlpha();
      alphas[i] = (alpha != -1) ? alpha : 0;
    }
    return new IndexColorModel(bits, size, reds, greens, blues, alphas);
  }
%}

%include typemaps_java.i
