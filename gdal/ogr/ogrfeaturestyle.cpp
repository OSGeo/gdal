/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Feature Representation string API
 * Author:   Stephane Villeneuve, stephane.v@videotron.ca
 *
 ******************************************************************************
 * Copyright (c) 2000-2001, Stephane Villeneuve
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_featurestyle.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_feature.h"

CPL_CVSID("$Id$")

/****************************************************************************/
/*                Class Parameter (used in the String)                      */
/*                                                                          */
/*      The order of all parameter MUST be the same than in the definition. */
/****************************************************************************/
static const OGRStyleParamId asStylePen[] =
{
    {OGRSTPenColor, "c", FALSE, OGRSTypeString},
    {OGRSTPenWidth, "w", TRUE, OGRSTypeDouble},
    // Georefed, but multiple times.
    {OGRSTPenPattern, "p", FALSE, OGRSTypeString},
    {OGRSTPenId, "id", FALSE, OGRSTypeString},
    {OGRSTPenPerOffset, "dp", TRUE, OGRSTypeDouble},
    {OGRSTPenCap, "cap", FALSE, OGRSTypeString},
    {OGRSTPenJoin, "j", FALSE, OGRSTypeString},
    {OGRSTPenPriority,  "l",  FALSE,  OGRSTypeInteger}
};

static const OGRStyleParamId asStyleBrush[] =
{
    {OGRSTBrushFColor, "fc", FALSE, OGRSTypeString},
    {OGRSTBrushBColor, "bc", FALSE, OGRSTypeString},
    {OGRSTBrushId, "id", FALSE, OGRSTypeString},
    {OGRSTBrushAngle, "a", FALSE, OGRSTypeDouble},
    {OGRSTBrushSize, "s", TRUE, OGRSTypeDouble},
    {OGRSTBrushDx, "dx", TRUE, OGRSTypeDouble},
    {OGRSTBrushDy, "dy", TRUE, OGRSTypeDouble},
    {OGRSTBrushPriority, "l", FALSE, OGRSTypeInteger}
};

static const OGRStyleParamId asStyleSymbol[] =
{
    {OGRSTSymbolId, "id", FALSE, OGRSTypeString},
    {OGRSTSymbolAngle, "a", FALSE, OGRSTypeDouble},
    {OGRSTSymbolColor, "c", FALSE, OGRSTypeString},
    {OGRSTSymbolSize, "s", TRUE, OGRSTypeDouble},
    {OGRSTSymbolDx, "dx", TRUE, OGRSTypeDouble},
    {OGRSTSymbolDy, "dy", TRUE, OGRSTypeDouble},
    {OGRSTSymbolStep, "ds", TRUE, OGRSTypeDouble},
    {OGRSTSymbolPerp, "dp", TRUE, OGRSTypeDouble},
    {OGRSTSymbolOffset, "di", TRUE, OGRSTypeDouble},
    {OGRSTSymbolPriority, "l", FALSE, OGRSTypeInteger},
    {OGRSTSymbolFontName, "f", FALSE, OGRSTypeString},
    {OGRSTSymbolOColor, "o", FALSE, OGRSTypeString}
};

static const OGRStyleParamId asStyleLabel[] =
{
    {OGRSTLabelFontName, "f", FALSE, OGRSTypeString},
    {OGRSTLabelSize, "s", TRUE, OGRSTypeDouble},
    {OGRSTLabelTextString, "t", FALSE, OGRSTypeString},
    {OGRSTLabelAngle, "a", FALSE, OGRSTypeDouble},
    {OGRSTLabelFColor, "c", FALSE, OGRSTypeString},
    {OGRSTLabelBColor, "b", FALSE, OGRSTypeString},
    {OGRSTLabelPlacement, "m", FALSE, OGRSTypeString},
    {OGRSTLabelAnchor, "p", FALSE, OGRSTypeInteger},
    {OGRSTLabelDx, "dx", TRUE, OGRSTypeDouble},
    {OGRSTLabelDy, "dy", TRUE, OGRSTypeDouble},
    {OGRSTLabelPerp, "dp", TRUE, OGRSTypeDouble},
    {OGRSTLabelBold, "bo", FALSE, OGRSTypeBoolean},
    {OGRSTLabelItalic, "it", FALSE, OGRSTypeBoolean},
    {OGRSTLabelUnderline, "un", FALSE, OGRSTypeBoolean},
    {OGRSTLabelPriority, "l", FALSE, OGRSTypeInteger},
    {OGRSTLabelStrikeout, "st", FALSE, OGRSTypeBoolean},
    {OGRSTLabelStretch, "w", FALSE, OGRSTypeDouble},
    {-1, nullptr, FALSE, OGRSTypeUnused}, // was OGRSTLabelAdjHor
    {-1, nullptr, FALSE, OGRSTypeUnused}, // was OGRSTLabelAdjVert
    {OGRSTLabelHColor, "h", FALSE, OGRSTypeString},
    {OGRSTLabelOColor, "o", FALSE, OGRSTypeString}
};

/* ======================================================================== */
/* OGRStyleMgr                                                              */
/* ======================================================================== */

/****************************************************************************/
/*             OGRStyleMgr::OGRStyleMgr(OGRStyleTable *poDataSetStyleTable) */
/*                                                                          */
/****************************************************************************/
/**
 * \brief Constructor.
 *
 * This method is the same as the C function OGR_SM_Create()
 *
 * @param poDataSetStyleTable (currently unused, reserved for future use),
 * pointer to OGRStyleTable. Pass NULL for now.
 */
OGRStyleMgr::OGRStyleMgr( OGRStyleTable *poDataSetStyleTable ):
    m_poDataSetStyleTable( poDataSetStyleTable )
{
}

/************************************************************************/
/*                            OGR_SM_Create()                           */
/************************************************************************/
/**
 * \brief OGRStyleMgr factory.
 *
 * This function is the same as the C++ method OGRStyleMgr::OGRStyleMgr().
 *
 * @param hStyleTable pointer to OGRStyleTable or NULL if not working with
 *  a style table.
 *
 * @return an handle to the new style manager object.
 */

OGRStyleMgrH OGR_SM_Create( OGRStyleTableH hStyleTable )

{
    return reinterpret_cast<OGRStyleMgrH>(
        new OGRStyleMgr(reinterpret_cast<OGRStyleTable *>(hStyleTable)));
}

/****************************************************************************/
/*             OGRStyleMgr::~OGRStyleMgr()                                  */
/*                                                                          */
/****************************************************************************/
/**
 * \brief Destructor.
 *
 * This method is the same as the C function OGR_SM_Destroy()
 */
OGRStyleMgr::~OGRStyleMgr()
{
    CPLFree(m_pszStyleString);
}

/************************************************************************/
/*                           OGR_SM_Destroy()                            */
/************************************************************************/
/**
 * \brief Destroy Style Manager
 *
 * This function is the same as the C++ method OGRStyleMgr::~OGRStyleMgr().
 *
 * @param hSM handle to the style manager to destroy.
 */

void OGR_SM_Destroy( OGRStyleMgrH hSM )

{
    delete reinterpret_cast<OGRStyleMgr *>(hSM);
}

/****************************************************************************/
/*      GBool OGRStyleMgr::SetFeatureStyleString(OGRFeature *poFeature,     */
/*                                       char *pszStyleString,              */
/*                                       GBool bNoMatching)                 */
/*      Set the given representation to the feature,                        */
/*      if bNoMatching == TRUE, don't try to find it in the styletable      */
/*      otherwise, we will use the name defined in the styletable.          */
/****************************************************************************/

/**
 * \brief Set a style in a feature
 *
 * @param poFeature       the feature object to store the style in
 * @param pszStyleString  the style to store
 * @param bNoMatching     TRUE to lookup the style in the style table and
 *  add the name to the feature
 *
 * @return TRUE on success, FALSE on error.
 */

GBool OGRStyleMgr::SetFeatureStyleString( OGRFeature *poFeature,
                                          const char *pszStyleString,
                                          GBool bNoMatching )
{
    if( poFeature == nullptr )
        return FALSE;

    const char *pszName = nullptr;

    if( pszStyleString == nullptr )
        poFeature->SetStyleString("");
    else if( bNoMatching == TRUE )
        poFeature->SetStyleString(pszStyleString);
    else if( (pszName = GetStyleName(pszStyleString)) != nullptr )
        poFeature->SetStyleString(pszName);
    else
        poFeature->SetStyleString(pszStyleString);

    return TRUE;
}

/****************************************************************************/
/*            const char *OGRStyleMgr::InitFromFeature(OGRFeature *)        */
/*                                                                          */
/****************************************************************************/

/**
 * \brief Initialize style manager from the style string of a feature.
 *
 * This method is the same as the C function OGR_SM_InitFromFeature().
 *
 * @param poFeature feature object from which to read the style.
 *
 * @return a reference to the style string read from the feature, or NULL
 * in case of error..
 */

const char *OGRStyleMgr::InitFromFeature( OGRFeature *poFeature )
{
    CPLFree(m_pszStyleString);
    m_pszStyleString = nullptr;

    if( poFeature )
        InitStyleString(poFeature->GetStyleString());
    else
        m_pszStyleString = nullptr;

    return m_pszStyleString;
}

/************************************************************************/
/*                     OGR_SM_InitFromFeature()                         */
/************************************************************************/

/**
 * \brief Initialize style manager from the style string of a feature.
 *
 * This function is the same as the C++ method
 * OGRStyleMgr::InitFromFeature().
 *
 * @param hSM handle to the style manager.
 * @param hFeat handle to the new feature from which to read the style.
 *
 * @return a reference to the style string read from the feature, or NULL
 * in case of error.
 */

const char *OGR_SM_InitFromFeature( OGRStyleMgrH hSM, OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hSM, "OGR_SM_InitFromFeature", nullptr );
    VALIDATE_POINTER1( hFeat, "OGR_SM_InitFromFeature", nullptr );

    return reinterpret_cast<OGRStyleMgr *>(hSM)->
        InitFromFeature(reinterpret_cast<OGRFeature *>(hFeat));
}

/****************************************************************************/
/*            GBool OGRStyleMgr::InitStyleString(char *pszStyleString)      */
/*                                                                          */
/****************************************************************************/

/**
 * \brief Initialize style manager from the style string.
 *
 * This method is the same as the C function OGR_SM_InitStyleString().
 *
 * @param pszStyleString the style string to use (can be NULL).
 *
 * @return TRUE on success, FALSE on errors.
 */
GBool OGRStyleMgr::InitStyleString( const char *pszStyleString )
{
    CPLFree(m_pszStyleString);
    m_pszStyleString = nullptr;

    if( pszStyleString && pszStyleString[0] == '@' )
        m_pszStyleString = CPLStrdup(GetStyleByName(pszStyleString));
    else
        m_pszStyleString = nullptr;

    if( m_pszStyleString == nullptr && pszStyleString )
        m_pszStyleString = CPLStrdup(pszStyleString);

    return TRUE;
}

/************************************************************************/
/*                     OGR_SM_InitStyleString()                         */
/************************************************************************/

/**
 * \brief Initialize style manager from the style string.
 *
 * This function is the same as the C++ method OGRStyleMgr::InitStyleString().
 *
 * @param hSM handle to the style manager.
 * @param pszStyleString the style string to use (can be NULL).
 *
 * @return TRUE on success, FALSE on errors.
 */

int OGR_SM_InitStyleString( OGRStyleMgrH hSM, const char *pszStyleString )

{
    VALIDATE_POINTER1( hSM, "OGR_SM_InitStyleString", FALSE );

    return reinterpret_cast<OGRStyleMgr *>(hSM)->
        InitStyleString(pszStyleString);
}

/****************************************************************************/
/*      const char *OGRStyleMgr::GetStyleName(const char *pszStyleString)   */
/****************************************************************************/

/**
 * \brief Get the name of a style from the style table.
 *
 * @param pszStyleString the style to search for, or NULL to use the style
 *   currently stored in the manager.
 *
 * @return The name if found, or NULL on error.
 */

const char *OGRStyleMgr::GetStyleName( const char *pszStyleString )
{
    // SECURITY: The unit and the value for all parameter should be the same,
    // a text comparison is executed.

    const char *pszStyle = pszStyleString ? pszStyleString : m_pszStyleString;

    if( pszStyle )
    {
        if( m_poDataSetStyleTable )
            return m_poDataSetStyleTable->GetStyleName(pszStyle);
    }
    return nullptr;
}
/****************************************************************************/
/*      const char *OGRStyleMgr::GetStyleByName(const char *pszStyleName)   */
/*                                                                          */
/****************************************************************************/

/**
 * \brief find a style in the current style table.
 *
 *
 * @param pszStyleName the name of the style to add.
 *
 * @return the style string matching the name or NULL if not found or error.
 */
const char *OGRStyleMgr::GetStyleByName( const char *pszStyleName )
{
    if( m_poDataSetStyleTable )
    {
        return m_poDataSetStyleTable->Find(pszStyleName);
    }
    return nullptr;
}

/****************************************************************************/
/*            GBool OGRStyleMgr::AddStyle(char *pszStyleName,               */
/*                                   char *pszStyleString)                  */
/*                                                                          */
/****************************************************************************/

/**
 * \brief Add a style to the current style table.
 *
 * This method is the same as the C function OGR_SM_AddStyle().
 *
 * @param pszStyleName the name of the style to add.
 * @param pszStyleString the style string to use, or NULL to use the style
 *                       stored in the manager.
 *
 * @return TRUE on success, FALSE on errors.
 */

GBool OGRStyleMgr::AddStyle( const char *pszStyleName,
                             const char *pszStyleString )
{
    const char *pszStyle = pszStyleString ? pszStyleString : m_pszStyleString;

    if( m_poDataSetStyleTable )
    {
        return m_poDataSetStyleTable->AddStyle(pszStyleName, pszStyle);
    }
    return FALSE;
}

/************************************************************************/
/*                     OGR_SM_AddStyle()                         */
/************************************************************************/

/**
 * \brief Add a style to the current style table.
 *
 * This function is the same as the C++ method OGRStyleMgr::AddStyle().
 *
 * @param hSM handle to the style manager.
 * @param pszStyleName the name of the style to add.
 * @param pszStyleString the style string to use, or NULL to use the style
 *                       stored in the manager.
 *
 * @return TRUE on success, FALSE on errors.
 */

int OGR_SM_AddStyle( OGRStyleMgrH hSM, const char *pszStyleName,
                     const char *pszStyleString )
{
    VALIDATE_POINTER1( hSM, "OGR_SM_AddStyle", FALSE );
    VALIDATE_POINTER1( pszStyleName, "OGR_SM_AddStyle", FALSE );

    return reinterpret_cast<OGRStyleMgr *>(hSM)->
        AddStyle( pszStyleName, pszStyleString);
}

/****************************************************************************/
/*            const char *OGRStyleMgr::GetStyleString(OGRFeature *)         */
/*                                                                          */
/****************************************************************************/

/**
 * \brief Get the style string from the style manager.
 *
 * @param poFeature feature object from which to read the style or NULL to
 *                  get the style string stored in the manager.
 *
 * @return the style string stored in the feature or the style string stored
 *          in the style manager if poFeature is NULL
 *
 * NOTE: this method will call OGRStyleMgr::InitFromFeature() if poFeature is
 *       not NULL and replace the style string stored in the style manager
 */

const char *OGRStyleMgr::GetStyleString( OGRFeature *poFeature )
{
    if( poFeature == nullptr )
        return m_pszStyleString;

    return InitFromFeature(poFeature);
}

/****************************************************************************/
/*            GBool OGRStyleMgr::AddPart(const char *pszPart)               */
/*            Add a new part in the current style                           */
/****************************************************************************/

/**
 * \brief Add a part (style string) to the current style.
 *
 * @param pszPart the style string defining the part to add.
 *
 * @return TRUE on success, FALSE on errors.
 */

GBool OGRStyleMgr::AddPart( const char *pszPart )
{
    if( pszPart == nullptr )
        return FALSE;

    if( m_pszStyleString )
    {
        char *pszTmp =
            CPLStrdup(CPLString().Printf("%s;%s", m_pszStyleString, pszPart));
        CPLFree(m_pszStyleString);
        m_pszStyleString = pszTmp;
    }
    else
    {
        char *pszTmp = CPLStrdup(CPLString().Printf("%s", pszPart));
        CPLFree(m_pszStyleString);
        m_pszStyleString = pszTmp;
    }
    return TRUE;
}

/****************************************************************************/
/*            GBool OGRStyleMgr::AddPart(OGRStyleTool *)                    */
/*            Add a new part in the current style                           */
/****************************************************************************/

/**
 * \brief Add a part (style tool) to the current style.
 *
 * This method is the same as the C function OGR_SM_AddPart().
 *
 * @param poStyleTool the style tool defining the part to add.
 *
 * @return TRUE on success, FALSE on errors.
 */

GBool OGRStyleMgr::AddPart( OGRStyleTool *poStyleTool )
{
    if( poStyleTool == nullptr || !poStyleTool->GetStyleString() )
        return FALSE;

    if( m_pszStyleString )
    {
        char *pszTmp =
            CPLStrdup(CPLString().Printf("%s;%s", m_pszStyleString,
                                         poStyleTool->GetStyleString()));
        CPLFree(m_pszStyleString);
        m_pszStyleString = pszTmp;
    }
    else
    {
        char *pszTmp =
            CPLStrdup(CPLString().Printf("%s",
                                         poStyleTool->GetStyleString()));
          CPLFree(m_pszStyleString);
          m_pszStyleString = pszTmp;
    }
    return TRUE;
}

/************************************************************************/
/*                     OGR_SM_AddPart()                                 */
/************************************************************************/

/**
 * \brief Add a part (style tool) to the current style.
 *
 * This function is the same as the C++ method OGRStyleMgr::AddPart().
 *
 * @param hSM handle to the style manager.
 * @param hST the style tool defining the part to add.
 *
 * @return TRUE on success, FALSE on errors.
 */

int OGR_SM_AddPart( OGRStyleMgrH hSM, OGRStyleToolH hST )

{
    VALIDATE_POINTER1( hSM, "OGR_SM_InitStyleString", FALSE );
    VALIDATE_POINTER1( hST, "OGR_SM_InitStyleString", FALSE );

    return reinterpret_cast<OGRStyleMgr *>(hSM)->
        AddPart(reinterpret_cast<OGRStyleTool *>(hST));
}

/****************************************************************************/
/*            int OGRStyleMgr::GetPartCount(const char *pszStyleString)     */
/*            return the number of part in the stylestring                  */
/* FIXME: this function should actually parse style string instead of simple*/
/*        semicolon counting, we should not count broken and empty parts.   */
/****************************************************************************/

/**
 * \brief Get the number of parts in a style.
 *
 * This method is the same as the C function OGR_SM_GetPartCount().
 *
 * @param pszStyleString (optional) the style string on which to operate.
 * If NULL then the current style string stored in the style manager is used.
 *
 * @return the number of parts (style tools) in the style.
 */

int OGRStyleMgr::GetPartCount( const char *pszStyleString )
{
    const char *pszString = pszStyleString != nullptr
        ? pszStyleString
        : m_pszStyleString;

    if( pszString == nullptr )
      return 0;

    int nPartCount = 1;
    const char *pszStrTmp = pszString;
    // Search for parts separated by semicolons not counting the possible
    // semicolon at the and of string.
    const char *pszPart = nullptr;
    while( (pszPart = strstr(pszStrTmp, ";")) != nullptr && pszPart[1] != '\0' )
    {
        pszStrTmp = &pszPart[1];
        nPartCount++;
    }
    return nPartCount;
}

/************************************************************************/
/*                     OGR_SM_GetPartCount()                            */
/************************************************************************/

/**
 * \brief Get the number of parts in a style.
 *
 * This function is the same as the C++ method OGRStyleMgr::GetPartCount().
 *
 * @param hSM handle to the style manager.
 * @param pszStyleString (optional) the style string on which to operate.
 * If NULL then the current style string stored in the style manager is used.
 *
 * @return the number of parts (style tools) in the style.
 */

int OGR_SM_GetPartCount( OGRStyleMgrH hSM, const char *pszStyleString )

{
    VALIDATE_POINTER1( hSM, "OGR_SM_InitStyleString", FALSE );

    return reinterpret_cast<OGRStyleMgr *>(hSM)->GetPartCount(pszStyleString);
}

/****************************************************************************/
/*            OGRStyleTool *OGRStyleMgr::GetPart(int nPartId,               */
/*                                 const char *pszStyleString)              */
/*                                                                          */
/*     Return a StyleTool of the type of the wanted part, could return NULL */
/****************************************************************************/

/**
 * \brief Fetch a part (style tool) from the current style.
 *
 * This method is the same as the C function OGR_SM_GetPart().
 *
 * This method instantiates a new object that should be freed with
 * OGR_ST_Destroy().
 *
 * @param nPartId the part number (0-based index).
 * @param pszStyleString (optional) the style string on which to operate.
 * If NULL then the current style string stored in the style manager is used.
 *
 * @return OGRStyleTool of the requested part (style tools) or NULL on error.
 */

OGRStyleTool *OGRStyleMgr::GetPart( int nPartId,
                                    const char *pszStyleString )
{
    const char *pszStyle = pszStyleString ? pszStyleString : m_pszStyleString;

    if( pszStyle == nullptr )
      return nullptr;

    char **papszStyleString =
        CSLTokenizeString2(pszStyle, ";",
                           CSLT_HONOURSTRINGS
                           | CSLT_PRESERVEQUOTES
                           | CSLT_PRESERVEESCAPES );

    const char *pszString = CSLGetField( papszStyleString, nPartId );

    OGRStyleTool *poStyleTool = nullptr;
    if( strlen(pszString) > 0 )
    {
        poStyleTool = CreateStyleToolFromStyleString(pszString);
        if( poStyleTool )
            poStyleTool->SetStyleString(pszString);
    }

    CSLDestroy( papszStyleString );

    return poStyleTool;
}

/************************************************************************/
/*                     OGR_SM_GetPart()                                 */
/************************************************************************/

/**
 * \brief Fetch a part (style tool) from the current style.
 *
 * This function is the same as the C++ method OGRStyleMgr::GetPart().
 *
 * This function instantiates a new object that should be freed with
 * OGR_ST_Destroy().
 *
 * @param hSM handle to the style manager.
 * @param nPartId the part number (0-based index).
 * @param pszStyleString (optional) the style string on which to operate.
 * If NULL then the current style string stored in the style manager is used.
 *
 * @return OGRStyleToolH of the requested part (style tools) or NULL on error.
 */

OGRStyleToolH OGR_SM_GetPart( OGRStyleMgrH hSM, int nPartId,
                              const char *pszStyleString )

{
    VALIDATE_POINTER1( hSM, "OGR_SM_InitStyleString", nullptr );

    return
        reinterpret_cast<OGRStyleToolH>(
            reinterpret_cast<OGRStyleMgr *>(hSM)->
                GetPart(nPartId, pszStyleString));
}

/****************************************************************************/
/* OGRStyleTool *CreateStyleToolFromStyleString(const char *pszStyleString) */
/*                                                                          */
/* create a Style tool from the given StyleString, it should contain only a */
/* part of a StyleString.                                                    */
/****************************************************************************/

//! @cond Doxygen_Suppress
OGRStyleTool *
OGRStyleMgr::CreateStyleToolFromStyleString( const char *pszStyleString )
{
    char **papszToken = CSLTokenizeString2(pszStyleString, "();",
                                           CSLT_HONOURSTRINGS
                                           | CSLT_PRESERVEQUOTES
                                           | CSLT_PRESERVEESCAPES );
    OGRStyleTool *poStyleTool = nullptr;

    if( CSLCount(papszToken) < 2 )
        poStyleTool = nullptr;
    else if( EQUAL(papszToken[0], "PEN") )
        poStyleTool = new OGRStylePen();
    else if( EQUAL(papszToken[0], "BRUSH") )
        poStyleTool = new OGRStyleBrush();
    else if( EQUAL(papszToken[0], "SYMBOL") )
        poStyleTool = new OGRStyleSymbol();
    else if( EQUAL(papszToken[0], "LABEL") )
        poStyleTool = new OGRStyleLabel();
    else
        poStyleTool = nullptr;

    CSLDestroy( papszToken );

    return poStyleTool;
}
//! @endcond

/* ======================================================================== */
/*                OGRStyleTable                                             */
/*     Object Used to manage and store a styletable                         */
/* ======================================================================== */

/****************************************************************************/
/*              OGRStyleTable::OGRStyleTable()                              */
/*                                                                          */
/****************************************************************************/
OGRStyleTable::OGRStyleTable()
{
    m_papszStyleTable = nullptr;
    iNextStyle = 0;
}

/************************************************************************/
/*                            OGR_STBL_Create()                           */
/************************************************************************/
/**
 * \brief OGRStyleTable factory.
 *
 * This function is the same as the C++ method OGRStyleTable::OGRStyleTable().
 *
 *
 * @return an handle to the new style table object.
 */

OGRStyleTableH OGR_STBL_Create( void )

{
    return reinterpret_cast<OGRStyleTableH>(new OGRStyleTable());
}

/****************************************************************************/
/*                void OGRStyleTable::Clear()                               */
/*                                                                          */
/****************************************************************************/

/**
 * \brief Clear a style table.
 *
 */

void OGRStyleTable::Clear()
{
    if( m_papszStyleTable )
      CSLDestroy(m_papszStyleTable);
    m_papszStyleTable = nullptr;
}

/****************************************************************************/
/*          OGRStyleTable::~OGRStyleTable()                                 */
/*                                                                          */
/****************************************************************************/
OGRStyleTable::~OGRStyleTable()
{
    Clear();
}

/************************************************************************/
/*                           OGR_STBL_Destroy()                            */
/************************************************************************/
/**
 * \brief Destroy Style Table
 *
 * @param hSTBL handle to the style table to destroy.
 */

void OGR_STBL_Destroy( OGRStyleTableH hSTBL )

{
    delete reinterpret_cast<OGRStyleTable *>(hSTBL);
}

/****************************************************************************/
/*    const char *OGRStyleTable::GetStyleName(const char *pszStyleString)   */
/*                                                                          */
/*    return the Name of a given stylestring otherwise NULL.                */
/****************************************************************************/

/**
 * \brief Get style name by style string.
 *
 * @param pszStyleString the style string to look up.
 *
 * @return the Name of the matching style string or NULL on error.
 */

const char *OGRStyleTable::GetStyleName( const char *pszStyleString )
{
    for( int i = 0; i < CSLCount(m_papszStyleTable); i++ )
    {
        const char *pszStyleStringBegin =
            strstr(m_papszStyleTable[i], ":");

        if( pszStyleStringBegin && EQUAL(&pszStyleStringBegin[1],
                                         pszStyleString) )
        {
            osLastRequestedStyleName = m_papszStyleTable[i];
            const size_t nColon = osLastRequestedStyleName.find( ':' );
            if( nColon != std::string::npos )
                osLastRequestedStyleName =
                    osLastRequestedStyleName.substr(0, nColon);

            return osLastRequestedStyleName;
        }
    }

    return nullptr;
}

/****************************************************************************/
/*            GBool OGRStyleTable::AddStyle(char *pszName,                  */
/*                                          char *pszStyleString)           */
/*                                                                          */
/*   Add a new style in the table, no comparison will be done on the        */
/*   Style string, only on the name, TRUE success, FALSE error              */
/****************************************************************************/

/**
 * \brief Add a new style in the table.
 * No comparison will be done on the
 * Style string, only on the name.
 *
 * @param pszName the name the style to add.
 * @param pszStyleString the style string to add.
 *
 * @return TRUE on success, FALSE on error
 */

GBool OGRStyleTable::AddStyle( const char *pszName, const char *pszStyleString )
{
    if( pszName == nullptr || pszStyleString == nullptr )
        return FALSE;

    const int nPos = IsExist(pszName);
    if( nPos != -1 )
      return FALSE;

    m_papszStyleTable =
        CSLAddString(m_papszStyleTable,
                     CPLString().Printf("%s:%s", pszName, pszStyleString));
    return TRUE;
}

/************************************************************************/
/*                       OGR_STBL_AddStyle()                            */
/************************************************************************/

/**
 * \brief Add a new style in the table.
 * No comparison will be done on the
 * Style string, only on the name.
 * This function is the same as the C++ method OGRStyleTable::AddStyle().
 *
 * @param hStyleTable handle to the style table.
 * @param pszName the name the style to add.
 * @param pszStyleString the style string to add.
 *
 * @return TRUE on success, FALSE on error
 */

int OGR_STBL_AddStyle( OGRStyleTableH hStyleTable,
                       const char *pszName, const char *pszStyleString )
{
    VALIDATE_POINTER1( hStyleTable, "OGR_STBL_AddStyle", FALSE );

    return reinterpret_cast<OGRStyleTable *>(hStyleTable)->
        AddStyle(pszName, pszStyleString);
}

/****************************************************************************/
/*            GBool OGRStyleTable::RemoveStyle(char *pszName)               */
/*                                                                          */
/*    Remove the given style in the table based on the name, return TRUE    */
/*    on success otherwise FALSE.                                           */
/****************************************************************************/

/**
 * \brief Remove a style in the table by its name.
 *
 * @param pszName the name of the style to remove.
 *
 * @return TRUE on success, FALSE on error
 */

GBool OGRStyleTable::RemoveStyle( const char *pszName )
{
    const int nPos = IsExist(pszName);
    if( nPos == -1 )
        return FALSE;

    m_papszStyleTable = CSLRemoveStrings(m_papszStyleTable, nPos, 1, nullptr);
    return TRUE;
}

/****************************************************************************/
/*            GBool OGRStyleTable::ModifyStyle(char *pszName,               */
/*                                             char *pszStyleString)        */
/*                                                                          */
/*    Modify the given style, if the style doesn't exist, it will be added  */
/*    return TRUE on success otherwise return FALSE.                        */
/****************************************************************************/

/**
 * \brief Modify a style in the table by its name
 * If the style does not exist, it will be added.
 *
 * @param pszName the name of the style to modify.
 * @param pszStyleString the style string.
 *
 * @return TRUE on success, FALSE on error
 */

GBool OGRStyleTable::ModifyStyle( const char *pszName,
                                  const char * pszStyleString )
{
    if( pszName == nullptr || pszStyleString == nullptr )
      return FALSE;

    RemoveStyle(pszName);
    return AddStyle(pszName, pszStyleString);
}

/****************************************************************************/
/*            GBool OGRStyleTable::SaveStyleTable(char *)                   */
/*                                                                          */
/*    Save the StyleTable in the given file, return TRUE on success         */
/*    otherwise return FALSE.                                               */
/****************************************************************************/

/**
 * \brief Save a style table to a file.
 *
 * @param pszFilename the name of the file to save to.
 *
 * @return TRUE on success, FALSE on error
 */

GBool OGRStyleTable::SaveStyleTable( const char *pszFilename )
{
    if( pszFilename == nullptr )
      return FALSE;

    if( CSLSave(m_papszStyleTable, pszFilename) == 0 )
      return FALSE;

    return TRUE;
}

/************************************************************************/
/*                     OGR_STBL_SaveStyleTable()                        */
/************************************************************************/

/**
 * \brief Save a style table to a file.
 *
 * This function is the same as the C++ method OGRStyleTable::SaveStyleTable().
 *
 * @param hStyleTable handle to the style table.
 * @param pszFilename the name of the file to save to.
 *
 * @return TRUE on success, FALSE on error
 */

int OGR_STBL_SaveStyleTable( OGRStyleTableH hStyleTable,
                             const char *pszFilename )
{
    VALIDATE_POINTER1( hStyleTable, "OGR_STBL_SaveStyleTable", FALSE );
    VALIDATE_POINTER1( pszFilename, "OGR_STBL_SaveStyleTable", FALSE );

    return reinterpret_cast<OGRStyleTable *>(hStyleTable)->
        SaveStyleTable( pszFilename );
}

/****************************************************************************/
/*            GBool OGRStyleTable::LoadStyleTable(char *)                   */
/*                                                                          */
/*            Read the Style table from a file, return TRUE on success      */
/*            otherwise return FALSE                                        */
/****************************************************************************/

/**
 * \brief Load a style table from a file.
 *
 * @param pszFilename the name of the file to load from.
 *
 * @return TRUE on success, FALSE on error
 */

GBool OGRStyleTable::LoadStyleTable( const char *pszFilename )
{
    if( pszFilename == nullptr )
      return FALSE;

    CSLDestroy(m_papszStyleTable);

    m_papszStyleTable = CSLLoad(pszFilename);

    return m_papszStyleTable != nullptr;
}

/************************************************************************/
/*                     OGR_STBL_LoadStyleTable()                        */
/************************************************************************/

/**
 * \brief Load a style table from a file.
 *
 * This function is the same as the C++ method OGRStyleTable::LoadStyleTable().
 *
 * @param hStyleTable handle to the style table.
 * @param pszFilename the name of the file to load from.
 *
 * @return TRUE on success, FALSE on error
 */

int OGR_STBL_LoadStyleTable( OGRStyleTableH hStyleTable,
                             const char *pszFilename )
{
    VALIDATE_POINTER1( hStyleTable, "OGR_STBL_LoadStyleTable", FALSE );
    VALIDATE_POINTER1( pszFilename, "OGR_STBL_LoadStyleTable", FALSE );

    return reinterpret_cast<OGRStyleTable *>(hStyleTable)->
        LoadStyleTable( pszFilename );
}

/****************************************************************************/
/*             const char *OGRStyleTable::Find(const char *pszName)         */
/*                                                                          */
/*             return the StyleString based on the given name,              */
/*             otherwise return NULL.                                       */
/****************************************************************************/

/**
 * \brief Get a style string by name.
 *
 * @param pszName the name of the style string to find.
 *
 * @return the style string matching the name, NULL if not found or error.
 */

const char *OGRStyleTable::Find(const char *pszName)
{
    const int nPos = IsExist(pszName);
    if( nPos == -1 )
        return nullptr;

    const char *pszOutput = CSLGetField(m_papszStyleTable, nPos);

    const char *pszDash = strstr(pszOutput, ":");

    if( pszDash == nullptr )
        return nullptr;

    return &pszDash[1];
}

/************************************************************************/
/*                     OGR_STBL_Find()                                  */
/************************************************************************/

/**
 * \brief Get a style string by name.
 *
 * This function is the same as the C++ method OGRStyleTable::Find().
 *
 * @param hStyleTable handle to the style table.
 * @param pszName the name of the style string to find.
 *
 * @return the style string matching the name or NULL if not found or error.
 */

const char *OGR_STBL_Find( OGRStyleTableH hStyleTable, const char *pszName )
{
    VALIDATE_POINTER1( hStyleTable, "OGR_STBL_Find", nullptr );
    VALIDATE_POINTER1( pszName, "OGR_STBL_Find", nullptr );

    return reinterpret_cast<OGRStyleTable *>(hStyleTable)->Find(pszName);
}

/****************************************************************************/
/*              OGRStyleTable::Print(FILE *fpOut)                           */
/*                                                                          */
/****************************************************************************/

/**
 * \brief Print a style table to a FILE pointer.
 *
 * @param fpOut the FILE pointer to print to.
 *
 */

void OGRStyleTable::Print( FILE *fpOut )
{

    CPL_IGNORE_RET_VAL(VSIFPrintf(fpOut, "#OFS-Version: 1.0\n"));
    CPL_IGNORE_RET_VAL(VSIFPrintf(fpOut, "#StyleField: style\n"));
    if( m_papszStyleTable )
    {
        CSLPrint(m_papszStyleTable, fpOut);
    }
}

/****************************************************************************/
/*             int OGRStyleTable::IsExist(const char *pszName)              */
/*                                                                          */
/*   return a index of the style in the table otherwise return -1           */
/****************************************************************************/

/**
 * \brief Get the index of a style in the table by its name.
 *
 * @param pszName the name to look for.
 *
 * @return The index of the style if found, -1 if not found or error.
 */

int OGRStyleTable::IsExist( const char *pszName )
{
    if( pszName == nullptr )
      return -1;

    const int nCount = CSLCount(m_papszStyleTable);
    const char *pszNewString = CPLSPrintf("%s:", pszName);

    for( int i = 0; i < nCount; i++ )
    {
        if( strstr(m_papszStyleTable[i], pszNewString) != nullptr )
        {
            return i;
        }
    }

    return -1;
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

/**
 * \brief Duplicate style table.
 *
 * The newly created style table is owned by the caller, and will have its
 * own reference to the OGRStyleTable.
 *
 * @return new style table, exactly matching this style table.
 */

OGRStyleTable *OGRStyleTable::Clone()

{
    OGRStyleTable *poNew = new OGRStyleTable();

    poNew->m_papszStyleTable = CSLDuplicate( m_papszStyleTable );

    return poNew;
}

/************************************************************************/
/*                            ResetStyleStringReading()                 */
/************************************************************************/

/** Reset the next style pointer to 0 */
void OGRStyleTable::ResetStyleStringReading()

{
    iNextStyle = 0;
}

/************************************************************************/
/*                     OGR_STBL_ResetStyleStringReading()               */
/************************************************************************/

/**
 * \brief Reset the next style pointer to 0
 *
 * This function is the same as the C++ method
 * OGRStyleTable::ResetStyleStringReading().
 *
 * @param hStyleTable handle to the style table.
 *
 */

void OGR_STBL_ResetStyleStringReading( OGRStyleTableH hStyleTable )
{
    VALIDATE_POINTER0( hStyleTable, "OGR_STBL_ResetStyleStringReading" );

    reinterpret_cast<OGRStyleTable *>(hStyleTable)->ResetStyleStringReading();
}

/************************************************************************/
/*                           GetNextStyle()                             */
/************************************************************************/

/**
 * \brief Get the next style string from the table.
 *
 * @return the next style string or NULL on error.
 */

const char *OGRStyleTable::GetNextStyle()
{
    while( iNextStyle < CSLCount(m_papszStyleTable) )
    {
        const char *pszOutput = CSLGetField(m_papszStyleTable, iNextStyle++);
        if( pszOutput == nullptr )
            continue;

        const char *pszDash = strstr(pszOutput, ":");

        osLastRequestedStyleName = pszOutput;
        const size_t nColon = osLastRequestedStyleName.find( ':' );
        if( nColon != std::string::npos )
            osLastRequestedStyleName =
                osLastRequestedStyleName.substr(0, nColon);

        if( pszDash )
            return pszDash + 1;
    }
    return nullptr;
}

/************************************************************************/
/*                     OGR_STBL_GetNextStyle()                          */
/************************************************************************/

/**
 * \brief Get the next style string from the table.
 *
 * This function is the same as the C++ method OGRStyleTable::GetNextStyle().
 *
 * @param hStyleTable handle to the style table.
 *
 * @return the next style string or NULL on error.
 */

const char *OGR_STBL_GetNextStyle( OGRStyleTableH hStyleTable)
{
    VALIDATE_POINTER1( hStyleTable, "OGR_STBL_GetNextStyle", nullptr );

    return reinterpret_cast<OGRStyleTable *>(hStyleTable)->GetNextStyle();
}

/************************************************************************/
/*                           GetLastStyleName()                         */
/************************************************************************/

/**
 * Get the style name of the last style string fetched with
 * OGR_STBL_GetNextStyle.
 *
 * @return the Name of the last style string or NULL on error.
 */

const char *OGRStyleTable::GetLastStyleName()
{
    return osLastRequestedStyleName;
}

/************************************************************************/
/*                     OGR_STBL_GetLastStyleName()                      */
/************************************************************************/

/**
 * Get the style name of the last style string fetched with
 * OGR_STBL_GetNextStyle.
 *
 * This function is the same as the C++ method OGRStyleTable::GetStyleName().
 *
 * @param hStyleTable handle to the style table.
 *
 * @return the Name of the last style string or NULL on error.
 */

const char *OGR_STBL_GetLastStyleName( OGRStyleTableH hStyleTable)
{
    VALIDATE_POINTER1( hStyleTable, "OGR_STBL_GetLastStyleName", nullptr );

    return reinterpret_cast<OGRStyleTable *>(hStyleTable)->GetLastStyleName();
}

/****************************************************************************/
/*                          OGRStyleTool::OGRStyleTool()                    */
/*                                                                          */
/****************************************************************************/

/** Constructor */
OGRStyleTool::OGRStyleTool( OGRSTClassId eClassId ) :
    m_eClassId(eClassId)
{
}

/************************************************************************/
/*                            OGR_ST_Create()                           */
/************************************************************************/
/**
 * \brief OGRStyleTool factory.
 *
 * This function is a constructor for OGRStyleTool derived classes.
 *
 * @param eClassId subclass of style tool to create. One of OGRSTCPen (1),
 * OGRSTCBrush (2), OGRSTCSymbol (3) or OGRSTCLabel (4).
 *
 * @return an handle to the new style tool object or NULL if the creation
 * failed.
 */

OGRStyleToolH OGR_ST_Create( OGRSTClassId eClassId )

{
    switch( eClassId )
    {
      case OGRSTCPen:
        return reinterpret_cast<OGRStyleToolH>(new OGRStylePen());
      case OGRSTCBrush:
        return reinterpret_cast<OGRStyleToolH>(new OGRStyleBrush());
      case OGRSTCSymbol:
        return reinterpret_cast<OGRStyleToolH>(new OGRStyleSymbol());
      case OGRSTCLabel:
        return reinterpret_cast<OGRStyleToolH>(new OGRStyleLabel());
      default:
        return nullptr;
    }
}

/****************************************************************************/
/*                       OGRStyleTool::~OGRStyleTool()                      */
/*                                                                          */
/****************************************************************************/
OGRStyleTool::~OGRStyleTool()
{
    CPLFree(m_pszStyleString);
}

/************************************************************************/
/*                           OGR_ST_Destroy()                            */
/************************************************************************/
/**
 * \brief Destroy Style Tool
 *
 * @param hST handle to the style tool to destroy.
 */

void OGR_ST_Destroy( OGRStyleToolH hST )

{
    delete reinterpret_cast<OGRStyleTool *>(hST);
}

/****************************************************************************/
/*      void OGRStyleTool::SetStyleString(const char *pszStyleString)       */
/*                                                                          */
/****************************************************************************/

/** Undocumented
 * @param pszStyleString undocumented.
 */
void OGRStyleTool::SetStyleString( const char *pszStyleString )
{
    m_pszStyleString = CPLStrdup(pszStyleString);
}

/****************************************************************************/
/*const char *OGRStyleTool::GetStyleString( OGRStyleParamId *pasStyleParam, */
/*                          OGRStyleValue *pasStyleValue, int nSize)        */
/*                                                                          */
/****************************************************************************/

/** Undocumented
 * @param pasStyleParam undocumented.
 * @param pasStyleValue undocumented.
 * @param nSize undocumented.
 * @return undocumented.
 */
const char *OGRStyleTool::GetStyleString( const OGRStyleParamId *pasStyleParam,
                                          OGRStyleValue *pasStyleValue,
                                          int nSize )
{
    if( IsStyleModified() )
    {
        CPLFree(m_pszStyleString);

        const char *pszClass = nullptr;
        switch( GetType() )
        {
          case OGRSTCPen:
            pszClass = "PEN(";
            break;
          case OGRSTCBrush:
            pszClass = "BRUSH(";
            break;
          case OGRSTCSymbol:
            pszClass = "SYMBOL(";
            break;
          case OGRSTCLabel:
            pszClass = "LABEL(";
            break;
          default:
            pszClass = "UNKNOWN(";
        }

        CPLString osCurrent = pszClass;

        bool bFound = false;
        for( int i = 0; i < nSize; i++ )
        {
            if( !pasStyleValue[i].bValid ||
                pasStyleParam[i].eType == OGRSTypeUnused )
            {
                continue;
            }

            if( bFound )
                osCurrent += ",";
            bFound = true;

            osCurrent += pasStyleParam[i].pszToken;
            switch( pasStyleParam[i].eType )
            {
              case OGRSTypeString:
                osCurrent += ":";
                osCurrent += pasStyleValue[i].pszValue;
                break;
              case OGRSTypeDouble:
                osCurrent +=
                    CPLString().Printf(":%f", pasStyleValue[i].dfValue);
                break;
              case OGRSTypeInteger:
                osCurrent += CPLString().Printf(":%d", pasStyleValue[i].nValue);
                break;
              case OGRSTypeBoolean:
                osCurrent += CPLString().Printf(":%d",
                    pasStyleValue[i].nValue != 0);
                break;
              default:
                break;
            }
            if( pasStyleParam[i].bGeoref )
              switch( pasStyleValue[i].eUnit )
              {
                case OGRSTUGround:
                  osCurrent += "g";
                  break;
                case OGRSTUPixel:
                  osCurrent += "px";
                  break;
                case OGRSTUPoints:
                  osCurrent += "pt";
                  break;
                case OGRSTUCM:
                  osCurrent += "cm";
                  break;
                case OGRSTUInches:
                  osCurrent += "in";
                  break;
                case OGRSTUMM:
                  //osCurrent += "mm";
                default:
                  break;    //imp
              }
        }
        osCurrent += ")";

        m_pszStyleString = CPLStrdup(osCurrent);

        m_bModified = FALSE;
    }

    return m_pszStyleString;
}

/************************************************************************/
/*                          GetRGBFromString()                          */
/************************************************************************/
/**
 * \brief Return the r,g,b,a components of a color encoded in \#RRGGBB[AA]
 * format.
 *
 * Maps to OGRStyleTool::GetRGBFromString().
 *
 * @param pszColor the color to parse
 * @param nRed reference to an int in which the red value will be returned.
 * @param nGreen reference to an int in which the green value will be returned.
 * @param nBlue reference to an int in which the blue value will be returned.
 * @param nTransparance reference to an int in which the (optional) alpha value
 * will be returned.
 *
 * @return TRUE if the color could be successfully parsed, or FALSE in case of
 * errors.
 */
GBool OGRStyleTool::GetRGBFromString( const char *pszColor, int &nRed,
                                      int &nGreen, int &nBlue,
                                      int &nTransparance )
{
   int nCount = 0;

   nTransparance = 255;

   // FIXME: should we really use sscanf here?
   unsigned int unRed = 0;
   unsigned int unGreen = 0;
   unsigned int unBlue = 0;
   unsigned int unTransparance = 0;
   if( pszColor )
       nCount = sscanf(pszColor, "#%2x%2x%2x%2x",
                       &unRed, &unGreen, &unBlue, &unTransparance);
   nRed = static_cast<int>(unRed);
   nGreen = static_cast<int>(unGreen);
   nBlue = static_cast<int>(unBlue);
   if( nCount == 4 )
        nTransparance = static_cast<int>(unTransparance);
   return nCount >= 3;
}

/************************************************************************/
/*                           GetSpecificId()                            */
/*                                                                      */
/*      return -1, if the wanted type is not found, ex:                 */
/*      if you want ogr-pen value, pszWanted should be ogr-pen(case     */
/*      sensitive)                                                      */
/************************************************************************/

/** Undocumented
 * @param pszId Undocumented
 * @param pszWanted Undocumented
 * @return Undocumented
 */
int OGRStyleTool::GetSpecificId( const char *pszId, const char *pszWanted )
{
    const char *pszRealWanted = pszWanted;

    if( pszWanted == nullptr || strlen(pszWanted) == 0 )
        pszRealWanted = "ogr-pen";

    if( pszId == nullptr )
        return -1;

    int nValue = -1;
    const char *pszFound = strstr(pszId, pszRealWanted);
    if( pszFound != nullptr )
    {
        // We found the string, it could be no value after it, use default one.
        nValue = 0;

        if( pszFound[strlen(pszRealWanted)] == '-' )
            nValue =atoi(&pszFound[strlen(pszRealWanted)+1]);
    }

    return nValue;
}

/************************************************************************/
/*                              GetType()                               */
/************************************************************************/

/**
 * \brief Determine type of Style Tool
 *
 * @return the style tool type, one of OGRSTCPen (1), OGRSTCBrush (2),
 * OGRSTCSymbol (3) or OGRSTCLabel (4). Returns OGRSTCNone (0) if the
 * OGRStyleToolH is invalid.
 */
OGRSTClassId OGRStyleTool::GetType()
{
    return m_eClassId;
}

/************************************************************************/
/*                           OGR_ST_GetType()                           */
/************************************************************************/
/**
 * \brief Determine type of Style Tool
 *
 * @param hST handle to the style tool.
 *
 * @return the style tool type, one of OGRSTCPen (1), OGRSTCBrush (2),
 * OGRSTCSymbol (3) or OGRSTCLabel (4). Returns OGRSTCNone (0) if the
 * OGRStyleToolH is invalid.
 */

OGRSTClassId OGR_ST_GetType( OGRStyleToolH hST )

{
    VALIDATE_POINTER1( hST, "OGR_ST_GetType", OGRSTCNone );
    return reinterpret_cast<OGRStyleTool *>(hST)->GetType();
}

/************************************************************************/
/*                           OGR_ST_GetUnit()                           */
/************************************************************************/

/**
 * \fn OGRStyleTool::GetUnit()
 * \brief Get Style Tool units
 *
 * @return the style tool units.
 */

/**
 * \brief Get Style Tool units
 *
 * @param hST handle to the style tool.
 *
 * @return the style tool units.
 */

OGRSTUnitId OGR_ST_GetUnit( OGRStyleToolH hST )

{
    VALIDATE_POINTER1( hST, "OGR_ST_GetUnit", OGRSTUGround );
    return reinterpret_cast<OGRStyleTool *>(hST)->GetUnit();
}

/************************************************************************/
/*                              SetUnit()                               */
/************************************************************************/

/**
 * \brief Set Style Tool units
 *
 * @param eUnit the new unit.
 * @param dfGroundPaperScale ground to paper scale factor.
 *
 */
void OGRStyleTool::SetUnit( OGRSTUnitId eUnit, double dfGroundPaperScale )
{
    m_eUnit = eUnit;
    m_dfScale = dfGroundPaperScale;
}

/************************************************************************/
/*                           OGR_ST_SetUnit()                           */
/************************************************************************/
/**
 * \brief Set Style Tool units
 *
 * This function is the same as OGRStyleTool::SetUnit()
 *
 * @param hST handle to the style tool.
 * @param eUnit the new unit.
 * @param dfGroundPaperScale ground to paper scale factor.
 *
 */

void OGR_ST_SetUnit( OGRStyleToolH hST, OGRSTUnitId eUnit,
                     double dfGroundPaperScale )

{
    VALIDATE_POINTER0( hST, "OGR_ST_SetUnit" );
    reinterpret_cast<OGRStyleTool *>(hST)->SetUnit(eUnit, dfGroundPaperScale);
}

/************************************************************************/
/*                               Parse()                                */
/************************************************************************/

//! @cond Doxygen_Suppress
GBool OGRStyleTool::Parse( const OGRStyleParamId *pasStyle,
                           OGRStyleValue *pasValue,
                           int nCount )
{
    if( IsStyleParsed() )
        return TRUE;

    StyleParsed();

    if( m_pszStyleString == nullptr )
        return FALSE;

    // Token to contains StyleString Type and content.
    // Tokenize the String to get the Type and the content
    // Example: Type(elem1:val2,elem2:val2)
    char **papszToken =
        CSLTokenizeString2(m_pszStyleString, "()",
                           CSLT_HONOURSTRINGS
                           | CSLT_PRESERVEQUOTES
                           | CSLT_PRESERVEESCAPES );

    if( CSLCount(papszToken) > 2 || CSLCount(papszToken) == 0 )
    {
        CSLDestroy( papszToken );
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Error in the format of the StyleTool %s", m_pszStyleString);
        return FALSE;
    }

    // Token that will contains StyleString elements.
    // Tokenize the content of the StyleString to get paired components in it.
    char **papszToken2 =
        CSLTokenizeString2( papszToken[1], ",",
                            CSLT_HONOURSTRINGS
                            | CSLT_PRESERVEQUOTES
                            | CSLT_PRESERVEESCAPES );

    // Valid that we have the right StyleString for this feature type.
    switch( GetType() )
    {
      case OGRSTCPen:
        if( !EQUAL(papszToken[0],"PEN") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Error in the Type of StyleTool %s should be a PEN Type",
                     papszToken[0]);
            CSLDestroy( papszToken );
            CSLDestroy( papszToken2 );
            return FALSE;
        }
        break;
      case OGRSTCBrush:
        if( !EQUAL(papszToken[0], "BRUSH") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Error in the Type of StyleTool %s should be a BRUSH Type",
                     papszToken[0]);
            CSLDestroy( papszToken );
            CSLDestroy( papszToken2 );
            return FALSE;
        }
        break;
      case OGRSTCSymbol:
        if( !EQUAL(papszToken[0], "SYMBOL") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Error in the Type of StyleTool %s should be "
                     "a SYMBOL Type",
                     papszToken[0]);
            CSLDestroy( papszToken );
            CSLDestroy( papszToken2 );
            return FALSE;
        }
        break;
      case OGRSTCLabel:
        if( !EQUAL(papszToken[0], "LABEL") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Error in the Type of StyleTool %s should be a LABEL Type",
                     papszToken[0]);
            CSLDestroy( papszToken );
            CSLDestroy( papszToken2 );
            return FALSE;
        }
        break;
      default:
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Error in the Type of StyleTool, Type undetermined");
        CSLDestroy( papszToken );
        CSLDestroy( papszToken2 );
        return FALSE;
        break;
    }

    ////////////////////////////////////////////////////////////////////////
    // Here we will loop on each element in the StyleString. If it is
    // a valid element, we will add it in the StyleTool with
    // SetParamStr().
    //
    // It is important to note that the SetInternalUnit...() is use to update
    // the unit of the StyleTool param (m_eUnit).
    // See OGRStyleTool::SetParamStr().
    // There's a StyleTool unit (m_eUnit), which is the output unit, and each
    // parameter of the style have its own unit value (the input unit). Here we
    // set m_eUnit to the input unit and in SetParamStr(), we will use this
    // value to set the input unit. Then after the loop we will reset m_eUnit
    // to its original value. (Yes it is a side effect / black magic)
    //
    // The pasStyle variable is a global variable passed in argument to the
    // function. See at the top of this file the four OGRStyleParamId
    // variable. They are used to register the valid parameter of each
    // StyleTool.
    ////////////////////////////////////////////////////////////////////////

    // Save Scale and output Units because the parsing code will alter
    // the values.
    OGRSTUnitId eLastUnit = m_eUnit;
    double dSavedScale = m_dfScale;
    const int nElements = CSLCount(papszToken2);

    for( int i = 0; i < nElements; i++ )
    {
        char **papszStylePair =
            CSLTokenizeString2( papszToken2[i], ":",
                                CSLT_HONOURSTRINGS
                                | CSLT_STRIPLEADSPACES
                                | CSLT_STRIPENDSPACES
                                | CSLT_ALLOWEMPTYTOKENS );

        const int nTokens = CSLCount(papszStylePair);

        if( nTokens < 1 || nTokens > 2 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Error in the StyleTool String %s", m_pszStyleString );
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Malformed element #%d (\"%s\") skipped",
                      i, papszToken2[i] );
            CSLDestroy(papszStylePair);
            continue;
        }

        for( int j = 0; j < nCount; j++ )
        {
            if( pasStyle[j].pszToken &&
                EQUAL(pasStyle[j].pszToken, papszStylePair[0]) )
            {
                if( papszStylePair[1] != nullptr && pasStyle[j].bGeoref == TRUE )
                    SetInternalInputUnitFromParam(papszStylePair[1]);

                // Set either the actual value of style parameter or "1"
                // for boolean parameters which do not have values (legacy
                // behavior).
                OGRStyleTool::SetParamStr(
                    pasStyle[j], pasValue[j],
                    papszStylePair[1] != nullptr ? papszStylePair[1] : "1" );

                break;
            }
        }

        CSLDestroy( papszStylePair );
    }

    m_eUnit = eLastUnit;
    m_dfScale = dSavedScale;

    CSLDestroy(papszToken2);
    CSLDestroy(papszToken);

    return TRUE;
}
//! @endcond

/************************************************************************/
/*                   SetInternalInputUnitFromParam()                    */
/************************************************************************/

//! @cond Doxygen_Suppress
void OGRStyleTool::SetInternalInputUnitFromParam( char *pszString )
{
    if( pszString == nullptr )
        return;

    char *pszUnit = strstr(pszString, "g");
    if( pszUnit )
    {
        SetUnit(OGRSTUGround);
        pszUnit[0]= '\0';
        return;
    }
    pszUnit = strstr(pszString, "px");
    if( pszUnit )
    {
        SetUnit(OGRSTUPixel);
        pszUnit[0]= '\0';
        return;
    }
    pszUnit = strstr(pszString, "pt");
    if( pszUnit )
    {
        SetUnit(OGRSTUPoints);
        pszUnit[0]= '\0';
        return;
    }
    pszUnit = strstr(pszString, "mm");
    if( pszUnit )
    {
        SetUnit(OGRSTUMM);
        pszUnit[0]= '\0';
        return;
    }
    pszUnit = strstr(pszString, "cm");
    if( pszUnit )
    {
        SetUnit(OGRSTUCM);
        pszUnit[0]= '\0';
        return;
    }
    pszUnit = strstr(pszString, "in");
    if( pszUnit )
    {
        SetUnit(OGRSTUInches);
        pszUnit[0]= '\0';
        return;
    }

    SetUnit(OGRSTUMM);
}

/************************************************************************/
/*                          ComputeWithUnit()                           */
/************************************************************************/
double OGRStyleTool::ComputeWithUnit( double dfValue, OGRSTUnitId eInputUnit )
{
    OGRSTUnitId eOutputUnit = GetUnit();

    double dfNewValue = dfValue;  // dfValue in meters;

    if( eOutputUnit == eInputUnit )
        return dfValue;

    switch( eInputUnit )
    {
      case OGRSTUGround:
        dfNewValue = dfValue / m_dfScale;
        break;
      case OGRSTUPixel:
        dfNewValue = dfValue / (72.0 * 39.37);
        break;
      case OGRSTUPoints:
        dfNewValue =dfValue / (72.0 * 39.37);
        break;
      case OGRSTUMM:
        dfNewValue = 0.001 * dfValue;
        break;
      case OGRSTUCM:
        dfNewValue = 0.01 * dfValue;
        break;
      case OGRSTUInches:
        dfNewValue = dfValue / 39.37;
        break;
      default:
        break;  // imp.
    }

    switch( eOutputUnit )
    {
      case OGRSTUGround:
        dfNewValue *= m_dfScale;
        break;
      case OGRSTUPixel:
        dfNewValue *= 72.0 * 39.37;
        break;
      case OGRSTUPoints:
        dfNewValue *= 72.0 * 39.37;
        break;
      case OGRSTUMM:
        dfNewValue *= 1000.0;
        break;
      case OGRSTUCM:
        dfNewValue *= 100.0;
        break;
      case OGRSTUInches:
        dfNewValue *= 39.37;
        break;
      default:
        break;  // imp.
    }
    return dfNewValue;
}

/************************************************************************/
/*                          ComputeWithUnit()                           */
/************************************************************************/
int OGRStyleTool::ComputeWithUnit( int nValue, OGRSTUnitId eUnit )
{
    return
        static_cast<int>(ComputeWithUnit(static_cast<double>(nValue), eUnit));
}
//! @endcond

/************************************************************************/
/*                            GetParamStr()                             */
/************************************************************************/

/** Undocumented
 * @param sStyleParam undocumented.
 * @param sStyleValue undocumented.
 * @param bValueIsNull undocumented.
 * @return Undocumented.
 */
const char *OGRStyleTool::GetParamStr( const OGRStyleParamId &sStyleParam ,
                                       OGRStyleValue &sStyleValue,
                                       GBool &bValueIsNull )
{
    if( !Parse() )
    {
        bValueIsNull = TRUE;
        return nullptr;
    }

    bValueIsNull = !sStyleValue.bValid;

    if( bValueIsNull == TRUE )
        return nullptr;

    switch( sStyleParam.eType )
    {
      // If sStyleParam.bGeoref == TRUE, need to convert to output value.
      case OGRSTypeString:
        return sStyleValue.pszValue;
      case OGRSTypeDouble:
        if( sStyleParam.bGeoref )
          return CPLSPrintf("%f", ComputeWithUnit(sStyleValue.dfValue,
                                                  sStyleValue.eUnit));
        else
          return CPLSPrintf("%f", sStyleValue.dfValue);

      case OGRSTypeInteger:
        if( sStyleParam.bGeoref )
          return CPLSPrintf("%d", ComputeWithUnit(sStyleValue.nValue,
                                                  sStyleValue.eUnit));
        else
          return CPLSPrintf("%d", sStyleValue.nValue);
      case OGRSTypeBoolean:
        return CPLSPrintf("%d", sStyleValue.nValue != 0);
      default:
        bValueIsNull = TRUE;
        return nullptr;
    }
}

/****************************************************************************/
/*    int OGRStyleTool::GetParamNum(OGRStyleParamId sStyleParam ,           */
/*                               OGRStyleValue sStyleValue,                 */
/*                               GBool &bValueIsNull)                       */
/*                                                                          */
/****************************************************************************/

/** Undocumented
 * @param sStyleParam undocumented.
 * @param sStyleValue undocumented.
 * @param bValueIsNull undocumented.
 * @return Undocumented.
 */
int OGRStyleTool::GetParamNum( const OGRStyleParamId &sStyleParam ,
                               OGRStyleValue &sStyleValue,
                               GBool &bValueIsNull )
{
    return
        static_cast<int>(GetParamDbl(sStyleParam, sStyleValue, bValueIsNull));
}

/****************************************************************************/
/*       double OGRStyleTool::GetParamDbl(OGRStyleParamId sStyleParam ,     */
/*                               OGRStyleValue sStyleValue,                 */
/*                               GBool &bValueIsNull)                       */
/*                                                                          */
/****************************************************************************/

/** Undocumented
 * @param sStyleParam undocumented.
 * @param sStyleValue undocumented.
 * @param bValueIsNull undocumented.
 * @return Undocumented.
 */
double OGRStyleTool::GetParamDbl( const OGRStyleParamId &sStyleParam ,
                                  OGRStyleValue &sStyleValue,
                                  GBool &bValueIsNull )
{
    if( !Parse() )
    {
        bValueIsNull = TRUE;
        return 0.0;
    }

    bValueIsNull = !sStyleValue.bValid;

    if( bValueIsNull == TRUE )
        return 0.0;

    switch( sStyleParam.eType )
    {
      // if sStyleParam.bGeoref == TRUE, need to convert to output value.
      case OGRSTypeString:
        if( sStyleParam.bGeoref )
          return ComputeWithUnit(CPLAtof(sStyleValue.pszValue),
                                 sStyleValue.eUnit);
        else
          return CPLAtof(sStyleValue.pszValue);
      case OGRSTypeDouble:
        if( sStyleParam.bGeoref )
          return ComputeWithUnit(sStyleValue.dfValue,
                                 sStyleValue.eUnit);
        else
          return sStyleValue.dfValue;
      case OGRSTypeInteger:
        if( sStyleParam.bGeoref )
          return static_cast<double>(
              ComputeWithUnit(sStyleValue.nValue,
                              sStyleValue.eUnit));
        else
          return static_cast<double>(sStyleValue.nValue);
      case OGRSTypeBoolean:
        return static_cast<double>(sStyleValue.nValue != 0);
      default:
        bValueIsNull = TRUE;
        return 0.0;
    }
}

/****************************************************************************/
/*      void OGRStyleTool::SetParamStr(OGRStyleParamId &sStyleParam ,       */
/*                             OGRStyleValue &sStyleValue,                  */
/*                             const char *pszParamString)                  */
/*                                                                          */
/****************************************************************************/

/** Undocumented
 * @param sStyleParam undocumented.
 * @param sStyleValue undocumented.
 * @param pszParamString undocumented.
 */
void OGRStyleTool::SetParamStr( const OGRStyleParamId &sStyleParam ,
                                OGRStyleValue &sStyleValue,
                                const char *pszParamString )
{
    Parse();
    StyleModified();
    sStyleValue.bValid = TRUE;
    sStyleValue.eUnit = GetUnit();
    switch( sStyleParam.eType )
    {
      // If sStyleParam.bGeoref == TRUE, need to convert to output value;
      case OGRSTypeString:
        sStyleValue.pszValue = CPLStrdup(pszParamString);
        break;
      case OGRSTypeDouble:
        sStyleValue.dfValue = CPLAtof(pszParamString);
        break;
      case OGRSTypeInteger:
        sStyleValue.nValue = atoi(pszParamString);
        break;
      case OGRSTypeBoolean:
        sStyleValue.nValue = atoi(pszParamString) != 0;
        break;
      default:
        sStyleValue.bValid = FALSE;
        break;
    }
}

/****************************************************************************/
/*    void OGRStyleTool::SetParamNum(OGRStyleParamId &sStyleParam ,         */
/*                             OGRStyleValue &sStyleValue,                  */
/*                             int nParam)                                  */
/*                                                                          */
/****************************************************************************/

/** Undocumented
 * @param sStyleParam undocumented.
 * @param sStyleValue undocumented.
 * @param nParam undocumented.
 */
void OGRStyleTool::SetParamNum( const OGRStyleParamId &sStyleParam ,
                                OGRStyleValue &sStyleValue,
                                int nParam )
{
    Parse();
    StyleModified();
    sStyleValue.bValid = TRUE;
    sStyleValue.eUnit = GetUnit();
    switch( sStyleParam.eType )
    {

      // If sStyleParam.bGeoref == TRUE, need to convert to output value;
      case OGRSTypeString:
        sStyleValue.pszValue = CPLStrdup(CPLString().Printf("%d", nParam));
        break;
      case OGRSTypeDouble:
        sStyleValue.dfValue = static_cast<double>(nParam);
        break;
      case OGRSTypeInteger:
        sStyleValue.nValue = nParam;
        break;
      case OGRSTypeBoolean:
        sStyleValue.nValue = nParam != 0;
        break;
      default:
        sStyleValue.bValid = FALSE;
        break;
    }
}

/****************************************************************************/
/*      void OGRStyleTool::SetParamDbl(OGRStyleParamId &sStyleParam ,       */
/*                             OGRStyleValue &sStyleValue,                  */
/*                             double dfParam)                              */
/*                                                                          */
/****************************************************************************/

/** Undocumented
 * @param sStyleParam undocumented.
 * @param sStyleValue undocumented.
 * @param dfParam undocumented.
 */
void OGRStyleTool::SetParamDbl( const OGRStyleParamId &sStyleParam,
                                OGRStyleValue &sStyleValue,
                                double dfParam )
{
    Parse();
    StyleModified();
    sStyleValue.bValid = TRUE;
    sStyleValue.eUnit = GetUnit();
    switch( sStyleParam.eType )
    {
      // If sStyleParam.bGeoref == TRUE, need to convert to output value;
      case OGRSTypeString:
        sStyleValue.pszValue = CPLStrdup(CPLString().Printf("%f", dfParam));
        break;
      case OGRSTypeDouble:
        sStyleValue.dfValue = dfParam;
        break;
      case OGRSTypeInteger:
        sStyleValue.nValue = static_cast<int>(dfParam);
        break;
      case OGRSTypeBoolean:
        sStyleValue.nValue = static_cast<int>(dfParam) != 0;
        break;
      default:
        sStyleValue.bValid = FALSE;
        break;
    }
}

/************************************************************************/
/*                           OGR_ST_GetParamStr()                       */
/************************************************************************/
/**
 * \brief Get Style Tool parameter value as string
 *
 * Maps to the OGRStyleTool subclasses' GetParamStr() methods.
 *
 * @param hST handle to the style tool.
 * @param eParam the parameter id from the enumeration corresponding to the
 * type of this style tool (one of the OGRSTPenParam, OGRSTBrushParam,
 * OGRSTSymbolParam or OGRSTLabelParam enumerations)
 * @param bValueIsNull pointer to an integer that will be set to TRUE or FALSE
 * to indicate whether the parameter value is NULL.
 *
 * @return the parameter value as string and sets bValueIsNull.
 */

const char *OGR_ST_GetParamStr( OGRStyleToolH hST, int eParam,
                                int *bValueIsNull )
{
    VALIDATE_POINTER1( hST, "OGR_ST_GetParamStr", "" );
    VALIDATE_POINTER1( bValueIsNull, "OGR_ST_GetParamStr", "" );

    GBool bIsNull = TRUE;
    const char *pszVal = "";

    switch( reinterpret_cast<OGRStyleTool *>(hST)->GetType() )
    {
    case OGRSTCPen:
        pszVal = reinterpret_cast<OGRStylePen *>(hST)->
            GetParamStr(static_cast<OGRSTPenParam>(eParam), bIsNull);
        break;
    case OGRSTCBrush:
        pszVal = reinterpret_cast<OGRStyleBrush *>(hST)->
            GetParamStr(static_cast<OGRSTBrushParam>(eParam), bIsNull);
        break;
    case OGRSTCSymbol:
        pszVal = reinterpret_cast<OGRStyleSymbol *>(hST)->
            GetParamStr(static_cast<OGRSTSymbolParam>(eParam), bIsNull);
        break;
    case OGRSTCLabel:
        pszVal = reinterpret_cast<OGRStyleLabel *>(hST)->
            GetParamStr(static_cast<OGRSTLabelParam>(eParam), bIsNull);
        break;
    default:
        break;
    }

    *bValueIsNull = bIsNull;
    return pszVal;
}

/************************************************************************/
/*                           OGR_ST_GetParamNum()                       */
/************************************************************************/
/**
 * \brief Get Style Tool parameter value as an integer
 *
 * Maps to the OGRStyleTool subclasses' GetParamNum() methods.
 *
 * @param hST handle to the style tool.
 * @param eParam the parameter id from the enumeration corresponding to the
 * type of this style tool (one of the OGRSTPenParam, OGRSTBrushParam,
 * OGRSTSymbolParam or OGRSTLabelParam enumerations)
 * @param bValueIsNull pointer to an integer that will be set to TRUE or FALSE
 * to indicate whether the parameter value is NULL.
 *
 * @return the parameter value as integer and sets bValueIsNull.
 */

int OGR_ST_GetParamNum( OGRStyleToolH hST, int eParam, int *bValueIsNull )
{
    VALIDATE_POINTER1( hST, "OGR_ST_GetParamNum", 0 );
    VALIDATE_POINTER1( bValueIsNull, "OGR_ST_GetParamNum", 0 );

    GBool bIsNull = TRUE;
    int nVal = 0;

    switch( reinterpret_cast<OGRStyleTool *>(hST)->GetType() )
    {
    case OGRSTCPen:
        nVal = reinterpret_cast<OGRStylePen *>(hST)->
            GetParamNum(static_cast<OGRSTPenParam>(eParam), bIsNull);
        break;
    case OGRSTCBrush:
        nVal = reinterpret_cast<OGRStyleBrush *>(hST)->
            GetParamNum(static_cast<OGRSTBrushParam>(eParam), bIsNull);
        break;
    case OGRSTCSymbol:
        nVal = reinterpret_cast<OGRStyleSymbol *>(hST)->
            GetParamNum(static_cast<OGRSTSymbolParam>(eParam), bIsNull);
        break;
    case OGRSTCLabel:
        nVal = reinterpret_cast<OGRStyleLabel *>(hST)->
            GetParamNum(static_cast<OGRSTLabelParam>(eParam), bIsNull);
        break;
    default:
        break;
    }

    *bValueIsNull = bIsNull;
    return nVal;
}

/************************************************************************/
/*                           OGR_ST_GetParamDbl()                       */
/************************************************************************/
/**
 * \brief Get Style Tool parameter value as a double
 *
 * Maps to the OGRStyleTool subclasses' GetParamDbl() methods.
 *
 * @param hST handle to the style tool.
 * @param eParam the parameter id from the enumeration corresponding to the
 * type of this style tool (one of the OGRSTPenParam, OGRSTBrushParam,
 * OGRSTSymbolParam or OGRSTLabelParam enumerations)
 * @param bValueIsNull pointer to an integer that will be set to TRUE or FALSE
 * to indicate whether the parameter value is NULL.
 *
 * @return the parameter value as double and sets bValueIsNull.
 */

double OGR_ST_GetParamDbl( OGRStyleToolH hST, int eParam, int *bValueIsNull )
{
    VALIDATE_POINTER1( hST, "OGR_ST_GetParamDbl", 0.0 );
    VALIDATE_POINTER1( bValueIsNull, "OGR_ST_GetParamDbl", 0.0 );

    GBool bIsNull = TRUE;
    double dfVal = 0.0;

    switch( reinterpret_cast<OGRStyleTool *>(hST)->GetType() )
    {
    case OGRSTCPen:
        dfVal = reinterpret_cast<OGRStylePen *>(hST)->
            GetParamDbl(static_cast<OGRSTPenParam>(eParam), bIsNull);
        break;
    case OGRSTCBrush:
        dfVal = reinterpret_cast<OGRStyleBrush *>(hST)->
            GetParamDbl(static_cast<OGRSTBrushParam>(eParam), bIsNull);
        break;
    case OGRSTCSymbol:
        dfVal = reinterpret_cast<OGRStyleSymbol *>(hST)->
            GetParamDbl(static_cast<OGRSTSymbolParam>(eParam), bIsNull);
        break;
    case OGRSTCLabel:
        dfVal = reinterpret_cast<OGRStyleLabel *>(hST)->
            GetParamDbl(static_cast<OGRSTLabelParam>(eParam), bIsNull);
        break;
    default:
        break;
    }

    *bValueIsNull = bIsNull;
    return dfVal;
}

/************************************************************************/
/*                           OGR_ST_SetParamStr()                       */
/************************************************************************/
/**
 * \brief Set Style Tool parameter value from a string
 *
 * Maps to the OGRStyleTool subclasses' SetParamStr() methods.
 *
 * @param hST handle to the style tool.
 * @param eParam the parameter id from the enumeration corresponding to the
 * type of this style tool (one of the OGRSTPenParam, OGRSTBrushParam,
 * OGRSTSymbolParam or OGRSTLabelParam enumerations)
 * @param pszValue the new parameter value
 *
 */

void OGR_ST_SetParamStr( OGRStyleToolH hST, int eParam, const char *pszValue )
{
    VALIDATE_POINTER0( hST, "OGR_ST_SetParamStr" );
    VALIDATE_POINTER0( pszValue, "OGR_ST_SetParamStr" );

    switch( reinterpret_cast<OGRStyleTool *>(hST)->GetType() )
    {
    case OGRSTCPen:
        reinterpret_cast<OGRStylePen *>(hST)->
            SetParamStr(static_cast<OGRSTPenParam>(eParam), pszValue);
        break;
    case OGRSTCBrush:
        reinterpret_cast<OGRStyleBrush *>(hST)->
            SetParamStr(static_cast<OGRSTBrushParam>(eParam), pszValue);
        break;
    case OGRSTCSymbol:
        reinterpret_cast<OGRStyleSymbol *>(hST)->
            SetParamStr(static_cast<OGRSTSymbolParam>(eParam), pszValue);
        break;
    case OGRSTCLabel:
        reinterpret_cast<OGRStyleLabel *>(hST)->
            SetParamStr(static_cast<OGRSTLabelParam>(eParam), pszValue);
        break;
    default:
        break;
    }
}

/************************************************************************/
/*                           OGR_ST_SetParamNum()                       */
/************************************************************************/
/**
 * \brief Set Style Tool parameter value from an integer
 *
 * Maps to the OGRStyleTool subclasses' SetParamNum() methods.
 *
 * @param hST handle to the style tool.
 * @param eParam the parameter id from the enumeration corresponding to the
 * type of this style tool (one of the OGRSTPenParam, OGRSTBrushParam,
 * OGRSTSymbolParam or OGRSTLabelParam enumerations)
 * @param nValue the new parameter value
 *
 */

void OGR_ST_SetParamNum( OGRStyleToolH hST, int eParam, int nValue )
{
    VALIDATE_POINTER0( hST, "OGR_ST_SetParamNum" );

    switch( reinterpret_cast<OGRStyleTool *>(hST)->GetType() )
    {
    case OGRSTCPen:
        reinterpret_cast<OGRStylePen *>(hST)->
            SetParamNum(static_cast<OGRSTPenParam>(eParam), nValue);
        break;
    case OGRSTCBrush:
        reinterpret_cast<OGRStyleBrush *>(hST)->
            SetParamNum(static_cast<OGRSTBrushParam>(eParam), nValue);
        break;
    case OGRSTCSymbol:
        reinterpret_cast<OGRStyleSymbol *>(hST)->
            SetParamNum(static_cast<OGRSTSymbolParam>(eParam), nValue);
        break;
    case OGRSTCLabel:
        reinterpret_cast<OGRStyleLabel *>(hST)->
            SetParamNum(static_cast<OGRSTLabelParam>(eParam), nValue);
        break;
    default:
        break;
    }
}

/************************************************************************/
/*                           OGR_ST_SetParamDbl()                       */
/************************************************************************/
/**
 * \brief Set Style Tool parameter value from a double
 *
 * Maps to the OGRStyleTool subclasses' SetParamDbl() methods.
 *
 * @param hST handle to the style tool.
 * @param eParam the parameter id from the enumeration corresponding to the
 * type of this style tool (one of the OGRSTPenParam, OGRSTBrushParam,
 * OGRSTSymbolParam or OGRSTLabelParam enumerations)
 * @param dfValue the new parameter value
 *
 */

void OGR_ST_SetParamDbl( OGRStyleToolH hST, int eParam, double dfValue )
{
    VALIDATE_POINTER0( hST, "OGR_ST_SetParamDbl" );

    switch( reinterpret_cast<OGRStyleTool *>(hST)->GetType() )
    {
    case OGRSTCPen:
        reinterpret_cast<OGRStylePen *>(hST)->
            SetParamDbl(static_cast<OGRSTPenParam>(eParam), dfValue);
        break;
    case OGRSTCBrush:
        reinterpret_cast<OGRStyleBrush *>(hST)->
            SetParamDbl(static_cast<OGRSTBrushParam>(eParam), dfValue);
        break;
    case OGRSTCSymbol:
        reinterpret_cast<OGRStyleSymbol *>(hST)->
            SetParamDbl(static_cast<OGRSTSymbolParam>(eParam), dfValue);
        break;
    case OGRSTCLabel:
        reinterpret_cast<OGRStyleLabel *>(hST)->
            SetParamDbl(static_cast<OGRSTLabelParam>(eParam), dfValue);
        break;
    default:
        break;
    }
}

/************************************************************************/
/*                           OGR_ST_GetStyleString()                    */
/************************************************************************/

/**
 * \fn OGRStyleTool::GetStyleString()
 * \brief Get the style string for this Style Tool
 *
 * Maps to the OGRStyleTool subclasses' GetStyleString() methods.
 *
 * @return the style string for this style tool or "" if the hST is invalid.
 */

/**
 * \brief Get the style string for this Style Tool
 *
 * Maps to the OGRStyleTool subclasses' GetStyleString() methods.
 *
 * @param hST handle to the style tool.
 *
 * @return the style string for this style tool or "" if the hST is invalid.
 */

const char *OGR_ST_GetStyleString( OGRStyleToolH hST )
{
    const char *pszVal = "";

    VALIDATE_POINTER1( hST, "OGR_ST_GetStyleString", "" );

    switch( reinterpret_cast<OGRStyleTool *>(hST)->GetType() )
    {
      case OGRSTCPen:
        pszVal = reinterpret_cast<OGRStylePen *>(hST)->GetStyleString();
        break;
      case OGRSTCBrush:
        pszVal = reinterpret_cast<OGRStyleBrush *>(hST)->GetStyleString();
        break;
      case OGRSTCSymbol:
        pszVal = reinterpret_cast<OGRStyleSymbol *>(hST)->GetStyleString();
        break;
      case OGRSTCLabel:
        pszVal = reinterpret_cast<OGRStyleLabel *>(hST)->GetStyleString();
        break;
      default:
        break;
    }

    return pszVal;
}

/************************************************************************/
/*                           OGR_ST_GetRGBFromString()                  */
/************************************************************************/
/**
 * \brief Return the r,g,b,a components of a color encoded in \#RRGGBB[AA]
 * format.
 *
 * Maps to OGRStyleTool::GetRGBFromString().
 *
 * @param hST handle to the style tool.
 * @param pszColor the color to parse
 * @param pnRed pointer to an int in which the red value will be returned
 * @param pnGreen pointer to an int in which the green value will be returned
 * @param pnBlue pointer to an int in which the blue value will be returned
 * @param pnAlpha pointer to an int in which the (optional) alpha value will
 * be returned
 *
 * @return TRUE if the color could be successfully parsed, or FALSE in case of
 * errors.
 */

int OGR_ST_GetRGBFromString( OGRStyleToolH hST, const char *pszColor,
                             int *pnRed, int *pnGreen, int *pnBlue,
                             int *pnAlpha )
{

    VALIDATE_POINTER1( hST, "OGR_ST_GetRGBFromString", FALSE );
    VALIDATE_POINTER1( pnRed, "OGR_ST_GetRGBFromString", FALSE );
    VALIDATE_POINTER1( pnGreen, "OGR_ST_GetRGBFromString", FALSE );
    VALIDATE_POINTER1( pnBlue, "OGR_ST_GetRGBFromString", FALSE );
    VALIDATE_POINTER1( pnAlpha, "OGR_ST_GetRGBFromString", FALSE );

    return reinterpret_cast<OGRStyleTool *>(hST)->
        GetRGBFromString(pszColor, *pnRed, *pnGreen,
                         *pnBlue, *pnAlpha );
}

//! @cond Doxygen_Suppress
/* ======================================================================== */
/*                OGRStylePen                                               */
/*       Specific parameter (Set/Get) for the StylePen                      */
/* ======================================================================== */

/****************************************************************************/
/*                      OGRStylePen::OGRStylePen()                          */
/*                                                                          */
/****************************************************************************/
OGRStylePen::OGRStylePen() :
    OGRStyleTool(OGRSTCPen),
    m_pasStyleValue( static_cast<OGRStyleValue *>(
        CPLCalloc(OGRSTPenLast, sizeof(OGRStyleValue))))
{
}

/****************************************************************************/
/*                      OGRStylePen::~OGRStylePen()                         */
/*                                                                          */
/****************************************************************************/
OGRStylePen::~OGRStylePen()
{
    for( int i = 0; i < OGRSTPenLast; i++ )
    {
        if( m_pasStyleValue[i].pszValue != nullptr )
        {
            CPLFree(m_pasStyleValue[i].pszValue);
            m_pasStyleValue[i].pszValue = nullptr;
        }
    }

    CPLFree(m_pasStyleValue);
}

/************************************************************************/
/*                         OGRStylePen::Parse()                         */
/************************************************************************/
GBool OGRStylePen::Parse()

{
    return OGRStyleTool::Parse(asStylePen, m_pasStyleValue,
                               static_cast<int>(OGRSTPenLast));
}

/************************************************************************/
/*                            GetParamStr()                             */
/************************************************************************/
const char *OGRStylePen::GetParamStr( OGRSTPenParam eParam,
                                      GBool &bValueIsNull )
{
    return OGRStyleTool::GetParamStr(asStylePen[eParam],
                                     m_pasStyleValue[eParam],
                                     bValueIsNull);
}

/************************************************************************/
/*                            GetParamNum()                             */
/************************************************************************/
int OGRStylePen::GetParamNum( OGRSTPenParam eParam, GBool &bValueIsNull )
{
    return OGRStyleTool::GetParamNum(asStylePen[eParam],
                                     m_pasStyleValue[eParam], bValueIsNull);
}

/************************************************************************/
/*                            GetParamDbl()                             */
/************************************************************************/
double OGRStylePen::GetParamDbl( OGRSTPenParam eParam, GBool &bValueIsNull )
{
    return OGRStyleTool::GetParamDbl(asStylePen[eParam],
                                     m_pasStyleValue[eParam], bValueIsNull);
}

/************************************************************************/
/*                            SetParamStr()                             */
/************************************************************************/

void OGRStylePen::SetParamStr( OGRSTPenParam eParam,
                               const char *pszParamString )
{
    OGRStyleTool::SetParamStr(asStylePen[eParam], m_pasStyleValue[eParam],
                              pszParamString);
}

/************************************************************************/
/*                            SetParamNum()                             */
/************************************************************************/
void OGRStylePen::SetParamNum( OGRSTPenParam eParam, int nParam )
{
    OGRStyleTool::SetParamNum(asStylePen[eParam],
                              m_pasStyleValue[eParam], nParam);
}

/************************************************************************/
/*                            SetParamDbl()                             */
/************************************************************************/
void OGRStylePen::SetParamDbl( OGRSTPenParam eParam, double dfParam )
{
    OGRStyleTool::SetParamDbl(asStylePen[eParam],
                              m_pasStyleValue[eParam], dfParam);
}

/************************************************************************/
/*                           GetStyleString()                           */
/************************************************************************/
const char *OGRStylePen::GetStyleString()
{
    return OGRStyleTool::GetStyleString(asStylePen, m_pasStyleValue,
                                        static_cast<int>(OGRSTPenLast));
}

/****************************************************************************/
/*                      OGRStyleBrush::OGRStyleBrush()                      */
/*                                                                          */
/****************************************************************************/
OGRStyleBrush::OGRStyleBrush() :
    OGRStyleTool(OGRSTCBrush),
    m_pasStyleValue( static_cast<OGRStyleValue *>(
        CPLCalloc(OGRSTBrushLast, sizeof(OGRStyleValue))))
{
}

/****************************************************************************/
/*                      OGRStyleBrush::~OGRStyleBrush()                     */
/*                                                                          */
/****************************************************************************/
OGRStyleBrush::~OGRStyleBrush()
{
    for( int i = 0; i < OGRSTBrushLast; i++ )
    {
        if( m_pasStyleValue[i].pszValue != nullptr )
        {
            CPLFree(m_pasStyleValue[i].pszValue);
            m_pasStyleValue[i].pszValue = nullptr;
        }
    }

    CPLFree(m_pasStyleValue);
}

/************************************************************************/
/*                               Parse()                                */
/************************************************************************/
GBool OGRStyleBrush::Parse()
{
    return OGRStyleTool::Parse(asStyleBrush, m_pasStyleValue,
                               static_cast<int>(OGRSTBrushLast));
}

/************************************************************************/
/*                            GetParamStr()                             */
/************************************************************************/
const char *OGRStyleBrush::GetParamStr( OGRSTBrushParam eParam,
                                        GBool &bValueIsNull )
{
    return OGRStyleTool::GetParamStr(asStyleBrush[eParam],
                                     m_pasStyleValue[eParam],
                                     bValueIsNull);
}

/************************************************************************/
/*                            GetParamNum()                             */
/************************************************************************/
int OGRStyleBrush::GetParamNum( OGRSTBrushParam eParam, GBool &bValueIsNull )
{
    return OGRStyleTool::GetParamNum(asStyleBrush[eParam],
                                     m_pasStyleValue[eParam], bValueIsNull);
}

/************************************************************************/
/*                            GetParamDbl()                             */
/************************************************************************/
double OGRStyleBrush::GetParamDbl( OGRSTBrushParam eParam, GBool &bValueIsNull )
{
    return OGRStyleTool::GetParamDbl(asStyleBrush[eParam],
                                     m_pasStyleValue[eParam], bValueIsNull);
}

/************************************************************************/
/*                            SetParamStr()                             */
/************************************************************************/
void OGRStyleBrush::SetParamStr( OGRSTBrushParam eParam,
                                 const char *pszParamString )
{
    OGRStyleTool::SetParamStr(asStyleBrush[eParam], m_pasStyleValue[eParam],
                              pszParamString);
}

/************************************************************************/
/*                            SetParamNum()                             */
/************************************************************************/
void OGRStyleBrush::SetParamNum( OGRSTBrushParam eParam, int nParam )
{
    OGRStyleTool::SetParamNum(asStyleBrush[eParam],
                              m_pasStyleValue[eParam], nParam);
}

/************************************************************************/
/*                            SetParamDbl()                             */
/************************************************************************/
void OGRStyleBrush::SetParamDbl( OGRSTBrushParam eParam, double dfParam )
{
    OGRStyleTool::SetParamDbl(asStyleBrush[eParam],
                              m_pasStyleValue[eParam], dfParam);
}

/************************************************************************/
/*                           GetStyleString()                           */
/************************************************************************/
const char *OGRStyleBrush::GetStyleString()
{
    return OGRStyleTool::GetStyleString(asStyleBrush, m_pasStyleValue,
                                        static_cast<int>(OGRSTBrushLast));
}

/****************************************************************************/
/*                      OGRStyleSymbol::OGRStyleSymbol()                    */
/****************************************************************************/
OGRStyleSymbol::OGRStyleSymbol() :
    OGRStyleTool(OGRSTCSymbol),
    m_pasStyleValue( static_cast<OGRStyleValue *>(
        CPLCalloc(OGRSTSymbolLast, sizeof(OGRStyleValue))))
{
}

/****************************************************************************/
/*                      OGRStyleSymbol::~OGRStyleSymbol()                   */
/*                                                                          */
/****************************************************************************/
OGRStyleSymbol::~OGRStyleSymbol()
{
    for( int i = 0; i < OGRSTSymbolLast; i++ )
    {
        if( m_pasStyleValue[i].pszValue != nullptr )
        {
            CPLFree(m_pasStyleValue[i].pszValue);
            m_pasStyleValue[i].pszValue = nullptr;
        }
    }

    CPLFree(m_pasStyleValue);
}

/************************************************************************/
/*                               Parse()                                */
/************************************************************************/
GBool OGRStyleSymbol::Parse()
{
    return OGRStyleTool::Parse(asStyleSymbol, m_pasStyleValue,
                               static_cast<int>(OGRSTSymbolLast));
}

/************************************************************************/
/*                            GetParamStr()                             */
/************************************************************************/
const char *OGRStyleSymbol::GetParamStr( OGRSTSymbolParam eParam,
                                         GBool &bValueIsNull )
{
    return OGRStyleTool::GetParamStr(asStyleSymbol[eParam],
                                     m_pasStyleValue[eParam],
                                     bValueIsNull);
}
/************************************************************************/
/*                            GetParamNum()                             */
/************************************************************************/
int OGRStyleSymbol::GetParamNum( OGRSTSymbolParam eParam, GBool &bValueIsNull )
{
    return OGRStyleTool::GetParamNum(asStyleSymbol[eParam],
                                     m_pasStyleValue[eParam], bValueIsNull);
}
/************************************************************************/
/*                            GetParamDbl()                             */
/************************************************************************/
double OGRStyleSymbol::GetParamDbl( OGRSTSymbolParam eParam,
                                    GBool &bValueIsNull )
{
    return OGRStyleTool::GetParamDbl(asStyleSymbol[eParam],
                                     m_pasStyleValue[eParam], bValueIsNull);
}

/************************************************************************/
/*                            SetParamStr()                             */
/************************************************************************/
void OGRStyleSymbol::SetParamStr( OGRSTSymbolParam eParam,
                                  const char *pszParamString )
{
    OGRStyleTool::SetParamStr(asStyleSymbol[eParam], m_pasStyleValue[eParam],
                              pszParamString);
}

/************************************************************************/
/*                            SetParamNum()                             */
/************************************************************************/
void OGRStyleSymbol::SetParamNum( OGRSTSymbolParam eParam, int nParam )
{
    OGRStyleTool::SetParamNum(asStyleSymbol[eParam],
                              m_pasStyleValue[eParam], nParam);
}

/************************************************************************/
/*                            SetParamDbl()                             */
/************************************************************************/
void OGRStyleSymbol::SetParamDbl( OGRSTSymbolParam eParam, double dfParam )
{
    OGRStyleTool::SetParamDbl(asStyleSymbol[eParam],
                              m_pasStyleValue[eParam], dfParam);
}
/************************************************************************/
/*                           GetStyleString()                           */
/************************************************************************/
const char *OGRStyleSymbol::GetStyleString()
{
    return OGRStyleTool::GetStyleString(asStyleSymbol, m_pasStyleValue,
                                        static_cast<int>(OGRSTSymbolLast));
}

/****************************************************************************/
/*                      OGRStyleLabel::OGRStyleLabel()                      */
/*                                                                          */
/****************************************************************************/
OGRStyleLabel::OGRStyleLabel() :
    OGRStyleTool(OGRSTCLabel),
    m_pasStyleValue( static_cast<OGRStyleValue *>(
        CPLCalloc(OGRSTLabelLast, sizeof(OGRStyleValue))))
{
}

/****************************************************************************/
/*                      OGRStyleLabel::~OGRStyleLabel()                     */
/*                                                                          */
/****************************************************************************/
OGRStyleLabel::~OGRStyleLabel()
{
    for( int i = 0; i < OGRSTLabelLast; i++ )
    {
        if( m_pasStyleValue[i].pszValue != nullptr )
        {
            CPLFree(m_pasStyleValue[i].pszValue);
            m_pasStyleValue[i].pszValue = nullptr;
        }
    }

    CPLFree(m_pasStyleValue);
}

/************************************************************************/
/*                               Parse()                                */
/************************************************************************/
GBool OGRStyleLabel::Parse()
{
    return OGRStyleTool::Parse(asStyleLabel, m_pasStyleValue,
                               static_cast<int>(OGRSTLabelLast));
}

/************************************************************************/
/*                            GetParamStr()                             */
/************************************************************************/
const char *OGRStyleLabel::GetParamStr( OGRSTLabelParam eParam,
                                        GBool &bValueIsNull )
{
    return OGRStyleTool::GetParamStr(asStyleLabel[eParam],
                                     m_pasStyleValue[eParam],
                                     bValueIsNull);
}
/************************************************************************/
/*                            GetParamNum()                             */
/************************************************************************/
int OGRStyleLabel::GetParamNum( OGRSTLabelParam eParam,
                                GBool &bValueIsNull )
{
    return OGRStyleTool::GetParamNum(asStyleLabel[eParam],
                                     m_pasStyleValue[eParam], bValueIsNull);
}
/************************************************************************/
/*                            GetParamDbl()                             */
/************************************************************************/
double OGRStyleLabel::GetParamDbl( OGRSTLabelParam eParam,
                                   GBool &bValueIsNull )
{
    return OGRStyleTool::GetParamDbl(asStyleLabel[eParam],
                                     m_pasStyleValue[eParam], bValueIsNull);
}
/************************************************************************/
/*                            SetParamStr()                             */
/************************************************************************/
void OGRStyleLabel::SetParamStr( OGRSTLabelParam eParam,
                                 const char *pszParamString )
{
    OGRStyleTool::SetParamStr(asStyleLabel[eParam], m_pasStyleValue[eParam],
                              pszParamString);
}
/************************************************************************/
/*                            SetParamNum()                             */
/************************************************************************/
void OGRStyleLabel::SetParamNum( OGRSTLabelParam eParam, int nParam )
{
    OGRStyleTool::SetParamNum(asStyleLabel[eParam],
                              m_pasStyleValue[eParam], nParam);
}

/************************************************************************/
/*                            SetParamDbl()                             */
/************************************************************************/
void OGRStyleLabel::SetParamDbl( OGRSTLabelParam eParam, double dfParam )
{
    OGRStyleTool::SetParamDbl(asStyleLabel[eParam],
                              m_pasStyleValue[eParam], dfParam);
}
/************************************************************************/
/*                           GetStyleString()                           */
/************************************************************************/
const char *OGRStyleLabel::GetStyleString()
{
    return OGRStyleTool::GetStyleString(asStyleLabel, m_pasStyleValue,
                                        static_cast<int>(OGRSTLabelLast));
}
//! @endcond
