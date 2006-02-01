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
 * Revision 1.83  2006/02/01 17:22:19  fwarmerdam
 * added DIPX driver
 *
 * Revision 1.82  2005/12/20 16:16:57  fwarmerdam
 * added sgi format
 *
 * Revision 1.81  2005/10/20 13:46:07  fwarmerdam
 * added Leveller
 *
 * Revision 1.80  2005/08/17 15:34:23  fwarmerdam
 * added RIK driver
 *
 * Revision 1.79  2005/07/12 16:56:56  denad21
 * added hdf5 support
 *
 * Revision 1.78  2005/07/07 23:35:44  fwarmerdam
 * added msgn support
 *
 * Revision 1.77  2005/05/19 20:41:14  dron
 * Added RMF format.
 *
 * Revision 1.76  2005/04/04 15:25:26  fwarmerdam
 * GDALAllRegister() now CPL_STDCALL
 *
 * Revision 1.75  2005/01/15 07:46:53  fwarmerdam
 * added GDALRegister_JP2ECW
 *
 * Revision 1.74  2005/01/06 20:27:35  fwarmerdam
 * added NDF driver
 *
 * Revision 1.73  2004/12/26 16:17:31  fwarmerdam
 * added ida format
 *
 * Revision 1.72  2004/12/09 16:37:18  fwarmerdam
 * re-enable ILWIS
 *
 * Revision 1.71  2004/12/02 15:58:58  fwarmerdam
 * temporarily disable ilwis driver
 *
 * Revision 1.70  2004/11/30 17:02:37  lichun
 * Added ILWIS driver support
 *
 * Revision 1.69  2004/10/22 14:15:23  fwarmerdam
 * Added PCRaster support.
 *
 * Revision 1.68  2004/10/21 20:04:46  fwarmerdam
 * added Register_RS2
 *
 * Revision 1.67  2004/10/16 14:58:01  fwarmerdam
 * added GMT
 *
 * Revision 1.66  2004/09/16 18:24:13  fwarmerdam
 * added airsar
 *
 * Revision 1.65  2004/09/03 19:06:50  warmerda
 * added CPG driver
 *
 * Revision 1.64  2004/05/26 17:45:39  warmerda
 * added LAN format
 *
 * Revision 1.63  2004/04/05 21:30:44  warmerda
 * moved ECW down so other jpeg2000 drivers used in preference
 *
 * Revision 1.62  2004/01/07 20:06:34  warmerda
 * Added netcdf support
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
 * <li> HDF4 Hierachal Data Format Release 4
 * <li> HDF5 Hierachal Data Format Release 5
 * </ul>
 *
 */

void CPL_STDCALL GDALAllRegister()

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

#ifdef FRMT_airsar
    GDALRegister_AirSAR();
#endif

#ifdef FRMT_rs2
    GDALRegister_RS2();
#endif

#ifdef FRMT_pcidsk
    GDALRegister_PCIDSK();
#endif

#ifdef FRMT_pcraster
    GDALRegister_PCRaster();
#endif

#ifdef FRMT_ilwis
    GDALRegister_ILWIS();
#endif

#ifdef FRMT_rik
    GDALRegister_RIK();
#endif

#ifdef FRMT_sgi
    GDALRegister_SGI();
#endif

#ifdef FRMT_leveller
    GDALRegister_Leveller();
#endif

#ifdef FRMT_netcdf
    GDALRegister_GMT();
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
    GDALRegister_CPG();
    GDALRegister_IDA();
    GDALRegister_NDF();
    GDALRegister_DIPX();
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
    GDALRegister_JP2ECW();
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

#ifdef FRMT_rmf
    GDALRegister_RMF();
#endif

#ifdef FRMT_msgn
    GDALRegister_MSGN();
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
#ifdef FRMT_hdf5
    GDALRegister_HDF5();
    GDALRegister_HDF5Image();
#endif
/* -------------------------------------------------------------------- */
/*      Deregister any drivers explicitly marked as supressed by the    */
/*      GDAL_SKIP environment variable.                                 */
/* -------------------------------------------------------------------- */
    GetGDALDriverManager()->AutoSkipDrivers();
}
