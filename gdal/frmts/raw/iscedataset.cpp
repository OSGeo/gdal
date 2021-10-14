/******************************************************************************
 *
 * Project:  ISCE Raster Reader
 * Purpose:  Implementation of the ISCE raster reader
 * Author:   Matthieu Volat (ISTerre), matthieu.volat@ujf-grenoble.fr
 *
 ******************************************************************************
 * Copyright (c) 2015, Matthieu Volat <matthieu.volat@ujf-grenoble.fr>
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

#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

CPL_CVSID("$Id$")

static const char * const apszISCE2GDALDatatypes[] = {
    "BYTE:Byte",
    "CHAR:Byte",
    "SHORT:Int16",
    "INT:Int32",
    "LONG:Int64",
    "FLOAT:Float32",
    "DOUBLE:Float64",
    "CBYTE:Unknown",
    "CCHAR:Unknown",
    "CSHORT:CInt16",
    "CINT:CInt32",
    "CLONG:CInt64",
    "CFLOAT:CFloat32",
    "CDOUBLE:CFloat64",
    nullptr };

static const char * const apszGDAL2ISCEDatatypes[] = {
    "Byte:BYTE",
    "Int16:SHORT",
    "Int32:INT",
    "Int64:LONG",
    "Float32:FLOAT",
    "Float64:DOUBLE",
    "CInt16:CSHORT",
    "CInt32:CINT",
    "CInt64:CLONG",
    "CFloat32:CFLOAT",
    "CFloat64:CDOUBLE",
    nullptr };

enum Scheme { BIL = 0, BIP = 1, BSQ = 2 };
static const char * const apszSchemeNames[] = { "BIL", "BIP", "BSQ", nullptr };

/************************************************************************/
/* ==================================================================== */
/*                              ISCEDataset                             */
/* ==================================================================== */
/************************************************************************/

class ISCERasterBand;

class ISCEDataset final: public RawDataset
{
    friend class ISCERasterBand;

    VSILFILE    *fpImage;

    char        *pszXMLFilename;

    enum Scheme eScheme;

    CPL_DISALLOW_COPY_ASSIGN(ISCEDataset)

  public:
    ISCEDataset();
    ~ISCEDataset() override;

    void FlushCache(bool bAtClosing) override;
    char **GetFileList() override;

    static int          Identify( GDALOpenInfo *poOpenInfo );
    static GDALDataset *Open( GDALOpenInfo *poOpenInfo );
    static GDALDataset *Open( GDALOpenInfo *poOpenInfo, bool bFileSizeCheck );
    static GDALDataset *Create( const char *pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char **papszOptions );
};

/************************************************************************/
/* ==================================================================== */
/*                            ISCERasterBand                            */
/* ==================================================================== */
/************************************************************************/

class ISCERasterBand final: public RawRasterBand
{
        CPL_DISALLOW_COPY_ASSIGN(ISCERasterBand)

    public:
                ISCERasterBand( GDALDataset *poDS, int nBand, VSILFILE *fpRaw,
                                  vsi_l_offset nImgOffset, int nPixelOffset,
                                  int nLineOffset,
                                  GDALDataType eDataType, int bNativeOrder );
};

/************************************************************************/
/*                           getXMLFilename()                           */
/************************************************************************/

static CPLString getXMLFilename( GDALOpenInfo *poOpenInfo )
{
    CPLString osXMLFilename;

    if( poOpenInfo->fpL == nullptr )
        return CPLString();

    char **papszSiblingFiles = poOpenInfo->GetSiblingFiles();
    if ( papszSiblingFiles == nullptr )
    {
        osXMLFilename = CPLFormFilename( nullptr, poOpenInfo->pszFilename,
                                         "xml" );
        VSIStatBufL psXMLStatBuf;
        if ( VSIStatL( osXMLFilename, &psXMLStatBuf ) != 0 )
        {
            osXMLFilename = "";
        }
    }
    else
    {
        /* ------------------------------------------------------------ */
        /*      We need to tear apart the filename to form a .xml       */
        /*      filename.                                               */
        /* ------------------------------------------------------------ */
        const CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
        const CPLString osName = CPLGetFilename( poOpenInfo->pszFilename );

        const int iFile =
            CSLFindString( papszSiblingFiles,
                           CPLFormFilename( nullptr, osName, "xml" ) );
        if( iFile >= 0 )
        {
            osXMLFilename = CPLFormFilename( osPath,
                                             papszSiblingFiles[iFile],
                                             nullptr );
        }
    }

    return osXMLFilename;
}

/************************************************************************/
/*                             ISCEDataset()                            */
/************************************************************************/

ISCEDataset::ISCEDataset() :
    fpImage(nullptr),
    pszXMLFilename(nullptr),
    eScheme(BIL)
{}

/************************************************************************/
/*                            ~ISCEDataset()                          */
/************************************************************************/

ISCEDataset::~ISCEDataset( void )
{
    ISCEDataset::FlushCache(true);
    if ( fpImage != nullptr )
    {
        if( VSIFCloseL( fpImage ) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        }
    }
    CPLFree( pszXMLFilename );
}

/************************************************************************/
/*                            FlushCache()                              */
/************************************************************************/

void ISCEDataset::FlushCache(bool bAtClosing)
{
    RawDataset::FlushCache(bAtClosing);

    GDALRasterBand *band = (GetRasterCount() > 0) ? GetRasterBand(1) : nullptr;

    if ( eAccess == GA_ReadOnly || band == nullptr )
        return;

/* -------------------------------------------------------------------- */
/*      Recreate a XML doc with the dataset information.                */
/* -------------------------------------------------------------------- */
    char sBuf[64] = { '\0' };
    CPLXMLNode *psDocNode = CPLCreateXMLNode( nullptr, CXT_Element, "imageFile" );

    CPLXMLNode *psTmpNode
        = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "WIDTH" );
    CPLsnprintf(sBuf, sizeof(sBuf), "%d", nRasterXSize);
    CPLCreateXMLElementAndValue( psTmpNode, "value", sBuf );

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "LENGTH" );
    CPLsnprintf(sBuf, sizeof(sBuf), "%d", nRasterYSize);
    CPLCreateXMLElementAndValue( psTmpNode, "value", sBuf );

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "NUMBER_BANDS" );
    CPLsnprintf(sBuf, sizeof(sBuf), "%d", nBands);
    CPLCreateXMLElementAndValue( psTmpNode, "value", sBuf );

    const char *sType = GDALGetDataTypeName( band->GetRasterDataType() );
    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "DATA_TYPE" );
    CPLCreateXMLElementAndValue(
        psTmpNode, "value",
        CSLFetchNameValue(
            const_cast<char **>(apszGDAL2ISCEDatatypes),
            sType ) );

    const char *pszScheme = apszSchemeNames[eScheme];
    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "SCHEME" );
    CPLCreateXMLElementAndValue( psTmpNode, "value", pszScheme );

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "BYTE_ORDER" );
#ifdef CPL_LSB
    CPLCreateXMLElementAndValue( psTmpNode, "value", "l" );
#else
    CPLCreateXMLElementAndValue( psTmpNode, "value", "b" );
#endif

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "ACCESS_MODE" );
    CPLCreateXMLElementAndValue( psTmpNode, "value", "read" );

    const char *pszFilename = CPLGetBasename( pszXMLFilename );
    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "FILE_NAME" );
    CPLCreateXMLElementAndValue( psTmpNode, "value", pszFilename );

/* -------------------------------------------------------------------- */
/*      Then, add the ISCE domain metadata.                             */
/* -------------------------------------------------------------------- */
    char **papszISCEMetadata = GetMetadata( "ISCE" );
    for (int i = 0; i < CSLCount( papszISCEMetadata ); i++)
    {
        /* Get the tokens from the metadata item */
        char **papszTokens = CSLTokenizeString2( papszISCEMetadata[i],
                                                 "=",
                                                 CSLT_STRIPLEADSPACES
                                                 | CSLT_STRIPENDSPACES);
        if ( CSLCount( papszTokens ) != 2 )
        {
            CPLDebug( "ISCE",
                      "Line of header file could not be split at = into two"
                      " elements: %s",
                      papszISCEMetadata[i] );
            CSLDestroy( papszTokens );
            continue;
        }

        /* Don't write it out if it is one of the bits of metadata that is
         * written out elsewhere in this routine */
        if ( EQUAL( papszTokens[0], "WIDTH" )
              || EQUAL( papszTokens[0], "LENGTH" )
              || EQUAL( papszTokens[0], "NUMBER_BANDS" )
              || EQUAL( papszTokens[0], "DATA_TYPE" )
              || EQUAL( papszTokens[0], "SCHEME" )
              || EQUAL( papszTokens[0], "BYTE_ORDER" ) )
        {
            CSLDestroy( papszTokens );
            continue;
        }

        psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
        CPLAddXMLAttributeAndValue( psTmpNode, "name", papszTokens[0] );
        CPLCreateXMLElementAndValue( psTmpNode, "value", papszTokens[1] );

        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Create the "Coordinate" component elements, possibly with       */
/*      georeferencing.                                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psCoordinate1Node, *psCoordinate2Node;
    double adfGeoTransform[6];

    /* Coordinate 1 */
    psCoordinate1Node = CPLCreateXMLNode( psDocNode,
                                          CXT_Element,
                                          "component" );
    CPLAddXMLAttributeAndValue( psCoordinate1Node, "name", "Coordinate1" );
    CPLCreateXMLElementAndValue( psCoordinate1Node,
                                 "factorymodule",
                                 "isceobj.Image" );
    CPLCreateXMLElementAndValue( psCoordinate1Node,
                                 "factoryname",
                                 "createCoordinate" );
    CPLCreateXMLElementAndValue( psCoordinate1Node,
                                 "doc",
                                 "First coordinate of a 2D image (width)." );
    /* Property name */
    psTmpNode = CPLCreateXMLNode( psCoordinate1Node,
                                  CXT_Element,
                                  "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "name" );
    CPLCreateXMLElementAndValue( psTmpNode,
                                 "value",
                                 "ImageCoordinate_name" );
    /* Property family */
    psTmpNode = CPLCreateXMLNode( psCoordinate1Node,
                                  CXT_Element,
                                  "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "family" );
    CPLCreateXMLElementAndValue( psTmpNode,
                                 "value",
                                 "ImageCoordinate" );
    /* Property size */
    CPLsnprintf(sBuf, sizeof(sBuf), "%d", nRasterXSize);
    psTmpNode = CPLCreateXMLNode( psCoordinate1Node,
                                  CXT_Element,
                                  "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "size" );
    CPLCreateXMLElementAndValue( psTmpNode,
                                 "value",
                                 sBuf );

    /* Coordinate 2 */
    psCoordinate2Node = CPLCreateXMLNode( psDocNode,
                                          CXT_Element,
                                          "component" );
    CPLAddXMLAttributeAndValue( psCoordinate2Node, "name", "Coordinate2" );
    CPLCreateXMLElementAndValue( psCoordinate2Node,
                                 "factorymodule",
                                 "isceobj.Image" );
    CPLCreateXMLElementAndValue( psCoordinate2Node,
                                 "factoryname",
                                 "createCoordinate" );
    /* Property name */
    psTmpNode = CPLCreateXMLNode( psCoordinate2Node,
                                  CXT_Element,
                                  "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "name" );
    CPLCreateXMLElementAndValue( psTmpNode,
                                 "value",
                                 "ImageCoordinate_name" );
    /* Property family */
    psTmpNode = CPLCreateXMLNode( psCoordinate2Node,
                                  CXT_Element,
                                  "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "family" );
    CPLCreateXMLElementAndValue( psTmpNode,
                                 "value",
                                 "ImageCoordinate" );
    /* Property size */
    CPLsnprintf(sBuf, sizeof(sBuf), "%d", nRasterYSize);
    psTmpNode = CPLCreateXMLNode( psCoordinate2Node,
                                  CXT_Element,
                                  "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "size" );
    CPLCreateXMLElementAndValue( psTmpNode,
                                 "value",
                                 sBuf );

    if ( GetGeoTransform( adfGeoTransform ) == CE_None )
    {
        if ( adfGeoTransform[2] != 0 || adfGeoTransform[4] != 0 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "ISCE format do not support geotransform with "
                          "rotation, discarding info.");
        }
        else {
            CPLsnprintf( sBuf, sizeof(sBuf), "%g", adfGeoTransform[0] );
            psTmpNode = CPLCreateXMLNode( psCoordinate1Node,
                                          CXT_Element,
                                          "property" );
            CPLAddXMLAttributeAndValue( psTmpNode, "name", "startingValue" );
            CPLCreateXMLElementAndValue( psTmpNode,
                                         "value",
                                         sBuf );

            CPLsnprintf( sBuf, sizeof(sBuf), "%g", adfGeoTransform[1] );
            psTmpNode = CPLCreateXMLNode( psCoordinate1Node,
                                          CXT_Element,
                                          "property" );
            CPLAddXMLAttributeAndValue( psTmpNode, "name", "delta" );
            CPLCreateXMLElementAndValue( psTmpNode,
                                         "value",
                                         sBuf );

            CPLsnprintf( sBuf, sizeof(sBuf), "%g", adfGeoTransform[3] );
            psTmpNode = CPLCreateXMLNode( psCoordinate2Node,
                                          CXT_Element,
                                          "property" );
            CPLAddXMLAttributeAndValue( psTmpNode, "name", "startingValue" );
            CPLCreateXMLElementAndValue( psTmpNode,
                                         "value",
                                         sBuf );

            CPLsnprintf( sBuf, sizeof(sBuf), "%g", adfGeoTransform[5] );
            psTmpNode = CPLCreateXMLNode( psCoordinate2Node,
                                          CXT_Element,
                                          "property" );
            CPLAddXMLAttributeAndValue( psTmpNode, "name", "delta" );
            CPLCreateXMLElementAndValue( psTmpNode,
                                         "value",
                                         sBuf );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write the XML file.                                             */
/* -------------------------------------------------------------------- */
    CPLSerializeXMLTreeToFile( psDocNode, pszXMLFilename );

/* -------------------------------------------------------------------- */
/*      Free the XML Doc.                                               */
/* -------------------------------------------------------------------- */
    CPLDestroyXMLNode( psDocNode );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **ISCEDataset::GetFileList()
{
    /* Main data file, etc. */
    char **papszFileList = RawDataset::GetFileList();

    /* XML file. */
    papszFileList = CSLAddString( papszFileList, pszXMLFilename );

    return papszFileList;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int ISCEDataset::Identify( GDALOpenInfo *poOpenInfo )
{
/* -------------------------------------------------------------------- */
/*      TODO: This function is unusable now:                            */
/*          * we can't just check for the presence of a XML file        */
/*          * we cannot parse it to check basic tree (Identify() is     */
/*            supposed to be faster than this                           */
/*          * we could read only a few bytes and strstr() for           */
/*            "imageData", but what if a file is padded with comments   */
/*            and/or whitespaces? it would still be legit, but the      */
/*            driver would fail...                                      */
/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */
/*      Check if there is a .xml file                                   */
/* -------------------------------------------------------------------- */
    CPLString osXMLFilename = getXMLFilename( poOpenInfo );
    if ( osXMLFilename.empty() )
    {
        return false;
    }

    return true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ISCEDataset::Open( GDALOpenInfo *poOpenInfo )
{
    return Open(poOpenInfo, true);
}

GDALDataset *ISCEDataset::Open( GDALOpenInfo *poOpenInfo, bool bFileSizeCheck )
{
/* -------------------------------------------------------------------- */
/*      Confirm that the header is compatible with a ISCE dataset.    */
/* -------------------------------------------------------------------- */
    if ( !Identify(poOpenInfo) || poOpenInfo->fpL == nullptr )
    {
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Open and parse the .xml file                                    */
/* -------------------------------------------------------------------- */
    const CPLString osXMLFilename = getXMLFilename( poOpenInfo );
    CPLXMLNode *psNode = CPLParseXMLFile( osXMLFilename );
    if ( psNode == nullptr || CPLGetXMLNode( psNode, "=imageFile" ) == nullptr )
    {
        CPLDestroyXMLNode( psNode );
        return nullptr;
    }
    CPLXMLNode *psCur  = CPLGetXMLNode( psNode, "=imageFile" )->psChild;
    char **papszXmlProps = nullptr;
    while ( psCur != nullptr )
    {
        if ( EQUAL( psCur->pszValue, "property" ) )
        {
            /* Top-level property */
            const char *pszName = CPLGetXMLValue( psCur, "name", nullptr );
            const char *pszValue = CPLGetXMLValue( psCur, "value", nullptr );
            if ( pszName != nullptr && pszValue != nullptr)
            {
                papszXmlProps = CSLSetNameValue( papszXmlProps,
                                                 pszName, pszValue );
            }
        }
        else if ( EQUAL( psCur->pszValue, "component" ) )
        {
            /* "components" elements in ISCE store set of properties.   */
            /* For now, they are avoided as I am not sure the full      */
            /* scope of these. An exception is made for the ones named  */
            /* Coordinate1 and Coordinate2, because they may have the   */
            /* georeferencing information.                              */
            const char *pszCurName = CPLGetXMLValue( psCur, "name", nullptr );
            if ( pszCurName != nullptr
                && ( EQUAL( pszCurName, "Coordinate1" )
                    || EQUAL( pszCurName, "Coordinate2" ) ) )
            {
                /* We need two subproperties: startingValue and delta.  */
                /* To simplify parsing code, we will store them in      */
                /* papszXmlProps with the coordinate name prefixed to   */
                /* the property name.                                   */
                CPLXMLNode *psCur2 = psCur->psChild;
                while ( psCur2 != nullptr )
                {
                    if ( ! EQUAL( psCur2->pszValue, "property" ) )
                    {
                        psCur2 = psCur2->psNext;
                        continue; /* Skip non property elements */
                    }

                    const char
                       *pszCur2Name = CPLGetXMLValue( psCur2, "name", nullptr ),
                       *pszCur2Value = CPLGetXMLValue( psCur2, "value", nullptr );

                    if ( pszCur2Name == nullptr || pszCur2Value == nullptr )
                    {
                        psCur2 = psCur2->psNext;
                        continue; /* Skip malformatted elements */
                    }

                    if ( EQUAL( pszCur2Name, "startingValue" )
                        || EQUAL( pszCur2Name, "delta" ) )
                    {
                        char szPropName[32];
                        snprintf(szPropName, sizeof(szPropName), "%s%s",
                                 pszCurName, pszCur2Name);

                        papszXmlProps =
                            CSLSetNameValue( papszXmlProps,
                                             szPropName,
                                             pszCur2Value );
                    }
                    psCur2 = psCur2->psNext;
                }
            }
        }
        psCur = psCur->psNext;
    }

    CPLDestroyXMLNode( psNode );

/* -------------------------------------------------------------------- */
/*      Fetch required fields.                                          */
/* -------------------------------------------------------------------- */
    if ( CSLFetchNameValue( papszXmlProps, "WIDTH" ) == nullptr
        || CSLFetchNameValue( papszXmlProps, "LENGTH" ) == nullptr
        || CSLFetchNameValue( papszXmlProps, "NUMBER_BANDS" ) == nullptr
        || CSLFetchNameValue( papszXmlProps, "DATA_TYPE" ) == nullptr
        || CSLFetchNameValue( papszXmlProps, "SCHEME" ) == nullptr )
    {
        CSLDestroy( papszXmlProps );
        return nullptr;
    }
    const int nWidth = atoi( CSLFetchNameValue( papszXmlProps, "WIDTH" ) );
    const int nHeight = atoi( CSLFetchNameValue( papszXmlProps, "LENGTH" ) );
    const int nBands = atoi( CSLFetchNameValue( papszXmlProps, "NUMBER_BANDS" ) );

    if (!GDALCheckDatasetDimensions(nWidth, nHeight) ||
        !GDALCheckBandCount(nBands, FALSE))
    {
        CSLDestroy( papszXmlProps );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Update byte order info if image specify something.              */
/* -------------------------------------------------------------------- */
    bool bNativeOrder = true;

    const char *pszByteOrder = CSLFetchNameValue( papszXmlProps,
                                                    "BYTE_ORDER" );
    if ( pszByteOrder != nullptr )
    {
#ifdef CPL_LSB
        if ( EQUAL( pszByteOrder, "b" ) )
#else
        if ( EQUAL( pszByteOrder, "l" ) )
#endif
            bNativeOrder = false;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ISCEDataset *poDS = new ISCEDataset();
    poDS->nRasterXSize = nWidth;
    poDS->nRasterYSize = nHeight;
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->pszXMLFilename = CPLStrdup( osXMLFilename.c_str() );
    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    const char *pszDataType =
        CSLFetchNameValue( const_cast<char **>(apszISCE2GDALDatatypes),
                           CSLFetchNameValue( papszXmlProps, "DATA_TYPE" ) );
    if( pszDataType == nullptr )
    {
        delete poDS;
        CSLDestroy( papszXmlProps );
        return nullptr;
    }
    const GDALDataType eDataType = GDALGetDataTypeByName( pszDataType );
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    if( nDTSize == 0 )
    {
        delete poDS;
        CSLDestroy( papszXmlProps );
        return nullptr;
    }
    const char *pszScheme = CSLFetchNameValue( papszXmlProps, "SCHEME" );
    int nPixelOffset = 0;
    int nLineOffset = 0;
    vsi_l_offset nBandOffset = 0;
    bool bIntOverflow = false;
    if( EQUAL( pszScheme, "BIL" ) )
    {
        poDS->eScheme = BIL;
        nPixelOffset = nDTSize;
        if( nWidth > INT_MAX / (nPixelOffset * nBands) )
            bIntOverflow = true;
        else
        {
            nLineOffset = nPixelOffset * nWidth * nBands;
            nBandOffset = nDTSize * static_cast<vsi_l_offset>(nWidth);
        }
    }
    else if( EQUAL( pszScheme, "BIP" ) )
    {
        poDS->eScheme = BIP;
        nPixelOffset = nDTSize * nBands;
        if( nWidth > INT_MAX / nPixelOffset )
            bIntOverflow = true;
        else
        {
            nLineOffset = nPixelOffset * nWidth;
            if( nBands > 1 && nLineOffset < INT_MAX / nBands )
            {
                // GDAL 2.1.0 had a value of nLineOffset that was equal to the theoretical
                // nLineOffset multiplied by nBands...
                VSIFSeekL( poDS->fpImage, 0, SEEK_END );
                const GUIntBig nWrongFileSize = nDTSize *
                    nWidth * (static_cast<GUIntBig>(nHeight - 1) *
                    nBands * nBands + nBands);
                if( VSIFTellL( poDS->fpImage ) == nWrongFileSize )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "This file has been incorrectly generated by an older "
                            "GDAL version whose line offset computation was erroneous. "
                            "Taking that into account, but the file should be re-encoded ideally");
                    nLineOffset = nLineOffset * nBands;
                }
            }
            nBandOffset = nDTSize;
        }
    }
    else if ( EQUAL( pszScheme, "BSQ" ) )
    {
        poDS->eScheme = BSQ;
        nPixelOffset = nDTSize;
        if( nWidth > INT_MAX / nPixelOffset )
            bIntOverflow = true;
        else
        {
            nLineOffset = nPixelOffset * nWidth;
            nBandOffset = nLineOffset * static_cast<vsi_l_offset>(nHeight);
        }
    }
    else
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unknown scheme \"%s\" within ISCE raster.",
                  pszScheme );
        CSLDestroy( papszXmlProps );
        delete poDS;
        return nullptr;
    }

    if (bIntOverflow)
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Int overflow occurred." );
        CSLDestroy( papszXmlProps );
        return nullptr;
    }

    if( bFileSizeCheck &&
        !RAWDatasetCheckMemoryUsage(
                        poDS->nRasterXSize, poDS->nRasterYSize, nBands,
                        nDTSize,
                        nPixelOffset, nLineOffset, 0, nBandOffset,
                        poDS->fpImage) )
    {
        delete poDS;
        CSLDestroy( papszXmlProps );
        return nullptr;
    }

    poDS->nBands = nBands;
    for (int b = 0; b < nBands; b++)
    {
        poDS->SetBand( b + 1,
                       new ISCERasterBand( poDS, b + 1, poDS->fpImage,
                                           nBandOffset * b,
                                           nPixelOffset, nLineOffset,
                                           eDataType, bNativeOrder ) );
    }

/* -------------------------------------------------------------------- */
/*      Interpret georeferencing, if present.                           */
/* -------------------------------------------------------------------- */
    if ( CSLFetchNameValue( papszXmlProps, "Coordinate1startingValue" ) != nullptr
         && CSLFetchNameValue( papszXmlProps, "Coordinate1delta" ) != nullptr
         && CSLFetchNameValue( papszXmlProps, "Coordinate2startingValue" ) != nullptr
         && CSLFetchNameValue( papszXmlProps, "Coordinate2delta" ) != nullptr )
    {
        double adfGeoTransform[6];
        adfGeoTransform[0] = CPLAtof( CSLFetchNameValue( papszXmlProps,
                                                         "Coordinate1startingValue" ) );
        adfGeoTransform[1] = CPLAtof( CSLFetchNameValue( papszXmlProps,
                                                         "Coordinate1delta" ) );
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = CPLAtof( CSLFetchNameValue( papszXmlProps,
                                                         "Coordinate2startingValue" ) );
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = CPLAtof( CSLFetchNameValue( papszXmlProps,
                                                               "Coordinate2delta" ) );
        poDS->SetGeoTransform( adfGeoTransform );

        /* ISCE format seems not to have a projection field, but uses   */
        /* WGS84.                                                       */
        poDS->SetProjection( SRS_WKT_WGS84_LAT_LONG );
    }

/* -------------------------------------------------------------------- */
/*      Set all the other header metadata into the ISCE domain          */
/* -------------------------------------------------------------------- */
    for( int i = 0; papszXmlProps != nullptr && papszXmlProps[i] != nullptr; i++ )
    {
        char **papszTokens =
            CSLTokenizeString2( papszXmlProps[i],
                                "=",
                                CSLT_STRIPLEADSPACES
                                | CSLT_STRIPENDSPACES);
        if ( CSLCount(papszTokens) < 2
              || EQUAL( papszTokens[0], "WIDTH" )
              || EQUAL( papszTokens[0], "LENGTH" )
              || EQUAL( papszTokens[0], "NUMBER_BANDS" )
              || EQUAL( papszTokens[0], "DATA_TYPE" )
              || EQUAL( papszTokens[0], "SCHEME" )
              || EQUAL( papszTokens[0], "BYTE_ORDER" )
              || EQUAL( papszTokens[0], "Coordinate1startingValue" )
              || EQUAL( papszTokens[0], "Coordinate1delta" )
              || EQUAL( papszTokens[0], "Coordinate2startingValue" )
              || EQUAL( papszTokens[0], "Coordinate2delta" ) )
        {
            CSLDestroy( papszTokens );
            continue;
        }
        poDS->SetMetadataItem(papszTokens[0], papszTokens[1], "ISCE");
        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Free papszXmlProps                                              */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszXmlProps );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

GDALDataset *ISCEDataset::Create( const char *pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType,
                                  char **papszOptions )
{
    const char *sType = GDALGetDataTypeName( eType );
    const char *pszScheme = CSLFetchNameValueDef( papszOptions,
                                                "SCHEME",
                                                "BIP" );

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( pszFilename, "wb" );
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.",
                  pszFilename );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Just write out a couple of bytes to establish the binary        */
/*      file, and then close it.                                        */
/* -------------------------------------------------------------------- */
    CPL_IGNORE_RET_VAL(VSIFWriteL( "\0\0", 2, 1, fp ));
    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));

/* -------------------------------------------------------------------- */
/*      Create a minimal XML document.                                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psDocNode = CPLCreateXMLNode( nullptr, CXT_Element, "imageFile" );

    CPLXMLNode *psTmpNode
        = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "WIDTH" );
    char sBuf[64] = { '\0' };
    CPLsnprintf(sBuf, sizeof(sBuf), "%d", nXSize);
    CPLCreateXMLElementAndValue( psTmpNode, "value", sBuf );

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "LENGTH" );
    CPLsnprintf(sBuf, sizeof(sBuf), "%d", nYSize);
    CPLCreateXMLElementAndValue( psTmpNode, "value", sBuf );

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "NUMBER_BANDS" );
    CPLsnprintf(sBuf, sizeof(sBuf), "%d", nBands);
    CPLCreateXMLElementAndValue( psTmpNode, "value", sBuf );

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "DATA_TYPE" );
    CPLCreateXMLElementAndValue(
        psTmpNode, "value",
        CSLFetchNameValue(
            const_cast<char **>(apszGDAL2ISCEDatatypes),
            sType ));

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "SCHEME" );
    CPLCreateXMLElementAndValue( psTmpNode, "value", pszScheme );

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "BYTE_ORDER" );
#ifdef CPL_LSB
    CPLCreateXMLElementAndValue( psTmpNode, "value", "l" );
#else
    CPLCreateXMLElementAndValue( psTmpNode, "value", "b" );
#endif

/* -------------------------------------------------------------------- */
/*      Write the XML file.                                             */
/* -------------------------------------------------------------------- */
    const char  *pszXMLFilename = CPLFormFilename( nullptr, pszFilename, "xml" );
    CPLSerializeXMLTreeToFile( psDocNode, pszXMLFilename );

/* -------------------------------------------------------------------- */
/*      Free the XML Doc.                                               */
/* -------------------------------------------------------------------- */
    CPLDestroyXMLNode( psDocNode );

    GDALOpenInfo oOpenInfo( pszFilename, GA_Update );
    return Open(&oOpenInfo, false);
}

/************************************************************************/
/*                          ISCERasterBand()                            */
/************************************************************************/

ISCERasterBand::ISCERasterBand( GDALDataset *poDSIn, int nBandIn, VSILFILE *fpRawIn,
                                vsi_l_offset nImgOffsetIn, int nPixelOffsetIn,
                                int nLineOffsetIn,
                                GDALDataType eDataTypeIn, int bNativeOrderIn ) :
    RawRasterBand( poDSIn, nBandIn, fpRawIn, nImgOffsetIn, nPixelOffsetIn,
                   nLineOffsetIn, eDataTypeIn, bNativeOrderIn,
                   RawRasterBand::OwnFP::NO )
{}

/************************************************************************/
/*                         GDALRegister_ISCE()                          */
/************************************************************************/

void GDALRegister_ISCE()
{
    if( GDALGetDriverByName( "ISCE" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "ISCE" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "ISCE raster" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/isce.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 Int32 Int64 Float32"
                               " Float64 CInt16 CInt64 CFloat32 "
                               " CFloat64" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
                               "<CreationOptionList>"
                               "   <Option name='SCHEME' type='string-select'>"
                               "       <Value>BIP</Value>"
                               "       <Value>BIL</Value>"
                               "       <Value>BSQ</Value>"
                               "   </Option>"
                               "</CreationOptionList>" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = ISCEDataset::Open;
    poDriver->pfnCreate = ISCEDataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
