/******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * gdalallregister.cpp
 *
 * Main format registration function.
 * 
 * $Log$
 * Revision 1.64  2004/05/26 17:45:39  warmerda
 * added LAN format
 *
 * Revision 1.63  2004/04/05 21:30:44  warmerda
 * moved ECW down so other jpeg2000 drivers used in preference
 *
 * Revision 1.62  2004/01/07 20:06:34  warmerda
 * Added netcdf support
 *
 * Revision 1.61  2003/12/12 23:06:12  jimg
 * Added test for DODS. It's near the end, grouped with the formats that are
 * expensive to test for, and it's run only if FRMT_dods is defined.
 *
 * Revision 1.60  2003/12/11 06:18:35  warmerda
 * added BT driver
 *
 * Revision 1.59  2003/10/09 13:37:14  warmerda
 * Remove GDALRegister_AIGrid2() ... too undependable.
 *
 * Revision 1.58  2003/09/09 08:32:17  dron
 * PCIDSK added.
 *
 * Revision 1.57  2003/06/26 19:51:49  dron
 * Register HDF4 driver before ESRI to avoid confusion when ESRI .hdr exist.
 *
 * Revision 1.56  2003/04/23 12:23:17  dron
 * Added MrSID format.
 *
 * Revision 1.55  2003/02/19 07:14:56  dron
 * EFF support removed.
 *
 * Revision 1.54  2003/02/14 11:27:16  dron
 * GDALRegister_JPEG2000() moved behind of PNM and other JasPer formats.
 *
 * Revision 1.53  2002/12/03 19:03:08  dron
 * Added BMP driver.
 *
 * Revision 1.52  2002/12/03 04:42:35  warmerda
 * added NITF
 *
 * Revision 1.51  2002/11/05 06:20:07  warmerda
 * hacks for JP2KAK support
 *
 * Revision 1.50  2002/10/21 18:03:22  warmerda
 * added AutoSkipDrivers() call
 *
 * Revision 1.49  2002/10/10 10:43:37  dron
 * Fix for buiding GDAL with JasPer software under Windows.
 *
 * Revision 1.48  2002/10/05 12:05:21  dron
 * JPEG2000, L1B and HDF4 registrations moved in front of RAW (to avoid
 * problems with PAux). Removed extra JPEG2000 registration. FAST registration
 * combined with other RAW formats.
 *
 * Revision 1.47  2002/10/03 05:41:17  warmerda
 * added GSC Geogrid format
 *
 * Revision 1.46  2002/09/19 14:49:41  warmerda
 * added jpeg2000
 *
 * Revision 1.45  2002/08/13 16:59:53  dron
 * New driver: EOSAT FAST format
 *
 * Revision 1.44  2002/07/16 13:30:27  dron
 * New driver: HDF4 dataset.
 *
 * Revision 1.43  2002/06/13 09:53:45  dron
 * Registration of AVHRR L1B driver moved above of GRASS driver registartion.
 *
 * Revision 1.42  2002/05/08 16:34:26  dron
 * NOAA Polar Orbiter Dataset reader added
 *
 * Revision 1.41  2002/04/12 20:19:25  warmerda
 * added xpm
 *
 * Revision 1.40  2002/03/04 21:54:20  warmerda
 * added envi format
 *
 * Revision 1.39  2001/12/08 04:43:48  warmerda
 * added BSB registration
 *
 * Revision 1.38  2001/11/27 14:39:41  warmerda
 * added usgsdem
 *
 * Revision 1.37  2001/11/16 21:13:47  warmerda
 * added VRT dataset
 *
 * Revision 1.36  2001/09/17 18:05:20  warmerda
 * Register DOQ2 format.
 *
 * Revision 1.35  2001/07/18 04:51:56  warmerda
 * added CPL_CVSID
 *
 * Revision 1.34  2001/07/05 23:53:53  nemec
 * Add FIT file format
 *
 * Revision 1.33  2001/05/15 13:24:42  warmerda
 * added fujibas
 *
 * Revision 1.32  2001/04/02 17:11:45  warmerda
 * added ecw support
 *
 * Revision 1.31  2001/03/12 15:17:32  warmerda
 * added aaigrid
 *
 * Revision 1.30  2001/03/06 03:53:44  sperkins
 * Added FITS format support.
 *
 * Revision 1.29  2001/02/06 16:34:30  warmerda
 * moved format registration entry points to gdal_frmts.h
 *
 * Revision 1.28  2001/01/15 14:32:30  warmerda
 * added envisat
 *
 * Revision 1.27  2001/01/10 04:41:09  warmerda
 * added gif support
 *
 * Revision 1.26  2001/01/03 18:53:23  warmerda
 * Added PNM
 *
 * Revision 1.25  2000/11/27 19:03:44  warmerda
 * added JDEM format
 *
 * Revision 1.24  2000/11/16 14:48:53  warmerda
 * moved GXF down in driver ordering
 *
 * Revision 1.23  2000/09/11 13:32:26  warmerda
 * added grass
 *
 * Revision 1.22  2000/07/19 19:06:39  warmerda
 * added mem
 *
 * Revision 1.21  2000/06/20 17:36:38  warmerda
 * added eosat fast format support
 *
 * Revision 1.20  2000/04/28 20:59:03  warmerda
 * added jpeg
 *
 * Revision 1.19  2000/04/27 20:02:17  warmerda
 * added png
 *
 * Revision 1.18  2000/04/04 23:44:45  warmerda
 * also call auto register function
 *
 * Revision 1.17  2000/03/31 13:35:32  warmerda
 * added SAR_CEOS
 *
 * Revision 1.16  2000/03/07 21:34:50  warmerda
 * added HKV
 *
 * Revision 1.15  2000/03/06 21:51:09  warmerda
 * Added MFF
 *
 * Revision 1.14  2000/01/31 16:24:37  warmerda
 * added aigrid2
 *
 * Revision 1.13  1999/12/29 20:42:45  warmerda
 * Added DOQ1
 *
 * Revision 1.12  1999/10/21 13:24:52  warmerda
 * Added documentation.
 *
 * Revision 1.11  1999/08/13 03:25:58  warmerda
 * add paux
 *
 * Revision 1.10  1999/07/23 19:36:41  warmerda
 * added raw/ehdr support
 *
 * Revision 1.9  1999/06/03 14:05:33  warmerda
 * added SDTS support
 *
 * Revision 1.8  1999/05/17 01:51:43  warmerda
 * Removed unused variable.
 *
 * Revision 1.7  1999/05/13 15:28:19  warmerda
 * Added elas format.
 *
 * Revision 1.6  1999/05/05 17:32:53  warmerda
 * added ceos
 *
 * Revision 1.5  1999/02/04 22:14:46  warmerda
 * added aigrid format
 *
 * Revision 1.4  1999/01/27 18:33:45  warmerda
 * Use FMRT_ macros to test if format avail
 *
 * Revision 1.3  1999/01/11 15:30:16  warmerda
 * added OGDI
 *
 * Revision 1.2  1998/12/03 18:37:26  warmerda
 * Drop GDB, add geotiff.
 *
 * Revision 1.1  1998/11/29 22:22:14  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "gdal_frmts.h"

CPL_CVSID("$Id$");

#ifdef notdef
// we may have a use for this some day
static char *szConfiguredFormats = "GDAL_FORMATS";
#endif

/************************************************************************/
/*                          GDALAllRegister()                           */
/*                                                                      */
/*      Register all identifiably supported formats.                    */
/************************************************************************/

/**
 * Register all known configured GDAL drivers.
 *
 * This function will drive any of the following that are configured into
 * GDAL.  Possible others as well that haven't been updated in this
 * documentation:
 *
 * <ul>
 * <li> GeoTIFF (GTiff)
 * <li> Geosoft GXF (GXF)
 * <li> Erdas Imagine (HFA)
 * <li> CEOS (CEOS)
 * <li> ELAS (ELAS)
 * <li> Arc/Info Binary Grid (AIGrid)
 * <li> SDTS Raster DEM (SDTS)
 * <li> OGDI (OGDI)
 * <li> ESRI Labelled BIL (EHdr)
 * <li> PCI .aux Labelled Raw Raster (PAux)
 * </ul>
 *
 */

void GDALAllRegister()

{
    GetGDALDriverManager()->AutoLoadDrivers();

#ifdef FRMT_vrt
    GDALRegister_VRT();
#endif    

#ifdef FRMT_gdb    
    GDALRegister_GDB();
#endif    

#ifdef FRMT_gtiff    
    GDALRegister_GTiff();
#endif    

#ifdef FRMT_nitf
    GDALRegister_NITF();
#endif

#ifdef FRMT_hfa
    GDALRegister_HFA();
#endif
    
#ifdef FRMT_ceos2
    GDALRegister_SAR_CEOS();
#endif
    
#ifdef FRMT_ceos
    GDALRegister_CEOS();
#endif
    
#ifdef FRMT_elas
    GDALRegister_ELAS();
#endif
    
#ifdef FRMT_aigrid
//    GDALRegister_AIGrid2();
    GDALRegister_AIGrid();
#endif

#ifdef FRMT_aaigrid
    GDALRegister_AAIGrid();
#endif

#ifdef FRMT_sdts
    GDALRegister_SDTS();
#endif

#ifdef FRMT_ogdi
    GDALRegister_OGDI();
#endif

#ifdef FRMT_dted
    GDALRegister_DTED();
#endif

#ifdef FRMT_png
    GDALRegister_PNG();
#endif

#ifdef FRMT_jpeg
    GDALRegister_JPEG();
#endif

#ifdef FRMT_mem
    GDALRegister_MEM();
#endif

#ifdef FRMT_jdem
    GDALRegister_JDEM();
#endif

#ifdef FRMT_gif
    GDALRegister_GIF();
#endif

#ifdef FRMT_envisat
    GDALRegister_Envisat();
#endif

#ifdef FRMT_fits
    GDALRegister_FITS();
#endif

#ifdef FRMT_bsb
    GDALRegister_BSB();
#endif

#ifdef FRMT_xpm
    GDALRegister_XPM();
#endif

#ifdef FRMT_bmp
    GDALRegister_BMP();
#endif

#ifdef FRMT_pcidsk
    GDALRegister_PCIDSK();
#endif

#ifdef FRMT_netcdf
    GDALRegister_netCDF();
#endif

#ifdef FRMT_hdf4
    GDALRegister_HDF4();
    GDALRegister_HDF4Image();
#endif

#ifdef FRMT_raw
    GDALRegister_PNM();
    GDALRegister_DOQ1();
    GDALRegister_DOQ2();
    GDALRegister_ENVI();
    GDALRegister_EHdr();
    GDALRegister_PAux();
    GDALRegister_MFF();
    GDALRegister_HKV();
    GDALRegister_FujiBAS();
    GDALRegister_GSC();
    GDALRegister_FAST();
    GDALRegister_BT();
    GDALRegister_LAN();
#endif

#ifdef FRMT_jp2kak
// JPEG2000 support using Kakadu toolkit
    GDALRegister_JP2KAK();
#endif

#ifdef FRMT_jpeg2000
// JPEG2000 support using JasPer toolkit
// This one should always be placed after other JasPer supported formats,
// such as BMP or PNM. In other case we will get bad side effects.
    GDALRegister_JPEG2000();
#endif

#ifdef FRMT_ecw
    GDALRegister_ECW();
#endif

#ifdef FRMT_l1b
    GDALRegister_L1B();
#endif

#ifdef FRMT_fit
    GDALRegister_FIT();
#endif

#ifdef FRMT_mrsid
    GDALRegister_MrSID();
#endif

/* -------------------------------------------------------------------- */
/*      Our test for the following is weak or expensive so we try       */
/*      them last.                                                      */
/* -------------------------------------------------------------------- */
#ifdef FRMT_usgsdem
    GDALRegister_USGSDEM();
#endif

#ifdef FRMT_gxf
    GDALRegister_GXF();
#endif    

#ifdef FRMT_grass
    GDALRegister_GRASS();
#endif

#ifdef FRMT_dods
    GDALRegister_DODS();
#endif
/* -------------------------------------------------------------------- */
/*      Deregister any drivers explicitly marked as supressed by the    */
/*      GDAL_SKIP environment variable.                                 */
/* -------------------------------------------------------------------- */
    GetGDALDriverManager()->AutoSkipDrivers();
}
