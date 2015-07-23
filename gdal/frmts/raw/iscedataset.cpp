/******************************************************************************
 * $Id$
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

#include "rawdataset.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_ISCE(void);
CPL_C_END

static const char *papszISCE2GDALDatatypes[] = {
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
    NULL };

static const char *papszGDAL2ISCEDatatypes[] = {
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
    NULL };

enum Scheme { BIL = 0, BIP = 1, BSQ = 2 };
const char *papszSchemeNames[] = { "BIL", "BIP", "BSQ", NULL };

/************************************************************************/
/* ==================================================================== */
/*                              ISCEDataset                             */
/* ==================================================================== */
/************************************************************************/

class ISCERasterBand;

class ISCEDataset : public RawDataset
{
    friend class ISCERasterBand;

    VSILFILE    *fpImage;
    
    char        *pszXMLFilename;

    enum Scheme eScheme;

  public:
                ISCEDataset( void );
                ~ISCEDataset( void );

    virtual void        FlushCache( void );
    virtual char      **GetFileList( void );
    
    static int          Identify( GDALOpenInfo *poOpenInfo );
    static GDALDataset *Open( GDALOpenInfo *poOpenInfo );
    static GDALDataset *Create( const char *pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char **papszOptions );
};

/************************************************************************/
/* ==================================================================== */
/*                            ISCERasterBand                            */
/* ==================================================================== */
/************************************************************************/

class ISCERasterBand : public RawRasterBand
{
    public:
                ISCERasterBand( GDALDataset *poDS, int nBand, void *fpRaw,
                                  vsi_l_offset nImgOffset, int nPixelOffset,
                                  int nLineOffset,
                                  GDALDataType eDataType, int bNativeOrder,
                                  int bIsVSIL = FALSE, int bOwnsFP = FALSE );
};

/************************************************************************/
/*                           getXMLFilename()                           */
/************************************************************************/

static CPLString getXMLFilename( GDALOpenInfo *poOpenInfo )
{
    CPLString osXMLFilename;

    char **papszSiblingFiles = poOpenInfo->GetSiblingFiles();
    if ( papszSiblingFiles == NULL )
    {
        osXMLFilename = CPLFormFilename( NULL, poOpenInfo->pszFilename, 
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
        CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
        CPLString osName = CPLGetFilename( poOpenInfo->pszFilename );

        int iFile = CSLFindString( papszSiblingFiles,
                                   CPLFormFilename( NULL, osName, "xml" ) );
        if( iFile >= 0 )
        {
            osXMLFilename = CPLFormFilename( osPath,
                                             papszSiblingFiles[iFile],
                                             NULL );
        }
    }

    return osXMLFilename;
}

/************************************************************************/
/*                             ISCEDataset()                            */
/************************************************************************/

ISCEDataset::ISCEDataset( void )
{
    fpImage = NULL;
    pszXMLFilename = NULL;
}

/************************************************************************/
/*                            ~ISCEDataset()                          */
/************************************************************************/

ISCEDataset::~ISCEDataset( void )
{
    FlushCache();
    if ( fpImage != NULL )
    {
        VSIFCloseL( fpImage );
    }
    CPLFree( pszXMLFilename );
}

/************************************************************************/
/*                            FlushCache()                              */
/************************************************************************/

void ISCEDataset::FlushCache( void )
{
    RawDataset::FlushCache();

    GDALRasterBand *band = (GetRasterCount() > 0) ? GetRasterBand(1) : NULL;

    if ( eAccess == GA_ReadOnly || band == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Recreate a XML doc with the dataset information.                */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psDocNode, *psTmpNode;
    char sBuf[64];
    psDocNode = CPLCreateXMLNode( NULL, CXT_Element, "imageFile" );

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "WIDTH" );
    snprintf(sBuf, sizeof(sBuf), "%d", nRasterXSize);
    CPLCreateXMLElementAndValue( psTmpNode, "value", sBuf );

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "LENGTH" );
    snprintf(sBuf, sizeof(sBuf), "%d", nRasterYSize);
    CPLCreateXMLElementAndValue( psTmpNode, "value", sBuf );

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "NUMBER_BANDS" );
    snprintf(sBuf, sizeof(sBuf), "%d", nBands);
    CPLCreateXMLElementAndValue( psTmpNode, "value", sBuf );

    const char *sType = GDALGetDataTypeName( band->GetRasterDataType() );
    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "DATA_TYPE" );
    CPLCreateXMLElementAndValue( psTmpNode, "value", 
                                 CSLFetchNameValue(
                                         (char **)papszGDAL2ISCEDatatypes, 
                                         sType ) );

    const char *sScheme = papszSchemeNames[eScheme];
    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "SCHEME" );
    CPLCreateXMLElementAndValue( psTmpNode, "value", sScheme );

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "BYTE_ORDER" );
#ifdef CPL_LSB
    CPLCreateXMLElementAndValue( psTmpNode, "value", "l" );
#else
    CPLCreateXMLElementAndValue( psTmpNode, "value", "b" );
#endif

/* -------------------------------------------------------------------- */
/*      Then, add the ISCE domain metadata.                             */
/* -------------------------------------------------------------------- */
    char **papszISCEMetadata = GetMetadata( "ISCE" );
    for (int i = 0; i < CSLCount( papszISCEMetadata ); i++)
    {
        char **papszTokens;

        /* Get the tokens from the metadata item */
        papszTokens = CSLTokenizeString2( papszISCEMetadata[i],
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
        if ( strcmp( papszTokens[0], "WIDTH" ) == 0
              || strcmp( papszTokens[0], "LENGTH" ) == 0
              || strcmp( papszTokens[0], "NUMBER_BANDS" ) == 0
              || strcmp( papszTokens[0], "DATA_TYPE" ) == 0
              || strcmp( papszTokens[0], "SCHEME" ) == 0 
              || strcmp( papszTokens[0], "BYTE_ORDER" ) == 0 )
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
    char **papszFileList = NULL;

    /* Main data file, etc. */
    papszFileList = RawDataset::GetFileList();

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
/*            the presence of a XML file                                */
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
    bool bNativeOrder = true;

/* -------------------------------------------------------------------- */
/*      Confirm that the header is compatible with a ISCE dataset.    */
/* -------------------------------------------------------------------- */
    if ( !Identify(poOpenInfo) )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open and parse the .xml file                                    */
/* -------------------------------------------------------------------- */
    char **papszXmlProps = NULL;
    CPLString osXMLFilename = getXMLFilename( poOpenInfo );
    CPLXMLNode *psNode = CPLParseXMLFile( osXMLFilename );
    if ( psNode == NULL || CPLGetXMLNode( psNode, "=imageFile" ) == NULL )
    {
        CPLDestroyXMLNode( psNode );
        return NULL;
    }
    CPLXMLNode *psCur  = CPLGetXMLNode( psNode, "=imageFile" )->psChild;
    while ( psCur != NULL ) {
        const char *name, *value;
        if ( strcmp(psCur->pszValue, "property") != 0) {
            psCur = psCur->psNext;
            continue;
        }
        name = CPLGetXMLValue( psCur, "name", NULL );
        value = CPLGetXMLValue( psCur, "value.", NULL );
        papszXmlProps = CSLSetNameValue( papszXmlProps,
                                         name, value );
        psCur = psCur->psNext;
    }
    /* TODO: extract <component name=Coordinate[12]> for georeferencing */
    CPLDestroyXMLNode( psNode );

/* -------------------------------------------------------------------- */
/*      Fetch required fields.                                          */
/* -------------------------------------------------------------------- */
    int nWidth = 0, nFileLength = 0;
    if ( CSLFetchNameValue( papszXmlProps, "WIDTH" ) == NULL
        || CSLFetchNameValue( papszXmlProps, "LENGTH" ) == NULL
        || CSLFetchNameValue( papszXmlProps, "NUMBER_BANDS" ) == NULL
        || CSLFetchNameValue( papszXmlProps, "DATA_TYPE" ) == NULL
        || CSLFetchNameValue( papszXmlProps, "SCHEME" ) == NULL )
    {
        CSLDestroy( papszXmlProps );
        return NULL;
    }
    nWidth = atoi( CSLFetchNameValue( papszXmlProps, "WIDTH" ) );
    nFileLength = atoi( CSLFetchNameValue( papszXmlProps, "LENGTH" ) );

/* -------------------------------------------------------------------- */
/*      Update byte order info if image specify something.              */
/* -------------------------------------------------------------------- */
    if ( CSLFetchNameValue( papszXmlProps, "BYTE_ORDER" ) != NULL )
    {
        const char *sByteOrder = CSLFetchNameValue( papszXmlProps,
                                                    "BYTE_ORDER" );
#ifdef CPL_LSB
        if ( EQUAL( sByteOrder, "b" ) )
#else
        if ( EQUAL( sByteOrder, "l" ) )
#endif
            bNativeOrder = false;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ISCEDataset *poDS;
    poDS = new ISCEDataset();
    poDS->nRasterXSize = nWidth;
    poDS->nRasterYSize = nFileLength;
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->pszXMLFilename = CPLStrdup( osXMLFilename.c_str() );

/* -------------------------------------------------------------------- */
/*      Reopen file in update mode if necessary.                        */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb+" );
    }
    else
    {
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    }
    if( poDS->fpImage == NULL )
    {
        CSLDestroy( papszXmlProps );
        delete poDS;
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to re-open %s within ISCE driver.\n",
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    const char *sDataType =
        CSLFetchNameValue( (char **)papszISCE2GDALDatatypes,
                           CSLFetchNameValue( papszXmlProps, "DATA_TYPE" ) );
    GDALDataType eDataType = GDALGetDataTypeByName( sDataType );
    int nBands = atoi( CSLFetchNameValue( papszXmlProps, "NUMBER_BANDS" ) );
    const char *sScheme = CSLFetchNameValue( papszXmlProps, "SCHEME" );
    int nPixelOffset, nLineOffset, nBandOffset;
    if ( EQUAL( sScheme, "BIL" ) )
    {
		poDS->eScheme = BIL;
        nPixelOffset = GDALGetDataTypeSize(eDataType)/8;
        nLineOffset = nPixelOffset * nWidth * nBands;
        nBandOffset = GDALGetDataTypeSize(eDataType)/8 * nWidth;
    }
    else if ( EQUAL( sScheme, "BIP" ) )
    {
		poDS->eScheme = BIP;
        nPixelOffset = GDALGetDataTypeSize(eDataType)/8 * nBands;
        nLineOffset = nPixelOffset * nWidth * nBands;
        nBandOffset = GDALGetDataTypeSize(eDataType)/8;
    }
    else if ( EQUAL( sScheme, "BSQ" ) )
    {
		poDS->eScheme = BSQ;
        nPixelOffset = GDALGetDataTypeSize(eDataType)/8;
        nLineOffset = nPixelOffset * nWidth;
        nBandOffset = nLineOffset * nFileLength;
    }
    else
    {
        CSLDestroy( papszXmlProps );
        delete poDS;
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unkown scheme \"%s\" within ISCE raster.\n",
                  CSLFetchNameValue( papszXmlProps, "SCHEME" ) );
        return NULL;
    }
    poDS->nBands = nBands;
    for (int b = 0; b < nBands; b++)
    {
        poDS->SetBand( b + 1,
                       new ISCERasterBand( poDS, b + 1, poDS->fpImage,
                                             nBandOffset * b,
                                             nPixelOffset, nLineOffset,
                                             eDataType, TRUE,
                                             bNativeOrder, FALSE ) );
    }

/* -------------------------------------------------------------------- */
/*      Interpret georeferencing, if present.                           */
/* -------------------------------------------------------------------- */
   /* TODO */

/* -------------------------------------------------------------------- */
/*      Set all the other header metadata into the ISCE domain       */
/* -------------------------------------------------------------------- */
    for (int i = 0; i < CSLCount( papszXmlProps ); i++)
    {
        char **papszTokens;
        papszTokens = CSLTokenizeString2( papszXmlProps[i],
                                          "=",
                                          CSLT_STRIPLEADSPACES
                                            | CSLT_STRIPENDSPACES);
        if ( strcmp( papszTokens[0], "WIDTH" ) == 0
              || strcmp( papszTokens[0], "LENGTH" ) == 0
              || strcmp( papszTokens[0], "NUMBER_BANDS" ) == 0
              || strcmp( papszTokens[0], "DATA_TYPE" ) == 0
              || strcmp( papszTokens[0], "SCHEME" ) == 0 )
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

    return( poDS );
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
    const char *sScheme = CSLFetchNameValueDef( papszOptions,
                                                "SCHEME",
                                                "BIP" );

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    VSILFILE *fp;
    fp = VSIFOpenL( pszFilename, "wb" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Just write out a couple of bytes to establish the binary        */
/*      file, and then close it.                                        */
/* -------------------------------------------------------------------- */
    VSIFWriteL( (void *) "\0\0", 2, 1, fp );
    VSIFCloseL( fp );

/* -------------------------------------------------------------------- */
/*      Create a minimal XML document.                                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psDocNode, *psTmpNode;
    char sBuf[64];
    psDocNode = CPLCreateXMLNode( NULL, CXT_Element, "imageFile" );

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "WIDTH" );
    snprintf(sBuf, sizeof(sBuf), "%d", nXSize);
    CPLCreateXMLElementAndValue( psTmpNode, "value", sBuf );

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "LENGTH" );
    snprintf(sBuf, sizeof(sBuf), "%d", nYSize);
    CPLCreateXMLElementAndValue( psTmpNode, "value", sBuf );

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "NUMBER_BANDS" );
    snprintf(sBuf, sizeof(sBuf), "%d", nBands);
    CPLCreateXMLElementAndValue( psTmpNode, "value", sBuf );

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "DATA_TYPE" );
    CPLCreateXMLElementAndValue( psTmpNode, "value", 
                                 CSLFetchNameValue(
                                         (char **)papszGDAL2ISCEDatatypes, 
                                         sType ));

    psTmpNode = CPLCreateXMLNode( psDocNode, CXT_Element, "property" );
    CPLAddXMLAttributeAndValue( psTmpNode, "name", "SCHEME" );
    CPLCreateXMLElementAndValue( psTmpNode, "value", sScheme );

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
    const char  *pszXMLFilename;
    pszXMLFilename = CPLFormFilename( NULL, pszFilename, "xml" );
    CPLSerializeXMLTreeToFile( psDocNode, pszXMLFilename );

/* -------------------------------------------------------------------- */
/*      Free the XML Doc.                                               */
/* -------------------------------------------------------------------- */
    CPLDestroyXMLNode( psDocNode );

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                          ISCERasterBand()                            */
/************************************************************************/

ISCERasterBand::ISCERasterBand( GDALDataset *poDS, int nBand, void *fpRaw,
                                    vsi_l_offset nImgOffset, int nPixelOffset,
                                    int nLineOffset,
                                    GDALDataType eDataType, int bNativeOrder,
                                    int bIsVSIL, int bOwnsFP ) :
        RawRasterBand(poDS, nBand, fpRaw, nImgOffset, nPixelOffset,
                      nLineOffset, eDataType, bNativeOrder, bIsVSIL, bOwnsFP)
{
}

/************************************************************************/
/*                         GDALRegister_ISCE()                          */
/************************************************************************/

void GDALRegister_ISCE( void )
{
    GDALDriver  *poDriver;

    if ( !GDAL_CHECK_VERSION( "ISCE" ) )
    {
        return;
    }

    if ( GDALGetDriverByName( "ISCE" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "ISCE" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "ISCE raster" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#ISCE" );
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
}
