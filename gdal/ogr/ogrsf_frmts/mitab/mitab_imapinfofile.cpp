/**********************************************************************
 *
 * Name:     mitab_imapinfo
 * Project:  MapInfo mid/mif Tab Read/Write library
 * Language: C++
 * Purpose:  Implementation of the IMapInfoFile class, super class of
 *           of MIFFile and TABFile
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2008, Daniel Morissette
 * Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "cpl_port.h"
#include "mitab.h"

#include <cassert>
#include <cctype>
#include <cstring>
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "mitab_priv.h"
#include "mitab_utils.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"

#ifdef __HP_aCC
#  include <wchar.h>      /* iswspace() */
#else
#  include <wctype.h>      /* iswspace() */
#endif

CPL_CVSID("$Id$")

/**********************************************************************
 *                   IMapInfoFile::IMapInfoFile()
 *
 * Constructor.
 **********************************************************************/
IMapInfoFile::IMapInfoFile() :
    m_nCurFeatureId(0),
    m_poCurFeature(nullptr),
    m_bBoundsSet(FALSE),
    m_pszCharset(nullptr)
{}

/**********************************************************************
 *                   IMapInfoFile::~IMapInfoFile()
 *
 * Destructor.
 **********************************************************************/
IMapInfoFile::~IMapInfoFile()
{
    if( m_poCurFeature )
    {
        delete m_poCurFeature;
        m_poCurFeature = nullptr;
    }

    CPLFree(m_pszCharset);
    m_pszCharset = nullptr;
}

/**********************************************************************
 *                   IMapInfoFile::Open()
 *
 * Compatibility layer with new interface.
 * Return 0 on success, -1 in case of failure.
 **********************************************************************/

int IMapInfoFile::Open(const char *pszFname, const char* pszAccess,
                       GBool bTestOpenNoError, const char* pszCharset )
{
    // cppcheck-suppress nullPointer
    if( STARTS_WITH_CI(pszAccess, "r") )
        return Open(pszFname, TABRead, bTestOpenNoError, pszCharset);
    else if( STARTS_WITH_CI(pszAccess, "w") )
        return Open(pszFname, TABWrite, bTestOpenNoError, pszCharset);
    else
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: access mode \"%s\" not supported", pszAccess);
        return -1;
    }
}

/**********************************************************************
 *                   IMapInfoFile::SmartOpen()
 *
 * Use this static method to automatically open any flavor of MapInfo
 * dataset.  This method will detect the file type, create an object
 * of the right type, and open the file.
 *
 * Call GetFileClass() on the returned object if you need to find out
 * its exact type.  (To access format-specific methods for instance)
 *
 * Returns the new object ptr. , or NULL if the open failed.
 **********************************************************************/
IMapInfoFile *IMapInfoFile::SmartOpen(const char *pszFname,
                                      GBool bUpdate,
                                      GBool bTestOpenNoError /*=FALSE*/)
{
    IMapInfoFile *poFile = nullptr;
    int nLen = 0;

    if (pszFname)
        nLen = static_cast<int>(strlen(pszFname));

    if (nLen > 4 && (EQUAL(pszFname + nLen-4, ".MIF") ||
                     EQUAL(pszFname + nLen-4, ".MID") ) )
    {
        /*-------------------------------------------------------------
         * MIF/MID file
         *------------------------------------------------------------*/
        poFile = new MIFFile;
    }
    else if (nLen > 4 && EQUAL(pszFname + nLen-4, ".TAB"))
    {
        /*-------------------------------------------------------------
         * .TAB file ... is it a TABFileView or a TABFile?
         * We have to read the .tab header to find out.
         *------------------------------------------------------------*/
        char *pszAdjFname = CPLStrdup(pszFname);
        GBool bFoundFields = FALSE;
        GBool bFoundView = FALSE;
        GBool bFoundSeamless = FALSE;

        TABAdjustFilenameExtension(pszAdjFname);
        VSILFILE *fp = VSIFOpenL(pszAdjFname, "r");
        const char *pszLine = nullptr;
        while(fp && (pszLine = CPLReadLineL(fp)) != nullptr)
        {
            while (isspace(static_cast<unsigned char>(*pszLine)))  pszLine++;
            if (STARTS_WITH_CI(pszLine, "Fields"))
                bFoundFields = TRUE;
            else if (STARTS_WITH_CI(pszLine, "create view"))
                bFoundView = TRUE;
            else if (STARTS_WITH_CI(pszLine, "\"\\IsSeamless\" = \"TRUE\""))
                bFoundSeamless = TRUE;
        }

        if (bFoundView)
            poFile = new TABView;
        else if (bFoundFields && bFoundSeamless)
            poFile = new TABSeamless;
        else if (bFoundFields)
            poFile = new TABFile;

        if (fp)
            VSIFCloseL(fp);

        CPLFree(pszAdjFname);
    }

    /*-----------------------------------------------------------------
     * Perform the open() call
     *----------------------------------------------------------------*/
    if (poFile && poFile->Open(pszFname, bUpdate ? TABReadWrite : TABRead, bTestOpenNoError) != 0)
    {
        delete poFile;
        poFile = nullptr;
    }

    if (!bTestOpenNoError && poFile == nullptr)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "%s could not be opened as a MapInfo dataset.", pszFname);
    }

    return poFile;
}

/**********************************************************************
 *                   IMapInfoFile::GetNextFeature()
 *
 * Standard OGR GetNextFeature implementation.  This method is used
 * to retrieve the next OGRFeature.
 **********************************************************************/
OGRFeature *IMapInfoFile::GetNextFeature()
{
    GIntBig nFeatureId = 0;

    while( (nFeatureId = GetNextFeatureId(m_nCurFeatureId)) != -1 )
    {
        OGRGeometry *poGeom = nullptr;
        OGRFeature *poFeatureRef = GetFeatureRef(nFeatureId);
        if (poFeatureRef == nullptr)
            return nullptr;
        else if( (m_poFilterGeom == nullptr ||
                  ((poGeom = poFeatureRef->GetGeometryRef()) != nullptr &&
                   FilterGeometry( poGeom )))
                 && (m_poAttrQuery == nullptr
                     || m_poAttrQuery->Evaluate( poFeatureRef )) )
        {
            // Avoid cloning feature... return the copy owned by the class
            CPLAssert(poFeatureRef == m_poCurFeature);
            m_poCurFeature = nullptr;
            if( poFeatureRef->GetGeometryRef() != nullptr )
                poFeatureRef->GetGeometryRef()->assignSpatialReference(GetSpatialRef());
            return poFeatureRef;
        }
    }
    return nullptr;
}

/**********************************************************************
 *                   IMapInfoFile::CreateTABFeature()
 *
 * Instantiate a TABFeature* from a OGRFeature* (or NULL on error)
 **********************************************************************/

TABFeature* IMapInfoFile::CreateTABFeature(OGRFeature *poFeature)
{
    TABFeature *poTABFeature = nullptr;
    OGRwkbGeometryType eGType;
    TABPoint *poTABPointFeature = nullptr;
    TABRegion *poTABRegionFeature = nullptr;
    TABPolyline *poTABPolylineFeature = nullptr;

    /*-----------------------------------------------------------------
     * MITAB won't accept new features unless they are in a type derived
     * from TABFeature... so we have to do our best to map to the right
     * feature type based on the geometry type.
     *----------------------------------------------------------------*/
    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if( poGeom != nullptr )
        eGType = poGeom->getGeometryType();
    else
        eGType = wkbNone;

    switch( wkbFlatten(eGType) )
    {
      /*-------------------------------------------------------------
       * POINT
       *------------------------------------------------------------*/
      case wkbPoint:
        if(poFeature->GetStyleString())
        {
            TABFeatureClass featureClass = ITABFeatureSymbol::GetSymbolFeatureClass(poFeature->GetStyleString());
            if (featureClass == TABFCFontPoint) 
            {
                poTABFeature = new TABFontPoint(poFeature->GetDefnRef());
            }
            else if (featureClass == TABFCCustomPoint) 
            {
                poTABFeature = new TABCustomPoint(poFeature->GetDefnRef());
            }
            else 
            {
                poTABFeature = new TABPoint(poFeature->GetDefnRef());
            }
            poTABPointFeature = cpl::down_cast<TABPoint*>(poTABFeature);
            poTABPointFeature->SetSymbolFromStyleString(
                poFeature->GetStyleString());
        }
        else 
        {
            poTABFeature = new TABPoint(poFeature->GetDefnRef());
        }        
        break;
      /*-------------------------------------------------------------
       * REGION
       *------------------------------------------------------------*/
      case wkbPolygon:
      case wkbMultiPolygon:
        poTABFeature = new TABRegion(poFeature->GetDefnRef());
        if(poFeature->GetStyleString())
        {
            poTABRegionFeature = cpl::down_cast<TABRegion*>(poTABFeature);
            poTABRegionFeature->SetPenFromStyleString(
                poFeature->GetStyleString());

            poTABRegionFeature->SetBrushFromStyleString(
                poFeature->GetStyleString());
        }
        break;
      /*-------------------------------------------------------------
       * LINE/PLINE/MULTIPLINE
       *------------------------------------------------------------*/
      case wkbLineString:
      case wkbMultiLineString:
        poTABFeature = new TABPolyline(poFeature->GetDefnRef());
        if(poFeature->GetStyleString())
        {
            poTABPolylineFeature = cpl::down_cast<TABPolyline*>(poTABFeature);
            poTABPolylineFeature->SetPenFromStyleString(
                poFeature->GetStyleString());
        }
        break;
      /*-------------------------------------------------------------
       * Collection types that are not directly supported... convert
       * to multiple features in output file through recursive calls.
       *------------------------------------------------------------*/
      case wkbGeometryCollection:
      case wkbMultiPoint:
      {
          OGRErr eStatus = OGRERR_NONE;
          assert(poGeom); // for clang static analyzer
          OGRGeometryCollection *poColl = poGeom->toGeometryCollection();
          OGRFeature *poTmpFeature = poFeature->Clone();

          for( int i = 0;
               eStatus==OGRERR_NONE && poColl != nullptr &&
               i<poColl->getNumGeometries();
               i++)
          {
              poTmpFeature->SetFID(OGRNullFID);
              poTmpFeature->SetGeometry(poColl->getGeometryRef(i));
              eStatus = ICreateFeature(poTmpFeature);
          }
          delete poTmpFeature;
          return nullptr;
      }
        break;
      /*-------------------------------------------------------------
       * Unsupported type.... convert to MapInfo geometry NONE
       *------------------------------------------------------------*/
      case wkbUnknown:
      default:
         poTABFeature = new TABFeature(poFeature->GetDefnRef());
        break;
    }

    if( poGeom != nullptr )
        poTABFeature->SetGeometryDirectly(poGeom->clone());

    for (int i=0; i< poFeature->GetDefnRef()->GetFieldCount();i++)
    {
        poTABFeature->SetField(i,poFeature->GetRawFieldRef( i ));
    }

    poTABFeature->SetFID(poFeature->GetFID());

    return poTABFeature;
}

/**********************************************************************
 *                   IMapInfoFile::ICreateFeature()
 *
 * Standard OGR CreateFeature implementation.  This method is used
 * to create a new feature in current dataset
 **********************************************************************/
OGRErr     IMapInfoFile::ICreateFeature(OGRFeature *poFeature)
{
    TABFeature *poTABFeature = CreateTABFeature(poFeature);
    if( poTABFeature == nullptr ) /* MultiGeometry */
        return OGRERR_NONE;

    OGRErr eErr = CreateFeature(poTABFeature);
    if( eErr == OGRERR_NONE )
        poFeature->SetFID(poTABFeature->GetFID());

    delete poTABFeature;

    return eErr;
}

/**********************************************************************
 *                   IMapInfoFile::GetFeature()
 *
 * Standard OGR GetFeature implementation.  This method is used
 * to get the wanted (nFeatureId) feature, a NULL value will be
 * returned on error.
 **********************************************************************/
OGRFeature *IMapInfoFile::GetFeature(GIntBig nFeatureId)
{
    /*fprintf(stderr, "GetFeature(%ld)\n", nFeatureId);*/

    OGRFeature *poFeatureRef = GetFeatureRef(nFeatureId);
    if (poFeatureRef)
    {
        // Avoid cloning feature... return the copy owned by the class
        CPLAssert(poFeatureRef == m_poCurFeature);
        m_poCurFeature = nullptr;

        return poFeatureRef;
    }
    else
      return nullptr;
}

/************************************************************************/
/*                            GetTABType()                              */
/*                                                                      */
/*      Create a native field based on a generic OGR definition.        */
/************************************************************************/

int IMapInfoFile::GetTABType( OGRFieldDefn *poField,
                              TABFieldType* peTABType,
                              int *pnWidth,
                              int *pnPrecision)
{
    TABFieldType        eTABType;
    int                 nWidth = poField->GetWidth();
    int                 nPrecision = poField->GetPrecision();

    if( poField->GetType() == OFTInteger )
    {
        eTABType = TABFInteger;
        if( nWidth == 0 )
            nWidth = 12;
    }
    else if( poField->GetType() == OFTReal )
    {
        if( nWidth == 0 && poField->GetPrecision() == 0)
        {
            eTABType = TABFFloat;
            nWidth = 32;
        }
        else
        {
            eTABType = TABFDecimal;
            // Enforce Mapinfo limits, otherwise MapInfo will crash (#6392)
            if( nWidth > 20 || nWidth - nPrecision < 2 || nPrecision > 16 )
            {
                if( nWidth > 20 )
                    nWidth = 20;
                if( nWidth - nPrecision < 2 )
                    nPrecision = nWidth - 2;
                if( nPrecision > 16 )
                    nPrecision = 16;
                CPLDebug( "MITAB",
                          "Adjusting initial width,precision of %s from %d,%d to %d,%d",
                          poField->GetNameRef(),
                          poField->GetWidth(), poField->GetPrecision(),
                          nWidth, nPrecision );
            }
        }
    }
    else if( poField->GetType() == OFTDate )
    {
        eTABType = TABFDate;
        if( nWidth == 0 )
            nWidth = 10;
    }
    else if( poField->GetType() == OFTTime )
    {
        eTABType = TABFTime;
        if( nWidth == 0 )
            nWidth = 9;
    }
    else if( poField->GetType() == OFTDateTime )
    {
        eTABType = TABFDateTime;
        if( nWidth == 0 )
            nWidth = 19;
    }
    else if( poField->GetType() == OFTString )
    {
        eTABType = TABFChar;
        if( nWidth == 0 )
            nWidth = 254;
        else
            nWidth = std::min(254, nWidth);
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "IMapInfoFile::CreateField() called with unsupported field"
                  " type %d.\n"
                  "Note that Mapinfo files don't support list field types.\n",
                  poField->GetType() );

        return -1;
    }

    *peTABType = eTABType;
    *pnWidth = nWidth;
    *pnPrecision = nPrecision;

    return 0;
}

/************************************************************************/
/*                            CreateField()                             */
/*                                                                      */
/*      Create a native field based on a generic OGR definition.        */
/************************************************************************/

OGRErr IMapInfoFile::CreateField( OGRFieldDefn *poField, int bApproxOK )

{
    TABFieldType eTABType;
    int nWidth = 0;
    int nPrecision = 0;

    if( GetTABType( poField, &eTABType, &nWidth, &nPrecision ) < 0 )
        return OGRERR_FAILURE;

    if( AddFieldNative( poField->GetNameRef(), eTABType,
                        nWidth, nPrecision, FALSE, FALSE, bApproxOK ) > -1 )
        return OGRERR_NONE;

    return OGRERR_FAILURE;
}

/**********************************************************************
 *                   IMapInfoFile::SetCharset()
 *
 * Set the charset for the tab header.
 *
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int IMapInfoFile::SetCharset(const char* pszCharset)
{
    if(pszCharset && strlen(pszCharset) > 0)
    {
        if(pszCharset == m_pszCharset)
        {
            return 0;
        }
        CPLFree(m_pszCharset);
        m_pszCharset = CPLStrdup(pszCharset);
        return 0;
    }
    return -1;
}

const char* IMapInfoFile::GetCharset() const
{
    return m_pszCharset;
}

// Table is adopted from
// http://www.i-signum.com/Formation/download/MB_ReferenceGuide.pdf pp. 127-128
static const char* const apszCharsets[][2] = {
    { "Neutral", "" }, //No character conversions performed.
    { "ISO8859_1", "ISO-8859-1" }, //ISO 8859-1 (UNIX)
    { "ISO8859_2", "ISO-8859-2" }, //ISO 8859-2 (UNIX)
    { "ISO8859_3", "ISO-8859-3" }, //ISO 8859-3 (UNIX)
    { "ISO8859_4", "ISO-8859-4" }, //ISO 8859-4 (UNIX)
    { "ISO8859_5", "ISO-8859-5" }, //ISO 8859-5 (UNIX)
    { "ISO8859_6", "ISO-8859-6" }, //ISO 8859-6 (UNIX)
    { "ISO8859_7", "ISO-8859-7" }, //ISO 8859-7 (UNIX)
    { "ISO8859_8", "ISO-8859-8" }, //ISO 8859-8 (UNIX)
    { "ISO8859_9", "ISO-8859-9" }, //ISO 8859-9 (UNIX)
    { "PackedEUCJapaese", "EUC-JP" }, //UNIX, standard Japanese implementation.
    { "WindowsLatin1", "CP1252" },
    { "WindowsLatin2", "CP1250" },
    { "WindowsArabic", "CP1256" },
    { "WindowsCyrillic", "CP1251" },
    { "WindowsGreek", "CP1253" },
    { "WindowsHebrew", "CP1255" },
    { "WindowsTurkish", "CP1254" }, //Windows Eastern Europe
    { "WindowsTradChinese", "CP950" },//Windows Traditional Chinese
    { "WindowsSimpChinese", "CP936" },//Windows Simplified Chinese
    { "WindowsJapanese", "CP932" },
    { "WindowsKorean", "CP949" },
    { "CodePage437", "CP437" }, //DOS Code Page 437 = IBM Extended ASCII
    { "CodePage850", "CP850" }, //DOS Code Page 850 = Multilingual
    { "CodePage852", "CP852" }, //DOS Code Page 852 = Eastern Europe
    { "CodePage855", "CP855" }, //DOS Code Page 855 = Cyrillic
    { "CodePage857", "CP857" },
    { "CodePage860", "CP860" }, //DOS Code Page 860 = Portuguese
    { "CodePage861", "CP861" }, //DOS Code Page 861 = Icelandic
    { "CodePage863", "CP863" }, //DOS Code Page 863 = French Canadian
    { "CodePage864", "CP864" }, //DOS Code Page 864 = Arabic
    { "CodePage865", "CP865" }, //DOS Code Page 865 = Nordic
    { "CodePage869", "CP869" }, //DOS Code Page 869 = Modern Greek
    { "LICS", "" }, //Lotus worksheet release 1,2 character set
    { "LMBCS", "" },//Lotus worksheet release 3,4 character set
    { nullptr, nullptr }
};

const char* IMapInfoFile::CharsetToEncoding( const char* pszCharset )
{
    if( pszCharset == nullptr )
    {
        return apszCharsets[0][1];
    }

    for( size_t i = 0; apszCharsets[i][0] != nullptr; ++i)
    {
        if( EQUAL( pszCharset, apszCharsets[i][0] ) )
        {
            return apszCharsets[i][1];
        }
    }

    CPLError(CE_Warning, CPLE_NotSupported,
             "Cannot find iconv encoding corresponding to MapInfo %s charset",
             pszCharset);
    return apszCharsets[0][1];
}

const char* IMapInfoFile::EncodingToCharset( const char* pszEncoding )
{
    if( pszEncoding == nullptr )
    {
        return apszCharsets[0][0];
    }

    for( size_t i = 0; apszCharsets[i][1] != nullptr; ++i)
    {
        if( EQUAL( pszEncoding, apszCharsets[i][1] ) )
        {
            return apszCharsets[i][0];
        }
    }

    CPLError(CE_Warning, CPLE_NotSupported,
             "Cannot find MapInfo charset corresponding to iconv %s encoding",
             pszEncoding);
    return apszCharsets[0][0];
}

const char* IMapInfoFile::GetEncoding() const
{
    return CharsetToEncoding( GetCharset() );
}

void IMapInfoFile::SetEncoding( const char* pszEncoding )
{
    SetCharset( EncodingToCharset( pszEncoding ) );
}

int IMapInfoFile::TestUtf8Capability() const
{
    const char* pszEncoding( GetEncoding() );
    if( strlen( pszEncoding ) == 0 )
    {
        return FALSE;
    }

    return CPLCanRecode("test", GetEncoding(), CPL_ENC_UTF8);
}

CPLString IMapInfoFile::NormalizeFieldName(const char *pszName) const
{
    CPLString   osName(pszName);
    if( strlen( GetEncoding() ) > 0 )
        osName.Recode( CPL_ENC_UTF8, GetEncoding() );

    char szNewFieldName[31+1]; /* 31 is the max characters for a field name*/
    unsigned int nRenameNum = 1;

    strncpy(szNewFieldName, osName.c_str(), sizeof(szNewFieldName)-1);
    szNewFieldName[sizeof(szNewFieldName)-1] = '\0';

    while (m_oSetFields.find(CPLString(szNewFieldName).toupper()) != m_oSetFields.end() &&
           nRenameNum < 10)
    {
        CPLsnprintf( szNewFieldName, sizeof(szNewFieldName), "%.29s_%.1u",
                     osName.c_str(), nRenameNum );
        nRenameNum ++;
    }

    while (m_oSetFields.find(CPLString(szNewFieldName).toupper()) != m_oSetFields.end() &&
           nRenameNum < 100)
    {
        CPLsnprintf( szNewFieldName, sizeof(szNewFieldName),
                     "%.29s%.2u", osName.c_str(), nRenameNum );
        nRenameNum ++;
    }

    if (m_oSetFields.find(CPLString(szNewFieldName).toupper()) != m_oSetFields.end())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Too many field names like '%s' when truncated to 31 letters "
                  "for MapInfo format.", pszName );
    }

    CPLString   osNewFieldName(szNewFieldName);
    if( strlen( GetEncoding() ) > 0 )
        osNewFieldName.Recode( GetEncoding(), CPL_ENC_UTF8 );

    if( !EQUAL( pszName, osNewFieldName.c_str() ) )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Normalized/laundered field name: '%s' to '%s'",
                  pszName, osNewFieldName.c_str() );
    }

    return osNewFieldName;
}

