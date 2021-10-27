.. _csharp_raster:

================================================================================
C# Raster Interface
================================================================================

The GDAL C# interface supports transferring raster data between the C# application and GDAL.

The various :file:`Band.ReadRaster`, :file:`Band.WriteRaster`, :file:`Dataset.ReadRaster`, :file:`Dataset.WriteRaster` overloads
are involved in transferring raster data between the managed and the unmanaged parts of the application.

This page will summarize the main aspects of raster data handling related exclusively to the C# interface.

The :file:`Band` class contains the following :file:`ReadRaster`/:file:`WriteRaster` overloads:

.. code-block:: C#

    public CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, byte[] buffer, 
        int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace){}
        
    public CPLErr WriteRaster(int xOff, int yOff, int xSize, int ySize, byte[] buffer, 
        int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace){}
        
    public CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, short[] buffer, 
        int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace){}
        
    public CPLErr WriteRaster(int xOff, int yOff, int xSize, int ySize, short[] buffer, 
        int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace){}
        
    public CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, int[] buffer, 
        int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace){}
        
    public CPLErr WriteRaster(int xOff, int yOff, int xSize, int ySize, int[] buffer, 
        int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace){}
        
    public CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, float[] buffer, 
        int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace){}
        
    public CPLErr WriteRaster(int xOff, int yOff, int xSize, int ySize, float[] buffer, 
        int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace){}
        
    public CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, double[] buffer, 
        int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace){}
        
    public CPLErr WriteRaster(int xOff, int yOff, int xSize, int ySize, double[] buffer, 
        int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace){}
        
    public CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, IntPtr buffer, i
        nt buf_xSize, int buf_ySize, DataType buf_type, int pixelSpace, int lineSpace){}
  
    public CPLErr WriteRaster(int xOff, int yOff, int xSize, int ySize, IntPtr buffer, 
        int buf_xSize, int buf_ySize, DataType buf_type, int pixelSpace, int lineSpace){}

The only difference between these functions is the actual type of the buffer parameter.
The last 2 overloads are the generic overloads and the caller should write the proper marshaling
code for the buffer holding the raster data. The overloads that have a C# array as the buffer parameter
implement the proper marshaling code for the caller.

Reading the raster image
------------------------

When reading raster data from GDAL, the user will probably create a .NET image to hold C# representation of the data.
The raster data can be read directly or in a buffered fashion.

Using the buffered read approach
++++++++++++++++++++++++++++++++

When reading the image this way the C# API will copy the image data between the C and the C# arrays:

.. code-block:: C#

    // Creating a Bitmap to store the GDAL image in
    Bitmap bitmap = new Bitmap(width, height, PixelFormat.Format32bppRgb);
    // Creating a C# array to hold the image data
    byte[] r = new byte[width * height];
    band.ReadRaster(0, 0, width, height, r, width, height, 0, 0);
    // Copying the pixels into the C# bitmap
    int i, j;
    for (i = 0; i< width; i++) 
    {
        for (j=0; j<height; j++)
        {
            Color newColor = Color.FromArgb(Convert.ToInt32(r[i+j*width]),Convert.ToInt32(r[i+j*width]), Convert.ToInt32(r[i+j*width]));
                    bitmap.SetPixel(i, j, newColor);
        }
    }

In this case the interface implementation uses an internally created unmanaged array to transfer the data between the C and C++ part of the code, like:

.. code-block:: C#

    public CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, byte[] buffer, int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace) {
        CPLErr retval;
        IntPtr ptr = Marshal.AllocHGlobal(buf_xSize * buf_ySize * Marshal.SizeOf(buffer[0]));
        try {
            retval = ReadRaster(xOff, yOff, xSize, ySize, ptr, buf_xSize, buf_ySize, DataType.GDT_Byte, pixelSpace, lineSpace);
            Marshal.Copy(ptr, buffer, 0, buf_xSize * buf_ySize);
        } finally {
            Marshal.FreeHGlobal(ptr);
        }
        GC.KeepAlive(this);
        return retval;
    }

Using the direct read approach
++++++++++++++++++++++++++++++

Raster data can be read into the C# bitmap directly using the following approach:

.. code-block:: C#

    // Creating a Bitmap to store the GDAL image in
    Bitmap bitmap = new Bitmap(width, height, PixelFormat.Format8bppIndexed);
    // Obtaining the bitmap buffer
    BitmapData bitmapData = bitmap.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.ReadWrite, PixelFormat.Format8bppIndexed);
    try 
    {
        int stride = bitmapData.Stride;
        IntPtr buf = bitmapData.Scan0;
        band.ReadRaster(0, 0, width, height, buf, width, height, DataType.GDT_Byte, 1, stride);
    }
    finally 
    {
        bitmap.UnlockBits(bitmapData);
    }

This approach is more performant than the previous since there's no need to allocate an intermediary array for transferring the data.

Using /unsafe code and the fixed statement
++++++++++++++++++++++++++++++++++++++++++

In the previous examples the programmer could ignore bothering with implementing the marshaling code for the raster arrays.
Both of the examples prevent the garbage collector from relocating the array during the execution of the P/Invoke call.
Without using an intermediary array the programmer can also use the following method to read the raster data:

.. code-block:: C#

    byte[] buffer = new byte[width * height];
    fixed (IntPtr ptr = buffer) {
    band.ReadRaster(0, 0, width, height, ptr, width, height, 1, width);
    }

When using this approach the application must be compiled using the :program:`/unsafe` command line option.

Using indexed / grayscale images
++++++++++++++++++++++++++++++++

The :file:`PaletteInterp` enumeration can be used to distinguish between the various type of the image color interpretations.

.. code-block:: C#

    Band band = dataset.GetRasterBand(1);
    ColorTable ct = band.GetRasterColorTable();
    if (ct.GetPaletteInterpretation() != PaletteInterp.GPI_RGB)
    {
        Console.WriteLine("   This raster band has RGB palette interpretation!");
    }

When reading images with indexed color representations, the programmer might have to do some extra work copying the palette over:

.. code-block:: C#

    // Get the GDAL Band objects from the Dataset
    Band band = dataset.GetRasterBand(1);
    ColorTable ct = band.GetRasterColorTable();
    // Create a Bitmap to store the GDAL image in
    Bitmap bitmap = new Bitmap(width, height, PixelFormat.Format8bppIndexed);
    // Obtaining the bitmap buffer
    BitmapData bitmapData = bitmap.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.ReadWrite, PixelFormat.Format8bppIndexed);
    try 
        {
            int iCol = ct.GetCount();
            ColorPalette pal = bitmap.Palette;
            for (int i = 0; i < iCol; i++)
            {
                ColorEntry ce = ct.GetColorEntry(i);
                pal.Entries[i] = Color.FromArgb(ce.c4, ce.c1, ce.c2, ce.c3);
            }
            bitmap.Palette = pal;
                
            int stride = bitmapData.Stride;
            IntPtr buf = bitmapData.Scan0;

            band.ReadRaster(0, 0, width, height, buf, width, height, DataType.GDT_Byte, 1, stride);
            }
            finally 
            {
                bitmap.UnlockBits(bitmapData);
            }
        }

When reading grayscale images, the programmer should create a sufficient palette for the .NET image.

.. code-block:: C#

    // Get the GDAL Band objects from the Dataset
    Band band = ds.GetRasterBand(1);
    // Create a Bitmap to store the GDAL image in
    Bitmap bitmap = new Bitmap(width, height, PixelFormat.Format8bppIndexed);
    // Obtaining the bitmap buffer
    BitmapData bitmapData = bitmap.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.ReadWrite, PixelFormat.Format8bppIndexed);
    try 
        {
            ColorPalette pal = bitmap.Palette; 
            for(int i = 0; i < 256; i++) 
                pal.Entries[i] = Color.FromArgb( 255, i, i, i ); 
            bitmap.Palette = pal;
                
            int stride = bitmapData.Stride;
            IntPtr buf = bitmapData.Scan0;

            band.ReadRaster(0, 0, width, height, buf, width, height, DataType.GDT_Byte, 1, stride);
        }
        finally 
        {
            bitmap.UnlockBits(bitmapData);
        }

Related C# examples
+++++++++++++++++++

The following examples demonstrate the usage of the GDAL raster operations mentioned previously:

* `GDALRead.cs <https://github.com/OSGeo/gdal/blob/master/gdal/swig/csharp/apps/GDALRead.cs>`__
* `GDALReadDirect.cs <https://github.com/OSGeo/gdal/blob/master/gdal/swig/csharp/apps/GDALReadDirect.cs>`__
* `GDALWrite.cs <https://github.com/OSGeo/gdal/blob/master/gdal/swig/csharp/apps/GDALReadDirect.cs>`__

.. note:: This document was amended from the previous version at `https://trac.osgeo.org/gdal/wiki/GdalOgrCsharpRaster <https://trac.osgeo.org/gdal/wiki/GdalOgrCsharpRaster>`__