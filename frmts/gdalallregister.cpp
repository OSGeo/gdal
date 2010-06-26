/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALAllRegister(), primary format registration.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
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
 ****************************************************************************/

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
 * GDAL.  Many others as well haven't been updated in this
 * documentation (see <a href="http://gdal.org/formats_list.html">full list</a>):
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
 * <li> GSAG Golden Software ASCII Grid
 * <li> GSBG Golden Software Binary Grid
 * </ul>
 *
 * This function should generally be called once at the beginning of the application.
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
    GDALRegister_RPFTOC();
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
    
#ifdef FRMT_jaxapalsar
    GDALRegister_PALSARJaxa();
#endif
    
#ifdef FRMT_gff
    GDALRegister_GFF();
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
    GDALRegister_BIGGIF();
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

#ifdef FRMT_dimap
    GDALRegister_DIMAP();
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

#ifdef FRMT_sgi
    GDALRegister_SGI();
#endif

#ifdef FRMT_srtmhgt
    GDALRegister_SRTMHGT();
#endif

#ifdef FRMT_leveller
    GDALRegister_Leveller();
#endif

#ifdef FRMT_terragen
    GDALRegister_Terragen();
#endif

#ifdef FRMT_netcdf
    GDALRegister_GMT();
    GDALRegister_netCDF();
#endif

#ifdef FRMT_hdf4
    GDALRegister_HDF4();
    GDALRegister_HDF4Image();
#endif

#ifdef FRMT_pds
    GDALRegister_ISIS3();
    GDALRegister_ISIS2();
    GDALRegister_PDS();
#endif

#ifdef FRMT_til
    GDALRegister_TIL();
#endif

#ifdef FRMT_ers
    GDALRegister_ERS();
#endif

#ifdef FRMT_jp2kak
// JPEG2000 support using Kakadu toolkit
    GDALRegister_JP2KAK();
#endif

#ifdef FRMT_jpipkak
// JPEG2000 support using Kakadu toolkit
    GDALRegister_JPIPKAK();
#endif

#ifdef FRMT_ecw
    GDALRegister_ECW();
    GDALRegister_JP2ECW();
#endif

#ifdef FRMT_openjpeg
// JPEG2000 support using OpenJPEG library
    GDALRegister_JP2OpenJPEG();
#endif

#ifdef FRMT_jpeg2000
// JPEG2000 support using JasPer toolkit
// This one should always be placed after other JasPer supported formats,
// such as BMP or PNM. In other case we will get bad side effects.
    GDALRegister_JPEG2000();
#endif

#ifdef FRMT_l1b
    GDALRegister_L1B();
#endif

#ifdef FRMT_fit
    GDALRegister_FIT();
#endif

#ifdef FRMT_grib
    GDALRegister_GRIB();
#endif

#ifdef FRMT_mrsid
    GDALRegister_MrSID();
#endif

#ifdef FRMT_mrsid_lidar
    GDALRegister_MG4Lidar();
#endif

#ifdef FRMT_rmf
    GDALRegister_RMF();
#endif

#ifdef FRMT_wcs
    GDALRegister_WCS();
#endif

#ifdef FRMT_wms
    GDALRegister_WMS();
#endif

#ifdef FRMT_sde
    GDALRegister_SDE();
#endif

#ifdef FRMT_msgn
    GDALRegister_MSGN();
#endif

#ifdef FRMT_msg
    GDALRegister_MSG();
#endif

#ifdef FRMT_idrisi
    GDALRegister_IDRISI();
#endif

#ifdef FRMT_ingr
    GDALRegister_INGR();
#endif

#ifdef FRMT_gsg
    GDALRegister_GSAG();
    GDALRegister_GSBG();
    GDALRegister_GS7BG();
#endif

#ifdef FRMT_cosar
    GDALRegister_COSAR();
#endif

#ifdef FRMT_tsx
    GDALRegister_TSX();
#endif

#ifdef FRMT_coasp
    GDALRegister_COASP();
#endif

#ifdef FRMT_tms
    GDALRegister_TMS();
#endif

#ifdef FRMT_r
    GDALRegister_R();
#endif

/* -------------------------------------------------------------------- */
/*      Put raw formats at the end of the list. These drivers support   */
/*      various ASCII-header labeled formats, so the driver could be    */
/*      confused if you have files in some of above formats and such    */
/*      ASCII-header in the same directory.                             */
/* -------------------------------------------------------------------- */

#ifdef FRMT_raw
    GDALRegister_GTX();
    GDALRegister_PNM();
    GDALRegister_DOQ1();
    GDALRegister_DOQ2();
    GDALRegister_ENVI();
    GDALRegister_EHdr();
    GDALRegister_GenBin();
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
    GDALRegister_EIR();
    GDALRegister_DIPEx();
    GDALRegister_LCP();
#endif

/* -------------------------------------------------------------------- */
/*      Our test for the following is weak or expensive so we try       */
/*      them last.                                                      */
/* -------------------------------------------------------------------- */

#ifdef FRMT_rik
    GDALRegister_RIK();
#endif

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

#ifdef FRMT_wcs
    GDALRegister_HTTP();
#endif

#ifdef FRMT_hdf5
    GDALRegister_BAG();
    GDALRegister_HDF5();
    GDALRegister_HDF5Image();
#endif

#ifdef FRMT_northwood
	GDALRegister_NWT_GRD();
	GDALRegister_NWT_GRC();
#endif

#ifdef FRMT_adrg
    GDALRegister_ADRG();
    GDALRegister_SRP();
#endif

#ifdef FRMT_blx
    GDALRegister_BLX();
#endif

#ifdef FRMT_pgchip
    GDALRegister_PGCHIP();
#endif

#ifdef FRMT_georaster
    GDALRegister_GEOR();
#endif

#ifdef FRMT_rasterlite
    GDALRegister_Rasterlite();
#endif

#ifdef FRMT_epsilon
    GDALRegister_EPSILON();
#endif

#ifdef FRMT_wktraster
    GDALRegister_WKTRaster();
#endif

#ifdef FRMT_saga
    GDALRegister_SAGA();
#endif

#ifdef FRMT_kmlsuperoverlay
    GDALRegister_KMLSUPEROVERLAY();
#endif

#ifdef FRMT_xyz
    GDALRegister_XYZ();
#endif
/* -------------------------------------------------------------------- */
/*      Deregister any drivers explicitly marked as supressed by the    */
/*      GDAL_SKIP environment variable.                                 */
/* -------------------------------------------------------------------- */
    GetGDALDriverManager()->AutoSkipDrivers();
}
