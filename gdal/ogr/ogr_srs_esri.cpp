/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference translation to/from ESRI .prj definitions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
 * Copyright (c) 2013, Kyle Shannon <kyle at pobox dot com>
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

#include "cpl_port.h"
#include "ogr_spatialref.h"
#include "ogr_srs_esri_names.h"

#include <cmath>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <limits>

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$")

extern void OGREPSGDatumNameMassage( char ** ppszDatum );

CPL_C_START
void CleanupESRIDatumMappingTable( void );
CPL_C_END

static const char * const apszProjMapping[] = {
    "Albers", SRS_PT_ALBERS_CONIC_EQUAL_AREA,
    "Cassini", SRS_PT_CASSINI_SOLDNER,
    "Equidistant_Cylindrical", SRS_PT_EQUIRECTANGULAR,
    "Plate_Carree", SRS_PT_EQUIRECTANGULAR,
    "Hotine_Oblique_Mercator_Azimuth_Natural_Origin",
                                        SRS_PT_HOTINE_OBLIQUE_MERCATOR,
    "Lambert_Conformal_Conic", SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP,
    "Lambert_Conformal_Conic", SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP,
    "Van_der_Grinten_I", SRS_PT_VANDERGRINTEN,
    SRS_PT_TRANSVERSE_MERCATOR, SRS_PT_TRANSVERSE_MERCATOR,
    "Gauss_Kruger", SRS_PT_TRANSVERSE_MERCATOR,
    NULL, NULL };

static const char * const apszAlbersMapping[] = {
    SRS_PP_CENTRAL_MERIDIAN, SRS_PP_LONGITUDE_OF_CENTER,
    SRS_PP_LATITUDE_OF_ORIGIN, SRS_PP_LATITUDE_OF_CENTER,
    "Central_Parallel", SRS_PP_LATITUDE_OF_CENTER,
    NULL, NULL };

static const char * const apszECMapping[] = {
    SRS_PP_CENTRAL_MERIDIAN, SRS_PP_LONGITUDE_OF_CENTER,
    SRS_PP_LATITUDE_OF_ORIGIN, SRS_PP_LATITUDE_OF_CENTER,
    NULL, NULL };

static const char * const apszPolarStereographicMapping[] = {
    SRS_PP_STANDARD_PARALLEL_1, SRS_PP_LATITUDE_OF_ORIGIN,
    NULL, NULL };

static const char * const apszOrthographicMapping[] = {
    "Longitude_Of_Center", SRS_PP_CENTRAL_MERIDIAN,
    "Latitude_Of_Center", SRS_PP_LATITUDE_OF_ORIGIN,
    NULL, NULL };

static const char * const apszLambertConformalConicMapping[] = {
    "Central_Parallel", SRS_PP_LATITUDE_OF_ORIGIN,
    NULL, NULL };

static char **papszDatumMapping = NULL;
static CPLMutex* hDatumMappingMutex = NULL;

static const char * const apszDefaultDatumMapping[] = {
    "6267", "North_American_1927", SRS_DN_NAD27,
    "6269", "North_American_1983", SRS_DN_NAD83,
    NULL, NULL, NULL };

static const char * const apszSpheroidMapping[] = {
    "WGS_84", "WGS_1984",
    "WGS_72", "WGS_1972",
    "GRS_1967_Modified", "GRS_1967_Truncated",
    "Krassowsky_1940", "Krasovsky_1940",
    "Everest_1830_1937_Adjustment", "Everest_Adjustment_1937",
    NULL, NULL };

static const char * const apszUnitMapping[] = {
    "Meter", "meter",
    "Meter", "metre",
    "Foot", "foot",
    "Foot", "feet",
    "Foot", "international_feet",
    "Foot_US", SRS_UL_US_FOOT,
    "Foot_Clarke", "clarke_feet",
    "Degree", "degree",
    "Degree", "degrees",
    "Degree", SRS_UA_DEGREE,
    "Radian", SRS_UA_RADIAN,
    NULL, NULL };

/* -------------------------------------------------------------------- */
/*      Table relating USGS and ESRI state plane zones.                 */
/* -------------------------------------------------------------------- */
static const int anUsgsEsriZones[] =
{
  101, 3101,
  102, 3126,
  201, 3151,
  202, 3176,
  203, 3201,
  301, 3226,
  302, 3251,
  401, 3276,
  402, 3301,
  403, 3326,
  404, 3351,
  405, 3376,
  406, 3401,
  407, 3426,
  501, 3451,
  502, 3476,
  503, 3501,
  600, 3526,
  700, 3551,
  901, 3601,
  902, 3626,
  903, 3576,
 1001, 3651,
 1002, 3676,
 1101, 3701,
 1102, 3726,
 1103, 3751,
 1201, 3776,
 1202, 3801,
 1301, 3826,
 1302, 3851,
 1401, 3876,
 1402, 3901,
 1501, 3926,
 1502, 3951,
 1601, 3976,
 1602, 4001,
 1701, 4026,
 1702, 4051,
 1703, 6426,
 1801, 4076,
 1802, 4101,
 1900, 4126,
 2001, 4151,
 2002, 4176,
 2101, 4201,
 2102, 4226,
 2103, 4251,
 2111, 6351,
 2112, 6376,
 2113, 6401,
 2201, 4276,
 2202, 4301,
 2203, 4326,
 2301, 4351,
 2302, 4376,
 2401, 4401,
 2402, 4426,
 2403, 4451,
 2500,    0,
 2501, 4476,
 2502, 4501,
 2503, 4526,
 2600,    0,
 2601, 4551,
 2602, 4576,
 2701, 4601,
 2702, 4626,
 2703, 4651,
 2800, 4676,
 2900, 4701,
 3001, 4726,
 3002, 4751,
 3003, 4776,
 3101, 4801,
 3102, 4826,
 3103, 4851,
 3104, 4876,
 3200, 4901,
 3301, 4926,
 3302, 4951,
 3401, 4976,
 3402, 5001,
 3501, 5026,
 3502, 5051,
 3601, 5076,
 3602, 5101,
 3701, 5126,
 3702, 5151,
 3800, 5176,
 3900,    0,
 3901, 5201,
 3902, 5226,
 4001, 5251,
 4002, 5276,
 4100, 5301,
 4201, 5326,
 4202, 5351,
 4203, 5376,
 4204, 5401,
 4205, 5426,
 4301, 5451,
 4302, 5476,
 4303, 5501,
 4400, 5526,
 4501, 5551,
 4502, 5576,
 4601, 5601,
 4602, 5626,
 4701, 5651,
 4702, 5676,
 4801, 5701,
 4802, 5726,
 4803, 5751,
 4901, 5776,
 4902, 5801,
 4903, 5826,
 4904, 5851,
 5001, 6101,
 5002, 6126,
 5003, 6151,
 5004, 6176,
 5005, 6201,
 5006, 6226,
 5007, 6251,
 5008, 6276,
 5009, 6301,
 5010, 6326,
 5101, 5876,
 5102, 5901,
 5103, 5926,
 5104, 5951,
 5105, 5976,
 5201, 6001,
 5200, 6026,
 5200, 6076,
 5201, 6051,
 5202, 6051,
 5300,    0,
 5400,    0
};

/* -------------------------------------------------------------------- */
/*      Datum Mapping functions and definitions                         */
/* -------------------------------------------------------------------- */
// TODO: adapt existing code and test.
#define DM_IDX_EPSG_CODE 0
#define DM_IDX_ESRI_NAME 1
#define DM_IDX_EPSG_NAME 2
#define DM_ELT_SIZE      3

#define DM_GET_EPSG_CODE(map, i) map[(i)*DM_ELT_SIZE + DM_IDX_EPSG_CODE]
#define DM_GET_ESRI_NAME(map, i) map[(i)*DM_ELT_SIZE + DM_IDX_ESRI_NAME]

static char *DMGetEPSGCode( int i ) {
    return DM_GET_EPSG_CODE(papszDatumMapping, i);
}

static char *DMGetESRIName( int i ) {
    return DM_GET_ESRI_NAME(papszDatumMapping, i);
}

/************************************************************************/
/*                           SetNewName()                               */
/*                                                                      */
/*      Set an esri name                                                */
/************************************************************************/
static
void SetNewName( OGRSpatialReference* pOgr, const char* keyName,
                 const char* newName )
{
    OGR_SRSNode *poNode = pOgr->GetAttrNode( keyName );
    OGR_SRSNode *poNodeChild = NULL;
    if( poNode )
        poNodeChild = poNode->GetChild(0);
    if( poNodeChild)
        poNodeChild->SetValue( newName);
}

/************************************************************************/
/*                           ESRIToUSGSZone()                           */
/*                                                                      */
/*      Convert ESRI style state plane zones to USGS style state        */
/*      plane zones.                                                    */
/************************************************************************/

static int ESRIToUSGSZone( int nESRIZone )

{
    // anUsgsEsriZones is a series of ints where 2 consecutive integers
    // are used to map from USGS to ESRI state plane zones.
    // TODO(schwehr): Would be better as a std::map.
    const int nPairs = sizeof(anUsgsEsriZones) / (2 * sizeof(int));

    for( int i = 0; i < nPairs; i++ )
    {
        if( anUsgsEsriZones[i*2+1] == nESRIZone )
            return anUsgsEsriZones[i*2];
    }

    return 0;
}

/************************************************************************/
/*                          MorphNameToESRI()                           */
/*                                                                      */
/*      Make name ESRI compatible. Convert spaces and special           */
/*      characters to underscores and then strip down.                  */
/************************************************************************/

static void MorphNameToESRI( char ** ppszName )

{
    char *pszName = *ppszName;

    if( pszName[0] == '\0' )
        return;

/* -------------------------------------------------------------------- */
/*      Translate non-alphanumeric values to underscores.               */
/* -------------------------------------------------------------------- */
    for( int i = 0; pszName[i] != '\0'; i++ )
    {
        if( pszName[i] != '+'
            && !(pszName[i] >= 'A' && pszName[i] <= 'Z')
            && !(pszName[i] >= 'a' && pszName[i] <= 'z')
            && !(pszName[i] >= '0' && pszName[i] <= '9') )
        {
            pszName[i] = '_';
        }
    }

/* -------------------------------------------------------------------- */
/*      Remove repeated and trailing underscores.                       */
/* -------------------------------------------------------------------- */
    int j = 0;  // Used after the for loop.
    for( int i = 1; pszName[i] != '\0'; i++ )
    {
        if( pszName[j] == '_' && pszName[i] == '_' )
            continue;

        pszName[++j] = pszName[i];
    }
    if( pszName[j] == '_' )
        pszName[j] = '\0';
    else
        pszName[j+1] = '\0';
}

/************************************************************************/
/*                     CleanESRIDatumMappingTable()                     */
/************************************************************************/

CPL_C_START
void CleanupESRIDatumMappingTable()

{
    if( papszDatumMapping == NULL )
        return;

    if( papszDatumMapping != apszDefaultDatumMapping )
    {
        CSLDestroy( papszDatumMapping );
        papszDatumMapping = NULL;
    }

    if( hDatumMappingMutex != NULL )
    {
        CPLDestroyMutex(hDatumMappingMutex);
        hDatumMappingMutex = NULL;
    }
}
CPL_C_END

/************************************************************************/
/*                       InitDatumMappingTable()                        */
/************************************************************************/

static void InitDatumMappingTable()

{
    CPLMutexHolderD(&hDatumMappingMutex);
    if( papszDatumMapping != NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Try to open the datum.csv file.                                 */
/* -------------------------------------------------------------------- */
    const char *pszFilename = CSVFilename("gdal_datum.csv");
    VSILFILE *fp = VSIFOpenL( pszFilename, "rb" );

/* -------------------------------------------------------------------- */
/*      Use simple default set if we can't find the file.               */
/* -------------------------------------------------------------------- */
    if( fp == NULL )
    {
        papszDatumMapping = const_cast<char **>(apszDefaultDatumMapping);
        return;
    }

/* -------------------------------------------------------------------- */
/*      Figure out what fields we are interested in.                    */
/* -------------------------------------------------------------------- */
    char **papszFieldNames = CSVReadParseLineL( fp );
    const int nDatumCodeField = CSLFindString( papszFieldNames, "DATUM_CODE" );
    const int nEPSGNameField = CSLFindString( papszFieldNames, "DATUM_NAME" );
    const int nESRINameField =
        CSLFindString( papszFieldNames, "ESRI_DATUM_NAME" );

    CSLDestroy( papszFieldNames );

    if( nDatumCodeField == -1 || nEPSGNameField == -1 || nESRINameField == -1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to find required field in gdal_datum.csv in "
                  "InitDatumMappingTable(), using default table setup." );

        papszDatumMapping = const_cast<char **>(apszDefaultDatumMapping);
        VSIFCloseL( fp );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Read each line, adding a detail line for each.                  */
/* -------------------------------------------------------------------- */
    int nMappingCount = 0;
    const int nMaxDatumMappings = 1000;
    char **papszFields = NULL;
    // TODO(schwehr): Explain the 3.
    papszDatumMapping = static_cast<char **>(
        CPLCalloc(sizeof(char*), nMaxDatumMappings*3) );

    for( papszFields = CSVReadParseLineL( fp );
         papszFields != NULL;
         papszFields = CSVReadParseLineL( fp ) )
    {
        const int nFieldCount = CSLCount(papszFields);

        CPLAssert( nMappingCount+1 < nMaxDatumMappings );

        if( std::max(nEPSGNameField, std::max(nDatumCodeField, nESRINameField))
            < nFieldCount
            && nMaxDatumMappings > nMappingCount+1 )
        {
            papszDatumMapping[nMappingCount*3+0] =
                CPLStrdup( papszFields[nDatumCodeField] );
            papszDatumMapping[nMappingCount*3+1] =
                CPLStrdup( papszFields[nESRINameField] );
            papszDatumMapping[nMappingCount*3+2] =
                CPLStrdup( papszFields[nEPSGNameField] );
            OGREPSGDatumNameMassage( &(papszDatumMapping[nMappingCount*3+2]) );

            nMappingCount++;
        }
        CSLDestroy( papszFields );
    }

    VSIFCloseL( fp );

    papszDatumMapping[nMappingCount*3+0] = NULL;
    papszDatumMapping[nMappingCount*3+1] = NULL;
    papszDatumMapping[nMappingCount*3+2] = NULL;
}

/************************************************************************/
/*                         OSRImportFromESRI()                          */
/************************************************************************/

/**
 * \brief Import coordinate system from ESRI .prj format(s).
 *
 * This function is the same as the C++ method
 * OGRSpatialReference::importFromESRI().
 */
OGRErr OSRImportFromESRI( OGRSpatialReferenceH hSRS, char **papszPrj )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromESRI", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        importFromESRI( papszPrj );
}

/************************************************************************/
/*                              OSR_GDV()                               */
/*                                                                      */
/*      Fetch a particular parameter out of the parameter list, or      */
/*      the indicated default if it isn't available.  This is a         */
/*      helper function for importFromESRI().                           */
/************************************************************************/

static double OSR_GDV( char **papszNV, const char * pszField,
                       double dfDefaultValue )

{
    if( papszNV == NULL || papszNV[0] == NULL )
        return dfDefaultValue;

    if( STARTS_WITH_CI(pszField, "PARAM_") )
    {
        int iLine = 0;  // Used after for loop.
        for( ;
             papszNV[iLine] != NULL &&
                 !STARTS_WITH_CI(papszNV[iLine], "Paramet");
             iLine++ ) {}

        for( int nOffset = atoi(pszField+6);
             papszNV[iLine] != NULL && nOffset > 0;
             iLine++ )
        {
            if( strlen(papszNV[iLine]) > 0 )
                nOffset--;
        }

        while( papszNV[iLine] != NULL && strlen(papszNV[iLine]) == 0 )
            iLine++;

        if( papszNV[iLine] != NULL )
        {
            char * const pszLine = papszNV[iLine];

            // Trim comments.
            for( int i=0; pszLine[i] != '\0'; i++ )
            {
                if( pszLine[i] == '/' && pszLine[i+1] == '*' )
                    pszLine[i] = '\0';
            }

            double dfValue = 0.0;
            char **papszTokens = CSLTokenizeString(papszNV[iLine]);
            if( CSLCount(papszTokens) == 3 )
            {
                // http://agdcftp1.wr.usgs.gov/pub/projects/lcc/akcan_lcc/akcan.tar.gz
                // contains weird values for the second. Ignore it and
                // the result looks correct.
                double dfSecond = CPLAtof(papszTokens[2]);
                if( dfSecond < 0.0 || dfSecond >= 60.0 )
                    dfSecond = 0.0;

                dfValue = std::abs(CPLAtof(papszTokens[0]))
                    + CPLAtof(papszTokens[1]) / 60.0
                    + dfSecond / 3600.0;

                if( CPLAtof(papszTokens[0]) < 0.0 )
                    dfValue *= -1;
            }
            else if( CSLCount(papszTokens) > 0 )
            {
                dfValue = CPLAtof(papszTokens[0]);
            }
            else
            {
                dfValue = dfDefaultValue;
            }

            CSLDestroy( papszTokens );

            return dfValue;
        }

        return dfDefaultValue;
    }

    int iLine = 0;  // Used after for loop.
    for( ;
         papszNV[iLine] != NULL &&
             !EQUALN(papszNV[iLine], pszField, strlen(pszField));
         iLine++ ) {}

    if( papszNV[iLine] == NULL )
        return dfDefaultValue;

    return CPLAtof( papszNV[iLine] + strlen(pszField) );
}

/************************************************************************/
/*                              OSR_GDS()                               */
/************************************************************************/

static CPLString OSR_GDS( char **papszNV, const char * pszField,
                          const char *pszDefaultValue )

{
    if( papszNV == NULL || papszNV[0] == NULL )
        return pszDefaultValue;

    int iLine = 0;  // Used after for loop.
    for( ;
         papszNV[iLine] != NULL &&
             !EQUALN(papszNV[iLine], pszField, strlen(pszField));
         iLine++ ) {}

    if( papszNV[iLine] == NULL )
        return pszDefaultValue;

    char **papszTokens = CSLTokenizeString(papszNV[iLine]);

    CPLString osResult =
        CSLCount(papszTokens) > 1 ? papszTokens[1] : pszDefaultValue;

    CSLDestroy( papszTokens );
    return osResult;
}

/************************************************************************/
/*                  RemapPNamesBasedOnProjCSAndPName()                  */
/*                                                                      */
/*      Convert a parameters to ESRI style name                         */
/************************************************************************/

static
int RemapPNamesBasedOnProjCSAndPName( OGRSpatialReference* pOgr,
                                      const char* pszProgCSName,
                                      char **mappingTable )
{
  OGR_SRSNode *poPROJCS = pOgr->GetAttrNode( "PROJCS" );
  if( poPROJCS == NULL ) return -1;

  int ret = 0;
  for( int i = 0; mappingTable[i] != NULL; i += 3 )
  {
    while( mappingTable[i] != NULL &&
           EQUALN(pszProgCSName, mappingTable[i], strlen(mappingTable[i])) )
    {
      const char* pszParamName = mappingTable[i+1];
      for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
      {
          OGR_SRSNode *poParm = poPROJCS->GetChild( iChild );

          if( EQUAL(poParm->GetValue(), "PARAMETER")
              && poParm->GetChildCount() == 2
              && EQUAL(poParm->GetChild(0)->GetValue(), pszParamName) )
          {
              poParm->GetChild(0)->SetValue( mappingTable[i+2] );
              break;
          }
      }
      ret++;
      i += 3;
    }
    if( ret > 0 )
      break;
  }
  return ret;
}

/************************************************************************/
/*                          importFromESRI()                            */
/************************************************************************/

/**
 * \brief Import coordinate system from ESRI .prj format(s).
 *
 * This function will read the text loaded from an ESRI .prj file, and
 * translate it into an OGRSpatialReference definition.  This should support
 * many (but by no means all) old style (Arc/Info 7.x) .prj files, as well
 * as the newer pseudo-OGC WKT .prj files.  Note that new style .prj files
 * are in OGC WKT format, but require some manipulation to correct datum
 * names, and units on some projection parameters.  This is addressed within
 * importFromESRI() by an automatic call to morphFromESRI().
 *
 * Currently only GEOGRAPHIC, UTM, STATEPLANE, GREATBRITIAN_GRID, ALBERS,
 * EQUIDISTANT_CONIC, TRANSVERSE (mercator), POLAR, MERCATOR and POLYCONIC
 * projections are supported from old style files.
 *
 * At this time there is no equivalent exportToESRI() method.  Writing old
 * style .prj files is not supported by OGRSpatialReference. However the
 * morphToESRI() and exportToWkt() methods can be used to generate output
 * suitable to write to new style (Arc 8) .prj files.
 *
 * This function is the equivalent of the C function OSRImportFromESRI().
 *
 * @param papszPrj NULL terminated list of strings containing the definition.
 *
 * @return OGRERR_NONE on success or an error code in case of failure.
 */

OGRErr OGRSpatialReference::importFromESRI( char **papszPrj )

{
    if( papszPrj == NULL || papszPrj[0] == NULL )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      ArcGIS and related products now use a variant of Well Known     */
/*      Text.  Try to recognize this and ingest it.  WKT is usually     */
/*      all on one line, but we will accept multi-line formats and      */
/*      concatenate.                                                    */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(papszPrj[0], "GEOGCS")
        || STARTS_WITH_CI(papszPrj[0], "PROJCS")
        || STARTS_WITH_CI(papszPrj[0], "LOCAL_CS") )
    {
        char *pszWKT = CPLStrdup(papszPrj[0]);
        for( int i = 1; papszPrj[i] != NULL; i++ )
        {
            pszWKT = static_cast<char *>(
                CPLRealloc(pszWKT, strlen(pszWKT)+strlen(papszPrj[i]) + 1));
            strcat( pszWKT, papszPrj[i] );
        }
        char *pszWKT2 = pszWKT;
        OGRErr eErr = importFromWkt( &pszWKT2 );
        CPLFree( pszWKT );

        if( eErr == OGRERR_NONE )
            eErr = morphFromESRI();
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection name.                    */
/* -------------------------------------------------------------------- */
    CPLString osProj = OSR_GDS( papszPrj, "Projection", "" );

    if( EQUAL(osProj, "") )
    {
        CPLDebug( "OGR_ESRI", "Can't find Projection" );
        return OGRERR_CORRUPT_DATA;
    }
    else if( EQUAL(osProj, "GEOGRAPHIC") )
    {
        // Nothing to do.
    }
    else if( EQUAL(osProj, "utm") )
    {
        const double dfOsrGdv = OSR_GDV(papszPrj, "zone", 0.0);
        if( dfOsrGdv > 0 && dfOsrGdv < 61 )
        {
            const double dfYShift = OSR_GDV(papszPrj, "Yshift", 0.0);

            SetUTM(static_cast<int>(dfOsrGdv), dfYShift == 0.0);
        }
        else
        {
            const double dfCentralMeridian = OSR_GDV(papszPrj, "PARAM_1", 0.0);
            const double dfRefLat = OSR_GDV(papszPrj, "PARAM_2", 0.0);
            if( dfCentralMeridian >= -180.0 && dfCentralMeridian <= 180.0 )
            {
                const int nZone = static_cast<int>(
                    (dfCentralMeridian + 183.0) / 6.0 + 0.0000001 );
                SetUTM( nZone, dfRefLat >= 0.0 );
            }
        }
    }
    else if( EQUAL(osProj, "STATEPLANE") )
    {
        const double dfZone = OSR_GDV(papszPrj, "zone", 0.0);

        if( dfZone < std::numeric_limits<int>::min() ||
            dfZone > std::numeric_limits<int>::max() ||
            CPLIsNan(dfZone) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "zone out of range: %f", dfZone);
            return OGRERR_CORRUPT_DATA;
        }

        int nZone = static_cast<int>( dfZone );

        if( nZone != 0 )
            nZone = ESRIToUSGSZone( nZone );
        else
        {
            const double dfFipszone = OSR_GDV(papszPrj, "fipszone", 0.0);

            if( dfFipszone < std::numeric_limits<int>::min() ||
                dfFipszone > std::numeric_limits<int>::max() ||
                CPLIsNan(dfFipszone) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "fipszone out of range: %f", dfFipszone);
                return OGRERR_CORRUPT_DATA;
            }

            nZone = static_cast<int>( dfFipszone );
        }

        if( nZone != 0 )
        {
            if( EQUAL(OSR_GDS( papszPrj, "Datum", "NAD83"), "NAD27") )
                SetStatePlane( nZone, FALSE );
            else
                SetStatePlane( nZone, TRUE );
        }
    }
    else if( EQUAL(osProj, "GREATBRITIAN_GRID")
             || EQUAL(osProj, "GREATBRITAIN_GRID") )
    {
        const char *pszWkt =
            "PROJCS[\"OSGB 1936 / British National Grid\","
            "GEOGCS[\"OSGB 1936\",DATUM[\"OSGB_1936\","
            "SPHEROID[\"Airy 1830\",6377563.396,299.3249646]],"
            "PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]],"
            "PROJECTION[\"Transverse_Mercator\"],"
            "PARAMETER[\"latitude_of_origin\",49],"
            "PARAMETER[\"central_meridian\",-2],"
            "PARAMETER[\"scale_factor\",0.999601272],"
            "PARAMETER[\"false_easting\",400000],"
            "PARAMETER[\"false_northing\",-100000],UNIT[\"metre\",1]]";

        importFromWkt( const_cast<char **>(&pszWkt) );
    }
    else if( EQUAL(osProj, "ALBERS") )
    {
        SetACEA( OSR_GDV( papszPrj, "PARAM_1", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_4", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_5", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_6", 0.0 ) );
    }
    else if( EQUAL(osProj, "LAMBERT") )
    {
        SetLCC( OSR_GDV( papszPrj, "PARAM_1", 0.0 ),
                OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                OSR_GDV( papszPrj, "PARAM_4", 0.0 ),
                OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
                OSR_GDV( papszPrj, "PARAM_5", 0.0 ),
                OSR_GDV( papszPrj, "PARAM_6", 0.0 ) );
    }
    else if( EQUAL(osProj, "LAMBERT_AZIMUTHAL") )
    {
        SetLAEA( OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_1", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_4", 0.0 ) );
    }
    else if( EQUAL(osProj, "EQUIDISTANT_CONIC") )
    {
        const int nStdPCount = static_cast<int>(
            OSR_GDV( papszPrj, "PARAM_1", 0.0 ) );

        if( nStdPCount == 1 )
        {
            SetEC( OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_4", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_5", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_6", 0.0 ) );
        }
        else
        {
            SetEC( OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_5", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_4", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_5", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_7", 0.0 ) );
        }
    }
    else if( EQUAL(osProj, "TRANSVERSE") )
    {
        SetTM( OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
               OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
               OSR_GDV( papszPrj, "PARAM_1", 0.0 ),
               OSR_GDV( papszPrj, "PARAM_4", 0.0 ),
               OSR_GDV( papszPrj, "PARAM_5", 0.0 ) );
    }
    else if( EQUAL(osProj, "POLAR") )
    {
        SetPS( OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
               OSR_GDV( papszPrj, "PARAM_1", 0.0 ),
               1.0,
               OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
               OSR_GDV( papszPrj, "PARAM_4", 0.0 ) );
    }
    else if( EQUAL(osProj, "MERCATOR") )
    {
        SetMercator( OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                     OSR_GDV( papszPrj, "PARAM_1", 0.0 ),
                     1.0,
                     OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
                     OSR_GDV( papszPrj, "PARAM_4", 0.0 ) );
    }
    else if( EQUAL(osProj, SRS_PT_MERCATOR_AUXILIARY_SPHERE) )
    {
       // This is EPSG:3875 Pseudo Mercator. We might as well import it from
       // the EPSG spec.
       importFromEPSG(3857);
    }
    else if( EQUAL(osProj, "POLYCONIC") )
    {
        SetPolyconic( OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                      OSR_GDV( papszPrj, "PARAM_1", 0.0 ),
                      OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
                      OSR_GDV( papszPrj, "PARAM_4", 0.0 ) );
    }
    else
    {
        CPLDebug( "OGR_ESRI", "Unsupported projection: %s", osProj.c_str() );
        SetLocalCS( osProj );
    }

/* -------------------------------------------------------------------- */
/*      Try to translate the datum/spheroid.                            */
/* -------------------------------------------------------------------- */
    if( !IsLocal() && GetAttrNode( "GEOGCS" ) == NULL )
    {
        const CPLString osDatum = OSR_GDS( papszPrj, "Datum", "");

        if( EQUAL(osDatum, "NAD27") || EQUAL(osDatum, "NAD83")
            || EQUAL(osDatum, "WGS84") || EQUAL(osDatum, "WGS72") )
        {
            SetWellKnownGeogCS( osDatum );
        }
        else if( EQUAL( osDatum, "EUR" )
                 || EQUAL( osDatum, "ED50" ) )
        {
            SetWellKnownGeogCS( "EPSG:4230" );
        }
        else if( EQUAL( osDatum, "GDA94" ) )
        {
            SetWellKnownGeogCS( "EPSG:4283" );
        }
        else
        {
            CPLString osSpheroid = OSR_GDS( papszPrj, "Spheroid", "");

            if( EQUAL(osSpheroid, "INT1909")
                || EQUAL(osSpheroid, "INTERNATIONAL1909") )
            {
                OGRSpatialReference oGCS;
                oGCS.importFromEPSG( 4022 );
                CopyGeogCSFrom( &oGCS );
            }
            else if( EQUAL(osSpheroid, "AIRY") )
            {
                OGRSpatialReference oGCS;
                oGCS.importFromEPSG( 4001 );
                CopyGeogCSFrom( &oGCS );
            }
            else if( EQUAL(osSpheroid, "CLARKE1866") )
            {
                OGRSpatialReference oGCS;
                oGCS.importFromEPSG( 4008 );
                CopyGeogCSFrom( &oGCS );
            }
            else if( EQUAL(osSpheroid, "GRS80") )
            {
                OGRSpatialReference oGCS;
                oGCS.importFromEPSG( 4019 );
                CopyGeogCSFrom( &oGCS );
            }
            else if( EQUAL(osSpheroid, "KRASOVSKY")
                     || EQUAL(osSpheroid, "KRASSOVSKY")
                     || EQUAL(osSpheroid, "KRASSOWSKY") )
            {
                OGRSpatialReference oGCS;
                oGCS.importFromEPSG( 4024 );
                CopyGeogCSFrom( &oGCS );
            }
            else if( EQUAL(osSpheroid, "Bessel") )
            {
                OGRSpatialReference oGCS;
                oGCS.importFromEPSG( 4004 );
                CopyGeogCSFrom( &oGCS );
            }
            else
            {
                bool bFoundParameters = false;
                for( int iLine = 0; papszPrj[iLine] != NULL; iLine++ )
                {
                    if( STARTS_WITH_CI(papszPrj[iLine], "Parameters") )
                    {
                        char** papszTokens =
                            CSLTokenizeString(papszPrj[iLine] +
                                              strlen("Parameters"));
                        if( CSLCount(papszTokens) == 2 )
                        {
                            OGRSpatialReference oGCS;
                            const double dfSemiMajor = CPLAtof(papszTokens[0]);
                            const double dfSemiMinor = CPLAtof(papszTokens[1]);
                            const double dfInvFlattening =
                                OSRCalcInvFlattening(dfSemiMajor, dfSemiMinor);
                            oGCS.SetGeogCS( "unknown", "unknown", "unknown",
                                            dfSemiMajor, dfInvFlattening );
                            CopyGeogCSFrom( &oGCS );
                            bFoundParameters = true;
                        }
                        CSLDestroy(papszTokens);
                        break;
                    }
                }
                if( !bFoundParameters )
                {
                    // If unknown, default to WGS84 so there is something there.
                    SetWellKnownGeogCS( "WGS84" );
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Linear units translation                                        */
/* -------------------------------------------------------------------- */
    if( IsLocal() || IsProjected() )
    {
        const double dfOldUnits = GetLinearUnits();
        const CPLString osValue = OSR_GDS( papszPrj, "Units", "" );

        if( EQUAL(osValue, "" ) )
            SetLinearUnitsAndUpdateParameters( SRS_UL_METER, 1.0 );
        else if( EQUAL(osValue, "FEET") )
            SetLinearUnitsAndUpdateParameters( SRS_UL_US_FOOT,
                                               CPLAtof(SRS_UL_US_FOOT_CONV) );
        else if( CPLAtof(osValue) != 0.0 )
            SetLinearUnitsAndUpdateParameters( "user-defined",
                                               1.0 / CPLAtof(osValue) );
        else
            SetLinearUnitsAndUpdateParameters( osValue, 1.0 );

        // If we have reset the linear units we should clear any authority
        // nodes on the PROJCS.  This especially applies to state plane
        // per bug #1697.
        const double dfNewUnits = GetLinearUnits();
        if( dfOldUnits != 0.0
            && (dfNewUnits / dfOldUnits < 0.9999999
                || dfNewUnits / dfOldUnits > 1.0000001) )
        {
            if( GetRoot()->FindChild( "AUTHORITY" ) != -1 )
                GetRoot()->DestroyChild(GetRoot()->FindChild( "AUTHORITY" ));
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                RemapPValuesBasedOnProjCSAndPName()                   */
/*                                                                      */
/*      Convert a parameters to ESRI style name                         */
/************************************************************************/

static
int RemapPValuesBasedOnProjCSAndPName( OGRSpatialReference* pOgr,
                                       const char* pszProgCSName,
                                       char **mappingTable )
{
    OGR_SRSNode *poPROJCS = pOgr->GetAttrNode( "PROJCS" );
    if( poPROJCS == NULL ) return -1;

    int ret = 0;
    for( int i = 0; mappingTable[i] != NULL; i += 4 )
    {
        while( mappingTable[i] != NULL &&
               EQUALN(pszProgCSName, mappingTable[i], strlen(mappingTable[i])) )
        {
            const char* pszParamName = mappingTable[i+1];
            const char* pszParamValue = mappingTable[i+2];
            for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
            {
                OGR_SRSNode *poParm = poPROJCS->GetChild( iChild );

                if( EQUAL(poParm->GetValue(), "PARAMETER")
                    && poParm->GetChildCount() == 2
                    && EQUAL(poParm->GetChild(0)->GetValue(), pszParamName)
                    && EQUALN(poParm->GetChild(1)->GetValue(),
                              pszParamValue, strlen(pszParamValue) ) )
                {
                    poParm->GetChild(1)->SetValue( mappingTable[i+3] );
                    break;
                }
            }
            ret++;
            i += 4;
        }
        if( ret > 0 )
            break;
    }
    return ret;
}

/************************************************************************/
/*                          AddParamBasedOnPrjName()                    */
/*                                                                      */
/*      Add ESRI style parameters                                       */
/************************************************************************/

static
int AddParamBasedOnPrjName( OGRSpatialReference* pOgr,
                            const char* pszProjectionName,
                            char **mappingTable )
{
    OGR_SRSNode *poPROJCS = pOgr->GetAttrNode( "PROJCS" );
    if( poPROJCS == NULL ) return -1;

    int ret = -1;
    for( int i = 0; mappingTable[i] != NULL; i += 3 )
    {
        if( EQUALN(pszProjectionName, mappingTable[i],
                   strlen(mappingTable[i])) )
        {
            bool exist = false;
            for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
            {
                OGR_SRSNode *poParm = poPROJCS->GetChild( iChild );

                if( EQUAL(poParm->GetValue(), "PARAMETER")
                    && poParm->GetChildCount() == 2
                    && EQUAL(poParm->GetChild(0)->GetValue(),
                             mappingTable[i+1]) )
                    exist = true;
            }
            if( !exist )
            {
                OGR_SRSNode *poParm = new OGR_SRSNode( "PARAMETER" );
                poParm->AddChild( new OGR_SRSNode( mappingTable[i+1] ) );
                poParm->AddChild( new OGR_SRSNode( mappingTable[i+2] ) );
                poPROJCS->AddChild( poParm );
                ret++;
            }
        }
    }
    return ret;
}

/************************************************************************/
/*                        DeleteParamBasedOnPrjName                     */
/*                                                                      */
/*      Delete non-ESRI parameters                                      */
/************************************************************************/

static
int DeleteParamBasedOnPrjName( OGRSpatialReference* pOgr,
                               const char* pszProjectionName,
                               char **mappingTable )
{
    int ret = -1;
    for( int i = 0; mappingTable[i] != NULL; i += 2 )
    {
        if( EQUALN(pszProjectionName, mappingTable[i],
                   strlen(mappingTable[i])) )
        {
            OGR_SRSNode *poPROJCS = pOgr->GetAttrNode( "PROJCS" );
            if( poPROJCS == NULL ) break;

            const char* pszParamName = mappingTable[i+1];
            int iIndex = -1;
            for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
            {
                OGR_SRSNode *poParm = poPROJCS->GetChild( iChild );

                if( EQUAL(poParm->GetValue(), "PARAMETER")
                    && poParm->GetChildCount() == 2
                    && EQUAL(poParm->GetChild(0)->GetValue(), pszParamName) )
                {
                    iIndex = iChild;
                    break;
                }
            }
            if( iIndex >= 0 )
            {
                poPROJCS->DestroyChild( iIndex );
                ret++;
            }
        }
    }
    return ret;
}

/************************************************************************/
/*                     RemapNamesBasedOnTwo()                           */
/*                                                                      */
/*      Convert a name to ESRI style name                               */
/************************************************************************/

static
int RemapNamesBasedOnTwo( OGRSpatialReference* pOgr, const char* name1,
                          const char* name2,
                          char **mappingTable, int nTableStepSize,
                          char** pszkeyNames, long nKeys )
{
    int iIndex = -1;
    for( int i = 0; mappingTable[i] != NULL; i += nTableStepSize )
    {
        const size_t n = std::min(strlen(name1), strlen(mappingTable[i]));
        if( EQUALN(name1, mappingTable[i], n) )
        {
            int j = i;
            while( mappingTable[j] != NULL &&
                   EQUAL(mappingTable[i], mappingTable[j]) )
            {
                // TODO(warmerdam): Explain what is going on here from r15992.
                if( EQUALN(name2, mappingTable[j+1],
                           strlen(mappingTable[j+1])) )
                {
                    iIndex = j;
                    break;
                }
                j += 3;
            }
            if( iIndex >= 0 )
                break;
        }
    }
    if( iIndex >= 0 )
    {
        for( int i = 0; i < nKeys; i ++ )
        {
            OGR_SRSNode *poNode = pOgr->GetAttrNode( pszkeyNames[i] );
            OGR_SRSNode *poNodeChild = NULL;
            if( poNode )
                poNodeChild = poNode->GetChild(0);
            if( poNodeChild && strlen(poNodeChild->GetValue()) > 0 )
                poNodeChild->SetValue( mappingTable[iIndex+i+2]);
        }
    }
    return iIndex;
}

/************************************************************************/
/*                           RemapNameBasedOnKeyName()                  */
/*                                                                      */
/*      Convert a name to ESRI style name                               */
/************************************************************************/

static
int RemapNameBasedOnKeyName( OGRSpatialReference* pOgr, const char* pszName,
                             const char* pszkeyName,
                             char **mappingTable )
{
    int iIndex = -1;
    for( int i = 0; mappingTable[i] != NULL; i += 2 )
    {
        if( EQUAL(pszName, mappingTable[i]) )
        {
            iIndex = i;
            break;
        }
    }
    if( iIndex >= 0 )
    {
        OGR_SRSNode *poNode = pOgr->GetAttrNode( pszkeyName );
        OGR_SRSNode *poNodeChild = NULL;
        if( poNode )
            poNodeChild = poNode->GetChild(0);
        if( poNodeChild && strlen(poNodeChild->GetValue()) > 0 )
            poNodeChild->SetValue( mappingTable[iIndex+1]);
    }
    return iIndex;
}

/************************************************************************/
/*                                   RemapGeogCSName()                  */
/*                                                                      */
/*      Convert names to ESRI style                                     */
/************************************************************************/
static
int RemapGeogCSName( OGRSpatialReference* pOgr, const char *pszGeogCSName )
{
    static const char * const keyNamesG[] = { "GEOGCS" };
    int ret = -1;

    const char* pszUnitName = pOgr->GetAttrValue( "GEOGCS|UNIT");
    if( pszUnitName )
    {
        // TODO(schwehr): Figure out a safer way to rename.
        //   The casting away const here looks dangerous.
        ret = RemapNamesBasedOnTwo(
            pOgr, pszGeogCSName+4, pszUnitName,
            const_cast<char**>(apszGcsNameMappingBasedOnUnit),
            3, const_cast<char**>(keyNamesG), 1);
    }

    if( ret < 0 )
    {
        const char* pszPrimeName = pOgr->GetAttrValue("PRIMEM");
        if( pszPrimeName )
            ret = RemapNamesBasedOnTwo(
                pOgr, pszGeogCSName+4, pszPrimeName,
                const_cast<char**>(apszGcsNameMappingBasedPrime),
                3, const_cast<char**>(keyNamesG), 1);
        if( ret < 0 )
            ret = RemapNameBasedOnKeyName(
                pOgr, pszGeogCSName+4, "GEOGCS",
                const_cast<char**>(apszGcsNameMapping) );
    }
    if( ret < 0 )
    {
        const char* pszProjCS = pOgr->GetAttrValue( "PROJCS" );
        ret = RemapNamesBasedOnTwo(
            pOgr, pszProjCS, pszGeogCSName,
            const_cast<char**>(apszGcsNameMappingBasedOnProjCS),
            3, const_cast<char**>(keyNamesG), 1);
    }
    return ret;
}

/************************************************************************/
/*                            morphToESRI()                             */
/************************************************************************/
/**
 * \brief Convert in place to ESRI WKT format.
 *
 * The value nodes of this coordinate system are modified in various manners
 * more closely map onto the ESRI concept of WKT format.  This includes
 * renaming a variety of projections and arguments, and stripping out
 * nodes note recognised by ESRI (like AUTHORITY and AXIS).
 *
 * This does the same as the C function OSRMorphToESRI().
 *
 * @return OGRERR_NONE unless something goes badly wrong.
 */

OGRErr OGRSpatialReference::morphToESRI()

{
/* -------------------------------------------------------------------- */
/*      Special case for EPSG:3857. Hard to deal with in the general    */
/*      case as GDAL (at the time of writing) uses regular Mercator_1SP */
/*      to model it.                                                    */
/* -------------------------------------------------------------------- */
    const char* pszAuthName = GetAuthorityName(NULL);
    const char* pszAuthCode = GetAuthorityCode(NULL);
    if( (pszAuthName != NULL && EQUAL(pszAuthName, "EPSG") &&
         pszAuthCode != NULL && atoi(pszAuthCode) == 3857) ||
        (GetAttrValue("PROJCS") != NULL &&
         EQUAL(GetAttrValue("PROJCS"), "WGS 84 / Pseudo-Mercator")) )
    {
        Clear();
        const char* pszESRI_PE_WebMercator =
            "PROJCS[\"WGS_1984_Web_Mercator_Auxiliary_Sphere\","
            "GEOGCS[\"GCS_WGS_1984\",DATUM[\"D_WGS_1984\","
            "SPHEROID[\"WGS_1984\",6378137.0,298.257223563]],"
            "PRIMEM[\"Greenwich\",0.0],UNIT[\"Degree\",0.0174532925199433]],"
            "PROJECTION[\"Mercator_Auxiliary_Sphere\"],"
            "PARAMETER[\"False_Easting\",0.0],"
            "PARAMETER[\"False_Northing\",0.0],"
            "PARAMETER[\"Central_Meridian\",0.0],"
            "PARAMETER[\"Standard_Parallel_1\",0.0],"
            "PARAMETER[\"Auxiliary_Sphere_Type\",0.0],UNIT[\"Meter\",1.0]]";
        return importFromWkt( (char**) &pszESRI_PE_WebMercator );
    }

/* -------------------------------------------------------------------- */
/*      Fixup ordering, missing linear units, etc.                      */
/* -------------------------------------------------------------------- */
    {
        const OGRErr eErr = Fixup();
        if( eErr != OGRERR_NONE )
            return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Strip all CT parameters (AXIS, AUTHORITY, TOWGS84, etc).        */
/* -------------------------------------------------------------------- */
    {
        const OGRErr eErr = StripCTParms();
        if( eErr != OGRERR_NONE )
            return eErr;
    }

    if( GetRoot() == NULL )
        return OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      There is a special case for Hotine Oblique Mercator to split    */
/*      out the case with an angle to rectified grid.  Bug 423          */
/* -------------------------------------------------------------------- */
    const char *pszProjection = GetAttrValue("PROJECTION");

    if( pszProjection != NULL
        && EQUAL(pszProjection, SRS_PT_HOTINE_OBLIQUE_MERCATOR)
        && fabs(GetProjParm(SRS_PP_AZIMUTH, 0.0) - 90) < 0.0001
        && fabs(GetProjParm(SRS_PP_RECTIFIED_GRID_ANGLE, 0.0) - 90) < 0.0001 )
    {
        SetNode( "PROJCS|PROJECTION",
                 "Hotine_Oblique_Mercator_Azimuth_Center" );

        // Ideally we should strip out of the rectified_grid_angle.
        // Strip off rectified_grid_angle -- Hope it is 90!
        OGR_SRSNode *poPROJCS = GetAttrNode( "PROJCS" );
        const int iRGAChild = FindProjParm( "rectified_grid_angle", poPROJCS );
        if( iRGAChild != -1 )
            poPROJCS->DestroyChild( iRGAChild);

        pszProjection = GetAttrValue("PROJECTION");
    }

/* -------------------------------------------------------------------- */
/*      Polar_Stereographic maps to ESRI codes                          */
/*      Stereographic_South_Pole or Stereographic_North_Pole based      */
/*      on latitude.                                                    */
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL
        && ( EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) ))
    {
        if( GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) < 0.0 )
        {
            SetNode( "PROJCS|PROJECTION",
                     "Stereographic_South_Pole" );
            pszProjection = GetAttrValue("PROJECTION");
        }
        else
        {
            SetNode( "PROJCS|PROJECTION",
                     "Stereographic_North_Pole" );
            pszProjection = GetAttrValue("PROJECTION");
        }
    }

/* -------------------------------------------------------------------- */
/*      OBLIQUE_STEREOGRAPHIC maps to ESRI Double_Stereographic         */
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL
        && ( EQUAL(pszProjection, SRS_PT_OBLIQUE_STEREOGRAPHIC) ))
    {
        SetNode( "PROJCS|PROJECTION", "Double_Stereographic" );
    }

/* -------------------------------------------------------------------- */
/*      Translate PROJECTION keywords that are misnamed.                */
/* -------------------------------------------------------------------- */
    // TODO(schwehr): How is applyRemapper safe with
    //   static const char * const apszProjMapping[]?
    GetRoot()->applyRemapper( "PROJECTION",
                              const_cast<char **>(apszProjMapping + 1),
                              const_cast<char **>(apszProjMapping),
                              2 );
    pszProjection = GetAttrValue("PROJECTION");

/* -------------------------------------------------------------------- */
/*      Translate DATUM keywords that are misnamed.                     */
/* -------------------------------------------------------------------- */
    InitDatumMappingTable();

    GetRoot()->applyRemapper( "DATUM",
                              papszDatumMapping + 2, papszDatumMapping + 1, 3 );

    const char *pszProjCSName      = NULL;
    OGR_SRSNode *poProjCS          = NULL;
    OGR_SRSNode *poProjCSNodeChild = NULL;

/* -------------------------------------------------------------------- */
/*      Very specific handling for some well known geographic           */
/*      coordinate systems.                                             */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poGeogCS = GetAttrNode( "GEOGCS" );
    if( poGeogCS != NULL )
    {
        const char *pszGeogCSName = poGeogCS->GetChild(0)->GetValue();
        pszAuthName = GetAuthorityName("GEOGCS");
        const char *pszUTMPrefix = NULL;
        int nGCSCode = -1;

        if( pszAuthName != NULL && EQUAL(pszAuthName, "EPSG") )
            nGCSCode = atoi(GetAuthorityCode("GEOGCS"));

        if( nGCSCode == 4326
            || EQUAL(pszGeogCSName, "WGS84")
            || EQUAL(pszGeogCSName, "WGS 84") )
        {
            poGeogCS->GetChild(0)->SetValue( "GCS_WGS_1984" );
            pszUTMPrefix = "WGS_1984";
        }
        else if( nGCSCode == 4322
            || EQUAL(pszGeogCSName, "WGS72")
            || EQUAL(pszGeogCSName, "WGS 72") )
        {
            poGeogCS->GetChild(0)->SetValue( "GCS_WGS_1972" );
            pszUTMPrefix = "WGS_1972";
        }
        else if( nGCSCode == 4267
                 || EQUAL(pszGeogCSName, "NAD27")
                 || EQUAL(pszGeogCSName, "NAD 27") )
        {
            poGeogCS->GetChild(0)->SetValue( "GCS_North_American_1927" );
            pszUTMPrefix = "NAD_1927";
        }
        else if( nGCSCode == 4269
                 || EQUAL(pszGeogCSName, "NAD83")
                 || EQUAL(pszGeogCSName, "NAD 83") )
        {
            poGeogCS->GetChild(0)->SetValue( "GCS_North_American_1983" );
            pszUTMPrefix = "NAD_1983";
        }
        else if( nGCSCode == 4167
                 || EQUAL(pszGeogCSName, "NZGD2000")
                 || EQUAL(pszGeogCSName, "NZGD 2000") )
        {
            poGeogCS->GetChild(0)->SetValue( "GCS_NZGD_2000" );
            pszUTMPrefix = "NZGD_2000";
        }
        else if( nGCSCode == 4272
                 || EQUAL(pszGeogCSName, "NZGD49")
                 || EQUAL(pszGeogCSName, "NZGD 49") )
        {
            poGeogCS->GetChild(0)->SetValue( "GCS_New_Zealand_1949" );
            pszUTMPrefix = "NZGD_1949";
        }

/* -------------------------------------------------------------------- */
/*      Force Unnamed to Unknown for most common locations.             */
/* -------------------------------------------------------------------- */
        static const char * const apszUnknownMapping[] = {
            "Unknown", "Unnamed",
            NULL, NULL
        };

        char **apszMap = const_cast<char **>(apszUnknownMapping);

        GetRoot()->applyRemapper("PROJCS", apszMap + 1, apszMap + 0, 2);
        GetRoot()->applyRemapper("GEOGCS", apszMap + 1, apszMap + 0, 2);
        GetRoot()->applyRemapper("DATUM", apszMap + 1, apszMap + 0, 2);
        GetRoot()->applyRemapper("SPHEROID", apszMap + 1, apszMap + 0, 2);
        GetRoot()->applyRemapper("PRIMEM", apszMap + 1, apszMap + 0, 2);

/* -------------------------------------------------------------------- */
/*      If the PROJCS name is unset, use the PROJECTION name in         */
/*      place of unknown, or unnamed.  At the request of Peng Gao.      */
/* -------------------------------------------------------------------- */
        if( (poProjCS = GetAttrNode( "PROJCS" )) != NULL )
            poProjCSNodeChild = poProjCS->GetChild(0);

        if( poProjCSNodeChild )
        {
            pszProjCSName = poProjCSNodeChild->GetValue();
            char *pszNewValue = CPLStrdup(pszProjCSName);
            MorphNameToESRI( &pszNewValue );
            poProjCSNodeChild->SetValue( pszNewValue );
            CPLFree( pszNewValue );
            pszProjCSName = poProjCSNodeChild->GetValue();
        }

        if( pszProjCSName != NULL
            && ( EQUAL(pszProjCSName, "unnamed")
                 || EQUAL(pszProjCSName, "unknown")
                 || EQUAL(pszProjCSName, "") ) )
        {
            if( GetAttrValue( "PROJECTION", 0 ) != NULL )
            {
                pszProjCSName = GetAttrValue( "PROJECTION", 0 );
                poProjCSNodeChild->SetValue( pszProjCSName );
            }
        }

/* -------------------------------------------------------------------- */
/*      Prepare very specific PROJCS names for UTM coordinate           */
/*      systems.                                                        */
/* -------------------------------------------------------------------- */
        int bNorth = FALSE;
        int nZone = 0;

        // Get zone from name first.
        if( pszProjCSName && STARTS_WITH_CI(pszProjCSName, "UTM Zone ") )
        {
            nZone = atoi(pszProjCSName+9);
            if( strstr(pszProjCSName, "North") )
                bNorth = TRUE;
        }

        // If can not get from the name, from the parameters.
        if( nZone <= 0 )
            nZone = GetUTMZone( &bNorth );

        if( nZone > 0 && pszUTMPrefix )
        {
            char szUTMName[128] = {};
            if( bNorth )
                snprintf( szUTMName, sizeof(szUTMName),
                          "%s_UTM_Zone_%dN", pszUTMPrefix, nZone );
            else
                snprintf( szUTMName, sizeof(szUTMName),
                          "%s_UTM_Zone_%dS", pszUTMPrefix, nZone );

            if( poProjCSNodeChild )
                poProjCSNodeChild->SetValue( szUTMName );
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate UNIT keywords that are misnamed, or even the wrong    */
/*      case.                                                           */
/* -------------------------------------------------------------------- */
    GetRoot()->applyRemapper( "UNIT",
                              const_cast<char **>(apszUnitMapping + 1),
                              const_cast<char **>(apszUnitMapping),
                              2 );

/* -------------------------------------------------------------------- */
/*      reset constants for decimal degrees to the exact string ESRI    */
/*      expects when encountered to ensure a matchup.                   */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poUnit = GetAttrNode( "GEOGCS|UNIT" );

    if( poUnit != NULL && poUnit->GetChildCount() >= 2
        && std::abs(GetAngularUnits()-0.0174532925199433) < 0.00000000001 )
    {
        poUnit->GetChild(0)->SetValue("Degree");
        poUnit->GetChild(1)->SetValue("0.017453292519943295");
    }

/* -------------------------------------------------------------------- */
/*      Make sure we reproduce US Feet exactly too.                     */
/* -------------------------------------------------------------------- */
    poUnit = GetAttrNode( "PROJCS|UNIT" );

    if( poUnit != NULL && poUnit->GetChildCount() >= 2
        && std::abs(GetLinearUnits()- 0.30480060960121924) < 0.000000000000001)
    {
        poUnit->GetChild(0)->SetValue("Foot_US");
        poUnit->GetChild(1)->SetValue("0.30480060960121924");
    }

/* -------------------------------------------------------------------- */
/*      Remap parameters used for Albers.                               */
/* -------------------------------------------------------------------- */
    // Do not remove the previous pszProjection assignment, for easy
    // maintainability.
    CPL_IGNORE_RET_VAL(pszProjection);
    pszProjection = GetAttrValue("PROJECTION");
    poProjCS = GetAttrNode( "PROJCS" );

    if( pszProjection != NULL && EQUAL(pszProjection, "Albers") )
        GetRoot()->applyRemapper(
            "PARAMETER", const_cast<char **>(apszAlbersMapping + 1),
            const_cast<char **>(apszAlbersMapping + 0), 2 );

    if( pszProjection != NULL
        && (EQUAL(pszProjection, SRS_PT_EQUIDISTANT_CONIC) ||
            EQUAL(pszProjection, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) ||
            EQUAL(pszProjection, SRS_PT_AZIMUTHAL_EQUIDISTANT) ||
            EQUAL(pszProjection, SRS_PT_SINUSOIDAL) ||
            EQUAL(pszProjection, SRS_PT_ROBINSON) ) )
        GetRoot()->applyRemapper(
            "PARAMETER", const_cast<char **>(apszECMapping + 1),
            const_cast<char **>(apszECMapping + 0), 2 );

    if( pszProjection != NULL
        && STARTS_WITH_CI(pszProjection, "Stereographic_")
        && STARTS_WITH_CI(pszProjection+strlen(pszProjection)-5, "_Pole") )
        GetRoot()->applyRemapper(
            "PARAMETER",
            const_cast<char **>(apszPolarStereographicMapping + 1),
            const_cast<char **>(apszPolarStereographicMapping + 0), 2);

    if( pszProjection != NULL && EQUAL(pszProjection, "Plate_Carree") )
        if( FindProjParm( SRS_PP_STANDARD_PARALLEL_1, poProjCS ) < 0 )
            GetRoot()->applyRemapper(
                "PARAMETER",
                const_cast<char **>(apszPolarStereographicMapping + 1),
                const_cast<char **>(apszPolarStereographicMapping + 0), 2);

/* -------------------------------------------------------------------- */
/*      ESRI's Equidistant_Cylindrical does not support the             */
/*      latitude_of_origin keyword.                                     */
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL
        && EQUAL(pszProjection, "Equidistant_Cylindrical") )
    {
        if( GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0) != 0.0 )
        {
            CPLDebug( "OGR_ESRI",
                      "Equirectangular with non-zero latitude of origin - "
                      "not supported." );
        }
        else
        {
            OGR_SRSNode *poPROJCS = GetAttrNode("PROJCS");
            if( poPROJCS )
                poPROJCS->DestroyChild(
                    FindProjParm( SRS_PP_LATITUDE_OF_ORIGIN ) );
        }
    }

    // See #4861.
    if( pszProjection != NULL && EQUAL(pszProjection, SRS_PT_MERCATOR_2SP) )
    {
        SetNode( "PROJCS|PROJECTION", "Mercator" );
        if( (poProjCS = GetAttrNode( "PROJCS" )) != NULL )
            poProjCSNodeChild = poProjCS->GetChild(0);
    }

    // See #4861.
    else if( pszProjection != NULL &&
             EQUAL(pszProjection, SRS_PT_MERCATOR_1SP) )
    {
        SetNode( "PROJCS|PROJECTION", "Mercator" );
        if( (poProjCS = GetAttrNode( "PROJCS" )) != NULL )
            poProjCSNodeChild = poProjCS->GetChild(0);

        const double dfK0 = GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);

        const double dfInvFlattening = GetInvFlattening();
        double e2 = 0.0;
        if( dfInvFlattening != 0.0 )
        {
            const double f = 1.0 / dfInvFlattening;
            e2 = 2 * f - f * f;
        }
        const double dfStdP1Lat =
            acos( sqrt( (1.0 - e2) / ((1.0 / (dfK0 * dfK0)) - e2)) ) /
            M_PI * 180.0;
        if( poProjCS )
        {
            const int iScaleFactorChild =
                FindProjParm( SRS_PP_SCALE_FACTOR, poProjCS );
            if( iScaleFactorChild != -1 )
                poProjCS->DestroyChild( iScaleFactorChild);
            SetProjParm(SRS_PP_STANDARD_PARALLEL_1, dfStdP1Lat);
            FixupOrdering();
        }
    }

/* -------------------------------------------------------------------- */
/*      Convert SPHEROID name to use underscores instead of spaces.     */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poSpheroidChild = NULL;
    OGR_SRSNode *poSpheroid = GetAttrNode( "SPHEROID" );
    if( poSpheroid != NULL )
        poSpheroidChild = poSpheroid->GetChild(0);

    if( poSpheroidChild != NULL )
    {
        // char *pszNewValue =
        //     CPLStrdup(RemapSpheroidName(poSpheroidChild->GetValue()));
        char *pszNewValue = CPLStrdup(poSpheroidChild->GetValue());

        MorphNameToESRI( &pszNewValue );

        poSpheroidChild->SetValue( pszNewValue );
        CPLFree( pszNewValue );

        GetRoot()->applyRemapper("SPHEROID",
                                 const_cast<char **>(apszSpheroidMapping + 0),
                                 const_cast<char **>(apszSpheroidMapping + 1),
                                 2);
    }

    if( poSpheroid != NULL )
        poSpheroidChild = poSpheroid->GetChild(2);

    if( poSpheroidChild != NULL )
    {
      const char * pszValue = poSpheroidChild->GetValue();
      for( int i = 0; apszInvFlatteningMapping[i] != NULL; i += 2 )
      {
        if( EQUALN(apszInvFlatteningMapping[i], pszValue,
                   strlen(apszInvFlatteningMapping[i]) ))
        {
          poSpheroidChild->SetValue( apszInvFlatteningMapping[i+1] );
          break;
        }
      }
    }

/* -------------------------------------------------------------------- */
/*      Try to insert a D_ in front of the datum name.                  */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poDatum = GetAttrNode( "DATUM" );
    if( poDatum != NULL )
        poDatum = poDatum->GetChild(0);

    if( poDatum != NULL )
    {
        const char* pszDatumName = poDatum->GetValue();
        if( !STARTS_WITH_CI(pszDatumName, "D_") )
        {
            char *pszNewValue = static_cast<char *>(
                CPLMalloc(strlen(poDatum->GetValue())+3) );
            strcpy( pszNewValue, "D_" );
            strcat( pszNewValue, poDatum->GetValue() );
            poDatum->SetValue( pszNewValue );
            CPLFree( pszNewValue );
        }
    }

/* -------------------------------------------------------------------- */
/*                        final check names                             */
/* -------------------------------------------------------------------- */
    if( poProjCSNodeChild )
        pszProjCSName = poProjCSNodeChild->GetValue();

    if( pszProjCSName )
    {
      const char *pszGcsName = GetAttrValue( "GEOGCS" );
      if( pszGcsName && !STARTS_WITH_CI(pszGcsName, "GCS_") )
      {
        char* newGcsName = static_cast<char *>(
            CPLMalloc(strlen(pszGcsName) + 5) );
        strcpy( newGcsName, "GCS_" );
        strcat(newGcsName, pszGcsName);
        SetNewName( this, "GEOGCS", newGcsName );
        CPLFree( newGcsName );
        pszGcsName = GetAttrValue( "GEOGCS" );
      }
      if( pszGcsName != NULL )
      {
        RemapGeogCSName(this, pszGcsName);
      }

      // Specific processing and remapping
      pszProjection = GetAttrValue("PROJECTION");
      if( pszProjection )
      {
        if( EQUAL(pszProjection, "Lambert_Conformal_Conic") )
        {
          if( FindProjParm( SRS_PP_STANDARD_PARALLEL_2, poProjCS ) < 0 )
          {
            int iChild = FindProjParm( SRS_PP_LATITUDE_OF_ORIGIN, poProjCS );
            int iChild1 = FindProjParm( SRS_PP_STANDARD_PARALLEL_1, poProjCS );
            if( iChild >= 0 && iChild1 < 0 )
            {
              const OGR_SRSNode *poParameter = poProjCS->GetChild(iChild);
              if( poParameter )
              {
                OGR_SRSNode *poNewParm = new OGR_SRSNode( "PARAMETER" );
                poNewParm->AddChild( new OGR_SRSNode( "standard_parallel_1" ) );
                poNewParm->AddChild( new OGR_SRSNode(
                    poParameter->GetChild(1)->GetValue() ) );
                poProjCS->AddChild( poNewParm );
              }
            }
          }
        }

        if( EQUAL(pszProjection, "Plate_Carree") )
        {
          int iChild = FindProjParm( SRS_PP_STANDARD_PARALLEL_1, poProjCS );
          if( iChild < 0 )
            iChild = FindProjParm( SRS_PP_PSEUDO_STD_PARALLEL_1, poProjCS );

          if( iChild >= 0 )
          {
            const OGR_SRSNode *poParameter = poProjCS->GetChild(iChild);
            if( !EQUAL(poParameter->GetChild(1)->GetValue(), "0.0") &&
                !EQUAL(poParameter->GetChild(1)->GetValue(), "0") )
            {
              SetNode( "PROJCS|PROJECTION", "Equidistant_Cylindrical" );
              pszProjection = GetAttrValue("PROJECTION");
            }
          }
        }

        DeleteParamBasedOnPrjName(
            this, pszProjection,
            const_cast<char **>(apszDeleteParametersBasedOnProjection));
        AddParamBasedOnPrjName(
            this, pszProjection,
            const_cast<char **>(apszAddParametersBasedOnProjection));
        RemapPValuesBasedOnProjCSAndPName(
            this, pszProjection,
            const_cast<char **>(apszParamValueMapping));
        RemapPNamesBasedOnProjCSAndPName(
            this, pszProjection,
            const_cast<char **>(apszParamNameMapping));
      }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRMorphToESRI()                           */
/************************************************************************/

/**
 * \brief Convert in place to ESRI WKT format.
 *
 * This function is the same as the C++ method
 * OGRSpatialReference::morphToESRI().
 */
OGRErr OSRMorphToESRI( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRMorphToESRI", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->morphToESRI();
}

/************************************************************************/
/*                           morphFromESRI()                            */
/*                                                                      */
/*      modify this definition from the ESRI definition of WKT to       */
/*      the "Standard" definition.                                      */
/************************************************************************/

/**
 * \brief Convert in place from ESRI WKT format.
 *
 * The value notes of this coordinate system are modified in various manners
 * to adhere more closely to the WKT standard.  This mostly involves
 * translating a variety of ESRI names for projections, arguments and
 * datums to "standard" names, as defined by Adam Gawne-Cain's reference
 * translation of EPSG to WKT for the CT specification.
 *
 * Starting with GDAL 1.9.0, missing parameters in TOWGS84, DATUM or GEOGCS
 * nodes can be added to the WKT, comparing existing WKT parameters to GDAL's
 * databases.  Note that this optional procedure is very conservative and should
 * not introduce false information into the WKT definition (although caution
 * should be advised when activating it). Needs the Configuration Option
 * GDAL_FIX_ESRI_WKT be set to one of the following values (TOWGS84 is
 * recommended for proper datum shift calculations):
 *
 * <b>GDAL_FIX_ESRI_WKT values</b>
 * <table border="0">
 * <tr><td>&nbsp;&nbsp;</td><td><b>TOWGS84</b></td><td>&nbsp;&nbsp;</td><td>
 * Adds missing TOWGS84 parameters (necessary for datum transformations),
 * based on named datum and spheroid values.</td></tr>
 * <tr><td>&nbsp;&nbsp;</td><td><b>DATUM</b></td><td>&nbsp;&nbsp;</td><td>
 * Adds EPSG AUTHORITY nodes and sets SPHEROID name to OGR spec.</td></tr>
 * <tr><td>&nbsp;&nbsp;</td><td><b>GEOGCS</b></td><td>&nbsp;&nbsp;</td><td>
 * Adds EPSG AUTHORITY nodes and sets GEOGCS, DATUM and SPHEROID
 * names to OGR spec. Effectively replaces GEOGCS node with the result of
 * importFromEPSG(n), using EPSG code n corresponding to the existing GEOGCS.
 * Does not impact PROJCS values.</td></tr>
 * </table>
 *
 * This does the same as the C function OSRMorphFromESRI().
 *
 * @return OGRERR_NONE unless something goes badly wrong.
 */

OGRErr OGRSpatialReference::morphFromESRI()

{
    if( GetRoot() == NULL )
        return OGRERR_NONE;

    InitDatumMappingTable();

/* -------------------------------------------------------------------- */
/*      Save original datum name for later                              */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poDatum = GetAttrNode( "DATUM" );
    char *pszDatumOrig = NULL;
    if( poDatum != NULL )
    {
        poDatum = poDatum->GetChild(0);
        pszDatumOrig = CPLStrdup( poDatum->GetValue() );
    }

/* -------------------------------------------------------------------- */
/*      Translate DATUM keywords that are oddly named.                  */
/* -------------------------------------------------------------------- */
    GetRoot()->applyRemapper( "DATUM",
                              const_cast<char **>(papszDatumMapping+1),
                              const_cast<char **>(papszDatumMapping+2), 3 );

/* -------------------------------------------------------------------- */
/*      Try to remove any D_ in front of the datum name.                */
/* -------------------------------------------------------------------- */
    poDatum = GetAttrNode( "DATUM" );
    if( poDatum != NULL )
        poDatum = poDatum->GetChild(0);

    if( poDatum != NULL )
    {
        if( STARTS_WITH_CI(poDatum->GetValue(), "D_") )
        {
            char *pszNewValue = CPLStrdup( poDatum->GetValue() + 2 );
            poDatum->SetValue( pszNewValue );
            CPLFree( pszNewValue );
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate some SPHEROID keywords that are oddly named.          */
/* -------------------------------------------------------------------- */
    GetRoot()->applyRemapper( "SPHEROID",
                              const_cast<char **>(apszSpheroidMapping+1),
                              const_cast<char **>(apszSpheroidMapping+0), 2 );

/* -------------------------------------------------------------------- */
/*      Split Lambert_Conformal_Conic into 1SP or 2SP form.             */
/*                                                                      */
/*      See bugzilla.remotesensing.org/show_bug.cgi?id=187              */
/*                                                                      */
/*      We decide based on whether it has 2SPs.  We used to assume      */
/*      1SP if it had a scale factor but that turned out to be a        */
/*      poor test.                                                      */
/* -------------------------------------------------------------------- */
    const char *pszProjection = GetAttrValue("PROJECTION");

    if( pszProjection != NULL
        && EQUAL(pszProjection, "Lambert_Conformal_Conic") )
    {
        if( GetProjParm( SRS_PP_STANDARD_PARALLEL_1, 1000.0 ) != 1000.0
            && GetProjParm( SRS_PP_STANDARD_PARALLEL_2, 1000.0 ) != 1000.0 )
            SetNode( "PROJCS|PROJECTION",
                     SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP );
        else
            SetNode( "PROJCS|PROJECTION",
                     SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP );

        pszProjection = GetAttrValue("PROJECTION");
    }

    if( pszProjection != NULL &&
             EQUAL(pszProjection, SRS_PT_MERCATOR_AUXILIARY_SPHERE) )
    {
        CPLFree( pszDatumOrig );
        return importFromEPSG(3857);
    }

/* -------------------------------------------------------------------- */
/*      If we are remapping Hotine_Oblique_Mercator_Azimuth_Center      */
/*      add a rectified_grid_angle parameter - to match the azimuth     */
/*      I guess.                                                        */
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL
        && EQUAL(pszProjection, "Hotine_Oblique_Mercator_Azimuth_Center") )
    {
        SetProjParm( SRS_PP_RECTIFIED_GRID_ANGLE ,
                     GetProjParm( SRS_PP_AZIMUTH, 0.0 ) );
        FixupOrdering();
    }

/* -------------------------------------------------------------------- */
/*      Remap Albers, Mercator and Polar Stereographic parameters.      */
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL && EQUAL(pszProjection, "Albers") )
        GetRoot()->applyRemapper(
            "PARAMETER", (char **)apszAlbersMapping + 0,
            (char **)apszAlbersMapping + 1, 2 );

    if( pszProjection != NULL
        && (EQUAL(pszProjection, SRS_PT_EQUIDISTANT_CONIC) ||
            EQUAL(pszProjection, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) ||
            EQUAL(pszProjection, SRS_PT_AZIMUTHAL_EQUIDISTANT) ||
            EQUAL(pszProjection, SRS_PT_SINUSOIDAL) ||
            EQUAL(pszProjection, SRS_PT_ROBINSON) ) )
        GetRoot()->applyRemapper(
            "PARAMETER", const_cast<char **>(apszECMapping + 0),
            const_cast<char **>(apszECMapping + 1), 2 );

    if( pszProjection != NULL && EQUAL(pszProjection, "Orthographic") )
        GetRoot()->applyRemapper(
            "PARAMETER", const_cast<char **>(apszOrthographicMapping + 0),
            const_cast<char **>(apszOrthographicMapping + 1), 2 );

    if( pszProjection != NULL
        && STARTS_WITH_CI(pszProjection, "Stereographic_")
        && STARTS_WITH_CI(pszProjection+strlen(pszProjection)-5, "_Pole") )
        GetRoot()->applyRemapper(
            "PARAMETER",
            const_cast<char **>(apszPolarStereographicMapping + 0),
            const_cast<char **>(apszPolarStereographicMapping + 1), 2 );

/* -------------------------------------------------------------------- */
/*      Remap south and north polar stereographic to one value.         */
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL
        && STARTS_WITH_CI(pszProjection, "Stereographic_")
        && STARTS_WITH_CI(pszProjection+strlen(pszProjection)-5, "_Pole") )
    {
        SetNode( "PROJCS|PROJECTION", SRS_PT_POLAR_STEREOGRAPHIC );
        pszProjection = GetAttrValue("PROJECTION");
    }

/* -------------------------------------------------------------------- */
/*      Remap Double_Stereographic to Oblique_Stereographic.            */
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL
        && EQUAL(pszProjection, "Double_Stereographic") )
    {
        SetNode( "PROJCS|PROJECTION", SRS_PT_OBLIQUE_STEREOGRAPHIC );
        pszProjection = GetAttrValue("PROJECTION");
    }

/* -------------------------------------------------------------------- */
/*      Remap Equidistant_Cylindrical parameter. It is same as          */
/*      Stereographic                                                   */
/* -------------------------------------------------------------------- */
#if 0
    if( pszProjection != NULL &&
        EQUAL(pszProjection, "Equidistant_Cylindrical") )
        GetRoot()->applyRemapper(
            "PARAMETER",
            const_cast<char **>(apszPolarStereographicMapping + 0),
            const_cast<char **>(apszPolarStereographicMapping + 1), 2 );
#endif

/* -------------------------------------------------------------------- */
/*      Remap Mercator to Mercator_2SP (#4861)                          */
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL && EQUAL(pszProjection, "Mercator") )
    {
        // Such as found in #6134.
        if( GetAttrValue("PROJCS") != NULL &&
            EQUAL(GetAttrValue("PROJCS"), "WGS_84_Pseudo_Mercator"))
        {
            CPLFree( pszDatumOrig );
            return importFromEPSG(3857);
        }

        SetNode( "PROJCS|PROJECTION", SRS_PT_MERCATOR_2SP );
        pszProjection = GetAttrValue("PROJECTION");
    }

    // Handle the value of Central_Parallel -> latitude_of_center.
    // See ticket #3191.  Other mappings probably need to be added.
    if( pszProjection != NULL &&
        ( EQUAL( pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP ) ||
          EQUAL( pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP ) ) )
    {
        GetRoot()->applyRemapper(
            "PARAMETER",
            const_cast<char **>(apszLambertConformalConicMapping + 0),
            const_cast<char **>(apszLambertConformalConicMapping + 1),
            2 );

        // LCC 1SP has duplicated parameters Standard_Parallel_1 and
        // Latitude_Of_Origin.
        // http://trac.osgeo.org/gdal/ticket/2072
        if( EQUAL( pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP ) )
        {
            OGR_SRSNode *poPROJCS = GetAttrNode("PROJCS");
            if( poPROJCS != NULL )
            {
                const int iSP1Child =
                    FindProjParm("Standard_Parallel_1", poPROJCS);
                const int iLatOrigChild =
                    FindProjParm("Latitude_Of_Origin", poPROJCS);
                if( iSP1Child != -1 && iLatOrigChild != -1 )
                {
                    // Do a sanity check before removing Standard_Parallel_1.
                    if( EQUAL(poPROJCS->GetChild(iSP1Child)->GetValue(),
                              poPROJCS->GetChild(iLatOrigChild)->GetValue()) )
                    {
                        poPROJCS->DestroyChild(iSP1Child);
                    }
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate PROJECTION keywords that are misnamed.                */
/* -------------------------------------------------------------------- */
    GetRoot()->applyRemapper( "PROJECTION",
                              const_cast<char **>(apszProjMapping),
                              const_cast<char **>(apszProjMapping+1),
                              2 );

/* -------------------------------------------------------------------- */
/*      Translate DATUM keywords that are misnamed.                     */
/* -------------------------------------------------------------------- */
    InitDatumMappingTable();

    GetRoot()->applyRemapper( "DATUM",
                              const_cast<char **>(papszDatumMapping+1),
                              const_cast<char **>(papszDatumMapping+2),
                              3 );

    // Refresh poDatum as the above SetNode() calls might have invalidated
    // it.
    poDatum = GetAttrNode( "DATUM" );
    if( poDatum != NULL )
        poDatum = poDatum->GetChild(0);

/* -------------------------------------------------------------------- */
/*      Special case for Peru96 related SRS that should use the         */
/*      Peru96 DATUM, but in ESRI world, both Peru96 and SIRGAS-Chile   */
/*      are translated as D_SIRGAS-Chile.                               */
/* -------------------------------------------------------------------- */
    bool bPeru96Datum = false;
    if( poDatum != NULL && EQUAL(poDatum->GetValue(), "SIRGAS_Chile") )
    {
        const char* pszSRSName = GetAttrValue("PROJCS");
        if( pszSRSName == NULL )
            pszSRSName = GetAttrValue("GEOGCS");
        if( pszSRSName != NULL && strstr(pszSRSName, "Peru96") )
        {
            bPeru96Datum = true;
            poDatum->SetValue( "Peru96" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Fix TOWGS84, DATUM or GEOGCS                                    */
/* -------------------------------------------------------------------- */
    // TODO test more ESRI WKT; also add PROJCS.

    // Check GDAL_FIX_ESRI_WKT config option (default=NO); if YES, set to DATUM.
    const char *pszFixWktConfig=CPLGetConfigOption( "GDAL_FIX_ESRI_WKT", "NO" );
    if( EQUAL(pszFixWktConfig, "YES") )
        pszFixWktConfig = "DATUM";

    if( !EQUAL(pszFixWktConfig, "NO") &&
        poDatum != NULL && pszDatumOrig != NULL )
    {
        CPLDebug( "OGR_ESRI",
                  "morphFromESRI() looking for missing TOWGS84, datum=%s, "
                  "config=%s",
                  pszDatumOrig, pszFixWktConfig );

        // Special case for WGS84 and other common GCS?
        for( int i = 0; DMGetESRIName(i) != NULL; i++ )
        {
            // Found the ESRI datum name in the map.
            if( EQUAL(DMGetESRIName(i), pszDatumOrig) )
            {
                const char *pszFilename = NULL;
                char **papszRecord = NULL;

                // Look for GEOGCS corresponding to this datum.
                pszFilename = CSVFilename("gcs.csv");
                papszRecord = CSVScanFileByName( pszFilename, "DATUM_CODE",
                                                 DMGetEPSGCode(i), CC_Integer );
                if( papszRecord != NULL )
                {
                    // Skip the SIRGAS-Chile record for Peru96 related SRS.
                    if( bPeru96Datum &&
                        EQUAL(CSLGetField(papszRecord,
                                          CSVGetFileFieldId(pszFilename,
                                                            "DATUM_NAME")),
                              "SIRGAS-Chile") )
                        continue;

                    // Ensure we got a valid EPSG code and it is not DEPRECATED.
                    const int nGeogCS = atoi(
                        CSLGetField(papszRecord,
                                    CSVGetFileFieldId(pszFilename,
                                                      "COORD_REF_SYS_CODE")));
                    // int bDeprecated =
                    //     atoi( CSLGetField( papszRecord,
                    //                        CSVGetFileFieldId(pszFilename,
                    //                        "DEPRECATED")) );

                    CPLDebug( "OGR_ESRI", "morphFromESRI() got GEOGCS node #%d",
                              nGeogCS );

                    // if( nGeogCS >= 1 && bDeprecated == 0 )
                    if( nGeogCS >= 1 )
                    {
                        OGRSpatialReference oSRSTemp;
                        if( oSRSTemp.importFromEPSG( nGeogCS ) == OGRERR_NONE )
                        {
                            // Clone of GEOGCS and strip CT parms for testing.
                            char *pszOtherValue = NULL;
                            OGR_SRSNode *poNode = NULL;

                            OGRSpatialReference *poSRSTemp2 =
                                oSRSTemp.CloneGeogCS();
                            poSRSTemp2->StripCTParms();
                            bool bIsSame =
                                CPL_TO_BOOL(IsSameGeogCS( poSRSTemp2 ));
                            exportToWkt ( &pszOtherValue );
                            CPLDebug("OGR_ESRI",
                                     "morphFromESRI() got SRS %s, matching: %d",
                                     pszOtherValue, static_cast<int>(bIsSame));
                            CPLFree( pszOtherValue );
                            delete poSRSTemp2;

                            // Clone GEOGCS from original if they match and if
                            // allowed.
                            if( EQUAL(pszFixWktConfig, "GEOGCS")
                                 && bIsSame )
                            {
                                CopyGeogCSFrom( &oSRSTemp );
                                CPLDebug( "OGR_ESRI",
                                          "morphFromESRI() cloned GEOGCS "
                                          "from EPSG:%d",
                                          nGeogCS );
                                // Exit loop.
                                break;
                            }
                            // Else try to copy only DATUM or TOWGS84.  We got
                            // here either because of config option or GEOGCS
                            // are not strictly equal.
                            else if( EQUAL(pszFixWktConfig, "GEOGCS") ||
                                     EQUAL(pszFixWktConfig, "DATUM") ||
                                     EQUAL(pszFixWktConfig, "TOWGS84") )
                            {
                                // Test for matching SPHEROID, because there can
                                // be 2 datums with same ESRI name but different
                                // spheroids (e.g. EPSG:4618 and EPSG:4291).
                                // See bug #4345.

                                // Instead of testing for matching SPHEROID name
                                // (which can be error-prone), test for matching
                                // parameters (semi-major and inverse
                                // flattening).  See bug #4673
                                bIsSame = true;
                                double dfThisValue = GetSemiMajor();
                                double dfOtherValue = oSRSTemp.GetSemiMajor();
                                if( std::abs( dfThisValue -
                                              dfOtherValue ) > 0.01 )
                                    bIsSame = false;
                                CPLDebug( "OGR_ESRI",
                                          "morphFromESRI() SemiMajor: "
                                          "this = %.15g other = %.15g",
                                          dfThisValue, dfOtherValue );
                                dfThisValue = GetInvFlattening();
                                dfOtherValue = oSRSTemp.GetInvFlattening();
                                if( std::abs( dfThisValue -
                                              dfOtherValue ) > 0.0001 )
                                    bIsSame = false;
                                CPLDebug( "OGR_ESRI",
                                          "morphFromESRI() InvFlattening: "
                                          "this = %g other = %g",
                                          dfThisValue, dfOtherValue );

                                if( bIsSame )
                                {
                                    // Test for matching PRIMEM, because there
                                    // can be 2 datums with same ESRI name but
                                    // different prime meridian (e.g. EPSG:4218
                                    // and EPSG:4802) - see bug #4378.

                                    // Instead of testing for matching PRIMEM
                                    // name (which can be error-prone), test for
                                    // matching value - see bug #4673.
                                    dfThisValue = GetPrimeMeridian();
                                    dfOtherValue = oSRSTemp.GetPrimeMeridian();
                                    CPLDebug( "OGR_ESRI",
                                              "morphFromESRI() PRIMEM: "
                                              "this = %.15g other = %.15g",
                                              dfThisValue, dfOtherValue );
                                    if( std::abs( dfThisValue - dfOtherValue )
                                        > 0.0001 )
                                        bIsSame = false;
                                }

                                // Found a matching spheroid.
                                if( bIsSame )
                                {
                                    // Clone DATUM.
                                    if( EQUAL(pszFixWktConfig, "GEOGCS") ||
                                        EQUAL(pszFixWktConfig, "DATUM") )
                                    {
                                        OGR_SRSNode *poGeogCS =
                                            GetAttrNode("GEOGCS");
                                        const OGR_SRSNode *poDatumOther =
                                            oSRSTemp.GetAttrNode( "DATUM" );
                                        if( poGeogCS && poDatumOther )
                                        {
                                            // Make sure we preserve the
                                            // position of the DATUM node.
                                            const int nPos =
                                                poGeogCS->FindChild( "DATUM" );
                                            if( nPos >= 0 )
                                            {
                                                poGeogCS->DestroyChild( nPos );
                                                poGeogCS->InsertChild(
                                                    poDatumOther->Clone(),
                                                    nPos );
                                                CPLDebug(
                                                    "OGR_ESRI",
                                                    "morphFromESRI() cloned "
                                                    "DATUM from EPSG:%d",
                                                    nGeogCS );
                                            }
                                        }
                                    }
                                    // Just copy TOWGS84.
                                    else if( EQUAL(pszFixWktConfig, "TOWGS84") )
                                    {
                                        poNode =
                                            oSRSTemp.GetAttrNode(
                                                "DATUM|TOWGS84");
                                        if( poNode )
                                        {
                                            poNode=poNode->Clone();
                                            GetAttrNode("DATUM")->
                                                AddChild(poNode);
                                            CPLDebug(
                                                "OGR_ESRI",
                                                "morphFromESRI() found missing "
                                                "TOWGS84 from EPSG:%d",
                                                nGeogCS );
                                        }
                                    }
                                    // Exit loop.
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    CPLFree( pszDatumOrig );

    // TODO: Somehow track issues.
    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRMorphFromESRI()                          */
/************************************************************************/

/**
 * \brief Convert in place from ESRI WKT format.
 *
 * This function is the same as the C++ method
 * OGRSpatialReference::morphFromESRI().
 */
OGRErr OSRMorphFromESRI( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRMorphFromESRI", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->morphFromESRI();
}

/************************************************************************/
/*                       FindCodeFromDict()                             */
/*                                                                      */
/*      Find the code from a dict file.                                 */
/************************************************************************/
static int FindCodeFromDict( const char* pszDictFile, const char* CSName,
                             char* code )
{
/* -------------------------------------------------------------------- */
/*      Find and open file.                                             */
/* -------------------------------------------------------------------- */
    const char *pszFilename = CPLFindFile( "gdal", pszDictFile );
    if( pszFilename == NULL )
        return OGRERR_UNSUPPORTED_SRS;

    VSILFILE *fp = VSIFOpenL( pszFilename, "rb" );
    if( fp == NULL )
        return OGRERR_UNSUPPORTED_SRS;

/* -------------------------------------------------------------------- */
/*      Process lines.                                                  */
/* -------------------------------------------------------------------- */
    OGRErr eErr = OGRERR_UNSUPPORTED_SRS;
    const char *pszLine = NULL;

    while( (pszLine = CPLReadLineL(fp)) != NULL )
    {
        if( pszLine[0] == '#' )
            continue;

        if( strstr(pszLine, CSName) )
        {
            const char* pComma = strchr(pszLine, ',');
            if( pComma )
            {
                strncpy( code, pszLine, pComma - pszLine);
                code[pComma - pszLine] = '\0';
                eErr = OGRERR_NONE;
            }
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFCloseL( fp );

    return eErr;
}

/************************************************************************/
/*                    ImportFromESRIStatePlaneWKT()                     */
/*                                                                      */
/*      Search a ESRI State Plane WKT and import it.                    */
/************************************************************************/

OGRErr OGRSpatialReference::ImportFromESRIStatePlaneWKT(
    int code, const char* datumName, const char* unitsName,
    int pcsCode, const char* csName )
{
    // If the CS name is known.
    if( code == 0 && !datumName && !unitsName && pcsCode == 32767 && csName )
    {
        char codeS[10] = {};
        if( FindCodeFromDict( "esri_StatePlane_extra.wkt", csName, codeS )
            != OGRERR_NONE )
            return OGRERR_FAILURE;
        return importFromDict( "esri_StatePlane_extra.wkt", codeS);
    }

    int searchCode = -1;
    if( unitsName == NULL )
        unitsName = "";

    // Find state plane prj str by pcs code only.
    if( code == 0 && !datumName && pcsCode != 32767 )
    {
        int unitCode = 1;
        if( EQUAL(unitsName, "international_feet") )
            unitCode = 3;
        else if( strstr(unitsName, "feet") || strstr(unitsName, "foot") )
            unitCode = 2;

        for( int i = 0; statePlanePcsCodeToZoneCode[i] != 0; i += 2 )
        {
            if( pcsCode == statePlanePcsCodeToZoneCode[i] )
            {
                searchCode = statePlanePcsCodeToZoneCode[i+1];
                const int unitIndex = searchCode % 10;
                if( (unitCode == 1 && !(unitIndex == 0 || unitIndex == 1))
                    || (unitCode == 2 && !(unitIndex == 2 || unitIndex == 3 ||
                                           unitIndex == 4 ))
                    || (unitCode == 3 && !(unitIndex == 5 || unitIndex == 6 )) )
                {
                    searchCode -= unitIndex;
                    switch( unitIndex )
                    {
                      case 0:
                      case 3:
                      case 5:
                        if( unitCode == 2 )
                            searchCode += 3;
                        else if( unitCode == 3 )
                            searchCode += 5;
                        break;
                      case 1:
                      case 2:
                      case 6:
                        if( unitCode == 1 )
                            searchCode += 1;
                        if( unitCode == 2 )
                            searchCode += 2;
                        else if( unitCode == 3 )
                            searchCode += 6;
                        break;
                      case 4:
                        // FIXME? The following cond is not possible:
                        // if( unitCode == 2 )
                        //     searchCode += 4;
                        break;
                    }
                }
                break;
            }
        }
    }
    else // Find state plane prj str by all inputs.
    {
        if( code < 0 || code > INT_MAX / 10 )
            return OGRERR_FAILURE;

        // Need to have a special EPSG-ESRI zone code mapping first.
        for( int i = 0; statePlaneZoneMapping[i] != 0; i += 3 )
        {
            if( code == statePlaneZoneMapping[i]
                && (statePlaneZoneMapping[i+1] == -1 ||
                    pcsCode == statePlaneZoneMapping[i+1]))
            {
                code = statePlaneZoneMapping[i+2];
                break;
            }
        }
        searchCode = code * 10;
        if( !datumName )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "datumName is NULL.");
            return OGRERR_FAILURE;
        }
        if( EQUAL(datumName, "HARN") )
        {
            if( EQUAL(unitsName, "international_feet") )
                searchCode += 5;
            else if( strstr(unitsName, "feet") || strstr(unitsName, "foot") )
                searchCode += 3;
        }
        else if( strstr(datumName, "NAD") && strstr(datumName, "83") )
        {
            if( EQUAL(unitsName, "meters") )
                searchCode += 1;
            else if( EQUAL(unitsName, "international_feet") )
                searchCode += 6;
            else if( strstr(unitsName, "feet") || strstr(unitsName, "foot") )
                searchCode += 2;
        }
        else if( strstr(datumName, "NAD") && strstr(datumName, "27") &&
                 !EQUAL(unitsName, "meters") )
        {
            searchCode += 4;
        }
        else
            searchCode = -1;
    }
    if( searchCode > 0 )
    {
        char codeS[10] = {};
        snprintf(codeS, sizeof(codeS), "%d", (int)searchCode);
        return importFromDict( "esri_StatePlane_extra.wkt", codeS);
    }
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                     ImportFromESRIWisconsinWKT()                     */
/*                                                                      */
/*      Search a ESRI State Plane WKT and import it.                    */
/************************************************************************/

OGRErr OGRSpatialReference::ImportFromESRIWisconsinWKT(
    const char* prjName, double centralMeridian, double latOfOrigin,
    const char* unitsName, const char* csName )
{
    // If the CS name is known.
    if( !prjName && !unitsName && csName )
    {
        char codeS[10] = {};
        if( FindCodeFromDict( "esri_Wisconsin_extra.wkt", csName,
                              codeS ) != OGRERR_NONE )
            return OGRERR_FAILURE;
        return importFromDict("esri_Wisconsin_extra.wkt", codeS);
    }
    const double* tableWISCRS = NULL;
    if( prjName != NULL && STARTS_WITH_CI(prjName, "Lambert_Conformal_Conic") )
        tableWISCRS = apszWISCRS_LCC_meter;
    else if( prjName != NULL && EQUAL(prjName, SRS_PT_TRANSVERSE_MERCATOR) )
        tableWISCRS = apszWISCRS_TM_meter;
    else
        return OGRERR_FAILURE;
    int k = -1;
    for( int i = 0; tableWISCRS[i] != 0; i += 3 )
    {
        if( fabs(centralMeridian - tableWISCRS[i]) <= 0.0000000001 &&
            fabs(latOfOrigin - tableWISCRS[i+1]) <= 0.0000000001 )
        {
            k = static_cast<int>(tableWISCRS[i+2]); // double to int?
            break;
        }
    }
    if( k > 0 )
    {
        if( unitsName != NULL && !EQUAL(unitsName, "meters") )
            k += 100;
        char codeS[15] = { '\0' };
        snprintf(codeS, sizeof(codeS), "%d", k);
        return importFromDict( "esri_Wisconsin_extra.wkt", codeS);
    }
    return OGRERR_FAILURE;
}
