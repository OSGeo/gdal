.. _raster_api_tut:

================================================================================
Raster API tutorial
================================================================================

Opening the File
----------------

Before opening a GDAL supported raster datastore it is necessary to register drivers. There is a driver for each supported format. Normally this is accomplished with the :cpp:func:`GDALAllRegister` function which attempts to register all known drivers, including those auto-loaded from .so files using :cpp:func:`GDALDriverManager::AutoLoadDrivers`. If for some applications it is necessary to limit the set of drivers it may be helpful to review the code from gdalallregister.cpp. Python automatically calls GDALAllRegister() when the gdal module is imported.

Once the drivers are registered, the application should call the free standing :cpp:func:`GDALOpen` function to open a dataset, passing the name of the dataset and the access desired (GA_ReadOnly or GA_Update).

In C++:

.. code-block:: c++

    #include "gdal_priv.h"
    #include "cpl_conv.h" // for CPLMalloc()
    int main()
    {
        GDALDataset  *poDataset;
        GDALAllRegister();
        poDataset = (GDALDataset *) GDALOpen( pszFilename, GA_ReadOnly );
        if( poDataset == NULL )
        {
            ...;
        }

In C:

.. code-block:: c

    #include "gdal.h"
    #include "cpl_conv.h" /* for CPLMalloc() */
    int main()
    {
        GDALDatasetH  hDataset;
        GDALAllRegister();
        hDataset = GDALOpen( pszFilename, GA_ReadOnly );
        if( hDataset == NULL )
        {
            ...;
        }


In Python:

.. code-block:: python

    from osgeo import gdal
    dataset = gdal.Open(filename, gdal.GA_ReadOnly)
    if not dataset:
        ...

Note that if :cpp:func:`GDALOpen` returns NULL it means the open failed, and that an error messages will already have been emitted via :cpp:func:`CPLError`. If you want to control how errors are reported to the user review the :cpp:func:`CPLError` documentation. Generally speaking all of GDAL uses :cpp:func:`CPLError` for error reporting. Also, note that pszFilename need not actually be the name of a physical file (though it usually is). It's interpretation is driver dependent, and it might be an URL, a filename with additional parameters added at the end controlling the open or almost anything. Please try not to limit GDAL file selection dialogs to only selecting physical files.

Getting Dataset Information
---------------------------

As described in the :ref:`raster_data_model`, a :cpp:class:`GDALDataset` contains a list of raster bands, all pertaining to the same area, and having the same resolution. It also has metadata, a coordinate system, a georeferencing transform, size of raster and various other information.

In the particular, but common, case of a "north up" image without any rotation or shearing, the georeferencing transform :ref:`geotransforms_tut` takes the following form :

.. code-block:: c

    adfGeoTransform[0] /* top left x */
    adfGeoTransform[1] /* w-e pixel resolution */
    adfGeoTransform[2] /* 0 */
    adfGeoTransform[3] /* top left y */
    adfGeoTransform[4] /* 0 */
    adfGeoTransform[5] /* n-s pixel resolution (negative value) */

In the general case, this is an affine transform.

If we wanted to print some general information about the dataset we might do the following:

In C++:

.. code-block:: c++

    double        adfGeoTransform[6];
    printf( "Driver: %s/%s\n",
            poDataset->GetDriver()->GetDescription(),
            poDataset->GetDriver()->GetMetadataItem( GDAL_DMD_LONGNAME ) );
    printf( "Size is %dx%dx%d\n",
            poDataset->GetRasterXSize(), poDataset->GetRasterYSize(),
            poDataset->GetRasterCount() );
    if( poDataset->GetProjectionRef()  != NULL )
        printf( "Projection is `%s'\n", poDataset->GetProjectionRef() );
    if( poDataset->GetGeoTransform( adfGeoTransform ) == CE_None )
    {
        printf( "Origin = (%.6f,%.6f)\n",
                adfGeoTransform[0], adfGeoTransform[3] );
        printf( "Pixel Size = (%.6f,%.6f)\n",
                adfGeoTransform[1], adfGeoTransform[5] );
    }

In C:

.. code-block:: c

    GDALDriverH   hDriver;
    double        adfGeoTransform[6];
    hDriver = GDALGetDatasetDriver( hDataset );
    printf( "Driver: %s/%s\n",
            GDALGetDriverShortName( hDriver ),
            GDALGetDriverLongName( hDriver ) );
    printf( "Size is %dx%dx%d\n",
            GDALGetRasterXSize( hDataset ),
            GDALGetRasterYSize( hDataset ),
            GDALGetRasterCount( hDataset ) );
    if( GDALGetProjectionRef( hDataset ) != NULL )
        printf( "Projection is `%s'\n", GDALGetProjectionRef( hDataset ) );
    if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None )
    {
        printf( "Origin = (%.6f,%.6f)\n",
                adfGeoTransform[0], adfGeoTransform[3] );
        printf( "Pixel Size = (%.6f,%.6f)\n",
                adfGeoTransform[1], adfGeoTransform[5] );
    }

In Python:

.. code-block:: python

    print("Driver: {}/{}".format(dataset.GetDriver().ShortName,
                                dataset.GetDriver().LongName))
    print("Size is {} x {} x {}".format(dataset.RasterXSize,
                                        dataset.RasterYSize,
                                        dataset.RasterCount))
    print("Projection is {}".format(dataset.GetProjection()))
    geotransform = dataset.GetGeoTransform()
    if geotransform:
        print("Origin = ({}, {})".format(geotransform[0], geotransform[3]))
        print("Pixel Size = ({}, {})".format(geotransform[1], geotransform[5]))

Fetching a Raster Band
----------------------

At this time access to raster data via GDAL is done one band at a time. Also, there is metadata, block sizes, color tables, and various other information available on a band by band basis. The following codes fetches a :cpp:class:`GDALRasterBand` object from the dataset (numbered 1 through :cpp:func:`GDALRasterBand::GetRasterCount`) and displays a little information about it.

In C++:

.. code-block:: c++

    GDALRasterBand  *poBand;
    int             nBlockXSize, nBlockYSize;
    int             bGotMin, bGotMax;
    double          adfMinMax[2];
    poBand = poDataset->GetRasterBand( 1 );
    poBand->GetBlockSize( &nBlockXSize, &nBlockYSize );
    printf( "Block=%dx%d Type=%s, ColorInterp=%s\n",
            nBlockXSize, nBlockYSize,
            GDALGetDataTypeName(poBand->GetRasterDataType()),
            GDALGetColorInterpretationName(
                poBand->GetColorInterpretation()) );
    adfMinMax[0] = poBand->GetMinimum( &bGotMin );
    adfMinMax[1] = poBand->GetMaximum( &bGotMax );
    if( ! (bGotMin && bGotMax) )
        GDALComputeRasterMinMax((GDALRasterBandH)poBand, TRUE, adfMinMax);
    printf( "Min=%.3fd, Max=%.3f\n", adfMinMax[0], adfMinMax[1] );
    if( poBand->GetOverviewCount() > 0 )
        printf( "Band has %d overviews.\n", poBand->GetOverviewCount() );
    if( poBand->GetColorTable() != NULL )
        printf( "Band has a color table with %d entries.\n",
                poBand->GetColorTable()->GetColorEntryCount() );


In C:

.. code-block:: c

    GDALRasterBandH hBand;
    int             nBlockXSize, nBlockYSize;
    int             bGotMin, bGotMax;
    double          adfMinMax[2];
    hBand = GDALGetRasterBand( hDataset, 1 );
    GDALGetBlockSize( hBand, &nBlockXSize, &nBlockYSize );
    printf( "Block=%dx%d Type=%s, ColorInterp=%s\n",
            nBlockXSize, nBlockYSize,
            GDALGetDataTypeName(GDALGetRasterDataType(hBand)),
            GDALGetColorInterpretationName(
                GDALGetRasterColorInterpretation(hBand)) );
    adfMinMax[0] = GDALGetRasterMinimum( hBand, &bGotMin );
    adfMinMax[1] = GDALGetRasterMaximum( hBand, &bGotMax );
    if( ! (bGotMin && bGotMax) )
        GDALComputeRasterMinMax( hBand, TRUE, adfMinMax );
    printf( "Min=%.3fd, Max=%.3f\n", adfMinMax[0], adfMinMax[1] );
    if( GDALGetOverviewCount(hBand) > 0 )
        printf( "Band has %d overviews.\n", GDALGetOverviewCount(hBand));
    if( GDALGetRasterColorTable( hBand ) != NULL )
        printf( "Band has a color table with %d entries.\n",
                GDALGetColorEntryCount(
                    GDALGetRasterColorTable( hBand ) ) );

In Python:

.. code-block:: python

    band = dataset.GetRasterBand(1)
    print("Band Type={}".format(gdal.GetDataTypeName(band.DataType)))

    min = band.GetMinimum()
    max = band.GetMaximum()
    if not min or not max:
        (min,max) = band.ComputeRasterMinMax(True)
    print("Min={:.3f}, Max={:.3f}".format(min,max))

    if band.GetOverviewCount() > 0:
        print("Band has {} overviews".format(band.GetOverviewCount()))

    if band.GetRasterColorTable():
        print("Band has a color table with {} entries".format(band.GetRasterColorTable().GetCount()))

Reading Raster Data
-------------------

There are a few ways to read raster data, but the most common is via the :cpp:func:`GDALRasterBand::RasterIO` method. This method will automatically take care of data type conversion, up/down sampling and windowing. The following code will read the first scanline of data into a similarly sized buffer, converting it to floating point as part of the operation.

In C++:

.. code-block:: c++

    float *pafScanline;
    int   nXSize = poBand->GetXSize();
    pafScanline = (float *) CPLMalloc(sizeof(float)*nXSize);
    poBand->RasterIO( GF_Read, 0, 0, nXSize, 1,
                    pafScanline, nXSize, 1, GDT_Float32,
                    0, 0 );

The pafScanline buffer should be freed with CPLFree() when it is no longer used.

In C:

.. code-block:: c

    float *pafScanline;
    int   nXSize = GDALGetRasterBandXSize( hBand );
    pafScanline = (float *) CPLMalloc(sizeof(float)*nXSize);
    GDALRasterIO( hBand, GF_Read, 0, 0, nXSize, 1,
                pafScanline, nXSize, 1, GDT_Float32,
                0, 0 );

The pafScanline buffer should be freed with CPLFree() when it is no longer used.

In Python:

.. code-block:: python

    scanline = band.ReadRaster(xoff=0, yoff=0,
                            xsize=band.XSize, ysize=1,
                            buf_xsize=band.XSize, buf_ysize=1,
                            buf_type=gdal.GDT_Float32)

Note that the returned scanline is of type string, and contains xsize*4 bytes of raw binary floating point data. This can be converted to Python values using the struct module from the standard library:

.. code-block:: python

    import struct
    tuple_of_floats = struct.unpack('f' * b2.XSize, scanline)

The RasterIO call takes the following arguments.

.. code-block:: c++

    CPLErr GDALRasterBand::RasterIO( GDALRWFlag eRWFlag,
                                    int nXOff, int nYOff, int nXSize, int nYSize,
                                    void * pData, int nBufXSize, int nBufYSize,
                                    GDALDataType eBufType,
                                    int nPixelSpace,
                                    int nLineSpace )

Note that the same RasterIO() call is used to read, or write based on the setting of eRWFlag (either GF_Read or GF_Write). The nXOff, nYOff, nXSize, nYSize argument describe the window of raster data on disk to read (or write). It doesn't have to fall on tile boundaries though access may be more efficient if it does.

The pData is the memory buffer the data is read into, or written from. It's real type must be whatever is passed as eBufType, such as GDT_Float32, or GDT_Byte. The RasterIO() call will take care of converting between the buffer's data type and the data type of the band. Note that when converting floating point data to integer RasterIO() rounds down, and when converting source values outside the legal range of the output the nearest legal value is used. This implies, for instance, that 16bit data read into a GDT_Byte buffer will map all values greater than 255 to 255, the data is not scaled!

The nBufXSize and nBufYSize values describe the size of the buffer. When loading data at full resolution this would be the same as the window size. However, to load a reduced resolution overview this could be set to smaller than the window on disk. In this case the RasterIO() will utilize overviews to do the IO more efficiently if the overviews are suitable.

The nPixelSpace, and nLineSpace are normally zero indicating that default values should be used. However, they can be used to control access to the memory data buffer, allowing reading into a buffer containing other pixel interleaved data for instance.

Closing the Dataset
-------------------

Please keep in mind that :cpp:class:`GDALRasterBand` objects are owned by their dataset, and they should never be destroyed with the C++ delete operator. :cpp:class:`GDALDataset`'s can be closed by calling :cpp:func:`GDALClose` (it is NOT recommended to use the delete operator on a GDALDataset for Windows users because of known issues when allocating and freeing memory across module boundaries. See the relevant topic on the FAQ). Calling GDALClose will result in proper cleanup, and flushing of any pending writes. Forgetting to call GDALClose on a dataset opened in update mode in a popular format like GTiff will likely result in being unable to open it afterwards.

Techniques for Creating Files
-----------------------------

New files in GDAL supported formats may be created if the format driver supports creation. There are two general techniques for creating files, using CreateCopy() and Create(). The CreateCopy method involves calling the CreateCopy() method on the format driver, and passing in a source dataset that should be copied. The Create method involves calling the Create() method on the driver, and then explicitly writing all the metadata, and raster data with separate calls. All drivers that support creating new files support the CreateCopy() method, but only a few support the Create() method.

To determine if a particular format supports Create or CreateCopy it is possible to check the DCAP_CREATE and DCAP_CREATECOPY metadata on the format driver object. Ensure that :cpp:func:`GDALAllRegister` has been called before calling :cpp:func:`GDALDriverManager::GetDriverByName`. In this example we fetch a driver, and determine whether it supports Create() and/or CreateCopy().

In C++:

.. code-block:: c++

    #include "cpl_string.h"
    ...
        const char *pszFormat = "GTiff";
        GDALDriver *poDriver;
        char **papszMetadata;
        poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
        if( poDriver == NULL )
            exit( 1 );
        papszMetadata = poDriver->GetMetadata();
        if( CSLFetchBoolean( papszMetadata, GDAL_DCAP_CREATE, FALSE ) )
            printf( "Driver %s supports Create() method.\n", pszFormat );
        if( CSLFetchBoolean( papszMetadata, GDAL_DCAP_CREATECOPY, FALSE ) )
            printf( "Driver %s supports CreateCopy() method.\n", pszFormat );

In C:

.. code-block:: c

        #include "cpl_string.h"
        ...
        const char *pszFormat = "GTiff";
        GDALDriverH hDriver = GDALGetDriverByName( pszFormat );
        char **papszMetadata;
        if( hDriver == NULL )
            exit( 1 );
        papszMetadata = GDALGetMetadata( hDriver, NULL );
        if( CSLFetchBoolean( papszMetadata, GDAL_DCAP_CREATE, FALSE ) )
            printf( "Driver %s supports Create() method.\n", pszFormat );
        if( CSLFetchBoolean( papszMetadata, GDAL_DCAP_CREATECOPY, FALSE ) )
            printf( "Driver %s supports CreateCopy() method.\n", pszFormat );

In Python:

.. code-block:: python

    fileformat = "GTiff"
    driver = gdal.GetDriverByName(fileformat)
    metadata = driver.GetMetadata()
    if metadata.get(gdal.DCAP_CREATE) == "YES":
        print("Driver {} supports Create() method.".format(fileformat))

    if metadata.get(gdal.DCAP_CREATECOPY) == "YES":
        print("Driver {} supports CreateCopy() method.".format(fileformat))

Note that a number of drivers are read-only and won't support Create() or CreateCopy().

Using CreateCopy()
------------------

The :cpp:func:`GDALDriver::CreateCopy` method can be used fairly simply as most information is collected from the source dataset. However, it includes options for passing format specific creation options, and for reporting progress to the user as a long dataset copy takes place. A simple copy from the a file named pszSrcFilename, to a new file named pszDstFilename using default options on a format whose driver was previously fetched might look like this:

In C++:

.. code-block:: c++

    GDALDataset *poSrcDS =
    (GDALDataset *) GDALOpen( pszSrcFilename, GA_ReadOnly );
    GDALDataset *poDstDS;
    poDstDS = poDriver->CreateCopy( pszDstFilename, poSrcDS, FALSE,
                                    NULL, NULL, NULL );
    /* Once we're done, close properly the dataset */
    if( poDstDS != NULL )
        GDALClose( (GDALDatasetH) poDstDS );
    GDALClose( (GDALDatasetH) poSrcDS );

In C:

.. code-block:: c

    GDALDatasetH hSrcDS = GDALOpen( pszSrcFilename, GA_ReadOnly );
    GDALDatasetH hDstDS;
    hDstDS = GDALCreateCopy( hDriver, pszDstFilename, hSrcDS, FALSE,
                            NULL, NULL, NULL );
    /* Once we're done, close properly the dataset */
    if( hDstDS != NULL )
        GDALClose( hDstDS );
    GDALClose(hSrcDS);

In Python:

.. code-block:: python

    src_ds = gdal.Open(src_filename)
    dst_ds = driver.CreateCopy(dst_filename, src_ds, strict=0)
    # Once we're done, close properly the dataset
    dst_ds = None
    src_ds = None

Note that the CreateCopy() method returns a writable dataset, and that it must be closed properly to complete writing and flushing the dataset to disk. In the Python case this occurs automatically when "dst_ds" goes out of scope. The FALSE (or 0) value used for the bStrict option just after the destination filename in the CreateCopy() call indicates that the CreateCopy() call should proceed without a fatal error even if the destination dataset cannot be created to exactly match the input dataset. This might be because the output format does not support the pixel datatype of the input dataset, or because the destination cannot support writing georeferencing for instance.

A more complex case might involve passing creation options, and using a predefined progress monitor like this:

In C++:

.. code-block:: c++

        #include "cpl_string.h"
        ...
        char **papszOptions = NULL;
        papszOptions = CSLSetNameValue( papszOptions, "TILED", "YES" );
        papszOptions = CSLSetNameValue( papszOptions, "COMPRESS", "PACKBITS" );
        poDstDS = poDriver->CreateCopy( pszDstFilename, poSrcDS, FALSE,
                                        papszOptions, GDALTermProgress, NULL );
        /* Once we're done, close properly the dataset */
        if( poDstDS != NULL )
            GDALClose( (GDALDatasetH) poDstDS );
        CSLDestroy( papszOptions );

In C:

.. code-block:: c

        #include "cpl_string.h"
        ...
        char **papszOptions = NULL;
        papszOptions = CSLSetNameValue( papszOptions, "TILED", "YES" );
        papszOptions = CSLSetNameValue( papszOptions, "COMPRESS", "PACKBITS" );
        hDstDS = GDALCreateCopy( hDriver, pszDstFilename, hSrcDS, FALSE,
                                papszOptions, GDALTermProgres, NULL );
        /* Once we're done, close properly the dataset */
        if( hDstDS != NULL )
            GDALClose( hDstDS );
        CSLDestroy( papszOptions );

In Python:

.. code-block:: python

    src_ds = gdal.Open(src_filename)
    dst_ds = driver.CreateCopy(dst_filename, src_ds, strict=0,
                            options=["TILED=YES", "COMPRESS=PACKBITS"])
    # Once we're done, close properly the dataset
    dst_ds = None
    src_ds = None

Using Create()
--------------

For situations in which you are not just exporting an existing file to a new file, it is generally necessary to use the :cpp:func:`GDALDriver::Create` method (though some interesting options are possible through use of virtual files or in-memory files). The Create() method takes an options list much like CreateCopy(), but the image size, number of bands and band type must be provided explicitly.

In C++:

.. code-block:: c++

    GDALDataset *poDstDS;
    char **papszOptions = NULL;
    poDstDS = poDriver->Create( pszDstFilename, 512, 512, 1, GDT_Byte,
                                papszOptions );

In C:

.. code-block:: c

    GDALDatasetH hDstDS;
    char **papszOptions = NULL;
    hDstDS = GDALCreate( hDriver, pszDstFilename, 512, 512, 1, GDT_Byte,
                        papszOptions );

In Python:

.. code-block:: python

    dst_ds = driver.Create(dst_filename, xsize=512, ysize=512,
                        bands=1, eType=gdal.GDT_Byte)

Once the dataset is successfully created, all appropriate metadata and raster data must be written to the file. What this is will vary according to usage, but a simple case with a projection, geotransform and raster data is covered here.

In C++:

.. code-block:: c++

    double adfGeoTransform[6] = { 444720, 30, 0, 3751320, 0, -30 };
    OGRSpatialReference oSRS;
    char *pszSRS_WKT = NULL;
    GDALRasterBand *poBand;
    GByte abyRaster[512*512];
    poDstDS->SetGeoTransform( adfGeoTransform );
    oSRS.SetUTM( 11, TRUE );
    oSRS.SetWellKnownGeogCS( "NAD27" );
    oSRS.exportToWkt( &pszSRS_WKT );
    poDstDS->SetProjection( pszSRS_WKT );
    CPLFree( pszSRS_WKT );
    poBand = poDstDS->GetRasterBand(1);
    poBand->RasterIO( GF_Write, 0, 0, 512, 512,
                    abyRaster, 512, 512, GDT_Byte, 0, 0 );
    /* Once we're done, close properly the dataset */
    GDALClose( (GDALDatasetH) poDstDS );

In C:

.. code-block:: c

    double adfGeoTransform[6] = { 444720, 30, 0, 3751320, 0, -30 };
    OGRSpatialReferenceH hSRS;
    char *pszSRS_WKT = NULL;
    GDALRasterBandH hBand;
    GByte abyRaster[512*512];
    GDALSetGeoTransform( hDstDS, adfGeoTransform );
    hSRS = OSRNewSpatialReference( NULL );
    OSRSetUTM( hSRS, 11, TRUE );
    OSRSetWellKnownGeogCS( hSRS, "NAD27" );
    OSRExportToWkt( hSRS, &pszSRS_WKT );
    OSRDestroySpatialReference( hSRS );
    GDALSetProjection( hDstDS, pszSRS_WKT );
    CPLFree( pszSRS_WKT );
    hBand = GDALGetRasterBand( hDstDS, 1 );
    GDALRasterIO( hBand, GF_Write, 0, 0, 512, 512,
                abyRaster, 512, 512, GDT_Byte, 0, 0 );
    /* Once we're done, close properly the dataset */
    GDALClose( hDstDS );

In Python:

.. code-block:: python

    from osgeo import osr
    import numpy
    dst_ds.SetGeoTransform([444720, 30, 0, 3751320, 0, -30])
    srs = osr.SpatialReference()
    srs.SetUTM(11, 1)
    srs.SetWellKnownGeogCS("NAD27")
    dst_ds.SetProjection(srs.ExportToWkt())
    raster = numpy.zeros((512, 512), dtype=numpy.uint8)
    dst_ds.GetRasterBand(1).WriteArray(raster)
    # Once we're done, close properly the dataset
    dst_ds = None

