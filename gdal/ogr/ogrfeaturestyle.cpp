/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Feature Representation string API
 * Author:   Stephane Villeneuve, stephane.v@videotron.ca
 *
 ******************************************************************************
 * Copyright (c) 2000-2001, Stephane Villeneuve
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_feature.h"
#include "ogr_featurestyle.h"
#include "ogr_api.h"

CPL_CVSID("$Id$");

CPL_C_START
void OGRFeatureStylePuller() {}
CPL_C_END

/****************************************************************************/
/*                Class Parameter (used in the String)                      */
/*                                                                          */
/*      The order of all parameter MUST be the same than in the definition  */
/****************************************************************************/
static const OGRStyleParamId asStylePen[] =
{
    {OGRSTPenColor,"c",FALSE,OGRSTypeString},
    {OGRSTPenWidth,"w",TRUE,OGRSTypeDouble},
    {OGRSTPenPattern,"p",TRUE,OGRSTypeString},
    {OGRSTPenId,"id",FALSE,OGRSTypeString},
    {OGRSTPenPerOffset,"dp",TRUE,OGRSTypeDouble},
    {OGRSTPenCap,"cap",FALSE,OGRSTypeString},
    {OGRSTPenJoin,"j",FALSE,OGRSTypeString},
    {OGRSTPenPriority, "l", FALSE, OGRSTypeInteger}
};

static const OGRStyleParamId asStyleBrush[] =
{
    {OGRSTBrushFColor,"fc",FALSE,OGRSTypeString},
    {OGRSTBrushBColor,"bc",FALSE,OGRSTypeString},
    {OGRSTBrushId,"id",FALSE,OGRSTypeString},
    {OGRSTBrushAngle,"a",FALSE,OGRSTypeDouble},
    {OGRSTBrushSize,"s",TRUE,OGRSTypeDouble},
    {OGRSTBrushDx,"dx",TRUE,OGRSTypeDouble},
    {OGRSTBrushDy,"dy",TRUE,OGRSTypeDouble},
    {OGRSTBrushPriority,"l",FALSE,OGRSTypeInteger}
};

static const OGRStyleParamId asStyleSymbol[] = 
{
    {OGRSTSymbolId,"id",FALSE,OGRSTypeString},
    {OGRSTSymbolAngle,"a",FALSE,OGRSTypeDouble},
    {OGRSTSymbolColor,"c",FALSE,OGRSTypeString},
    {OGRSTSymbolSize,"s",TRUE,OGRSTypeDouble},
    {OGRSTSymbolDx,"dx",TRUE,OGRSTypeDouble},
    {OGRSTSymbolDy,"dy",TRUE,OGRSTypeDouble},
    {OGRSTSymbolStep,"ds",TRUE,OGRSTypeDouble},
    {OGRSTSymbolPerp,"dp",TRUE,OGRSTypeDouble},
    {OGRSTSymbolOffset,"di",TRUE,OGRSTypeDouble},
    {OGRSTSymbolPriority,"l",FALSE,OGRSTypeInteger},
    {OGRSTSymbolFontName,"f",FALSE,OGRSTypeString},
    {OGRSTSymbolOColor,"o",FALSE,OGRSTypeString}
};

static const OGRStyleParamId asStyleLabel[] =
{
    {OGRSTLabelFontName,"f",FALSE,OGRSTypeString},
    {OGRSTLabelSize,"s",TRUE,OGRSTypeDouble},
    {OGRSTLabelTextString,"t",FALSE, OGRSTypeString},
    {OGRSTLabelAngle,"a",FALSE,OGRSTypeDouble},
    {OGRSTLabelFColor,"c",FALSE,OGRSTypeString},
    {OGRSTLabelBColor,"b",FALSE,OGRSTypeString},
    {OGRSTLabelPlacement,"m",FALSE, OGRSTypeString},
    {OGRSTLabelAnchor,"p",FALSE,OGRSTypeInteger},
    {OGRSTLabelDx,"dx",TRUE,OGRSTypeDouble},
    {OGRSTLabelDy,"dy",TRUE,OGRSTypeDouble},
    {OGRSTLabelPerp,"dp",TRUE,OGRSTypeDouble},
    {OGRSTLabelBold,"bo",FALSE,OGRSTypeBoolean},
    {OGRSTLabelItalic,"it",FALSE,OGRSTypeBoolean},
    {OGRSTLabelUnderline,"un",FALSE, OGRSTypeBoolean},
    {OGRSTLabelPriority,"l",FALSE, OGRSTypeInteger},
    {OGRSTLabelStrikeout,"st",FALSE, OGRSTypeBoolean},
    {OGRSTLabelStretch,"w",FALSE, OGRSTypeDouble},
    {OGRSTLabelAdjHor,"ah",FALSE, OGRSTypeString},
    {OGRSTLabelAdjVert,"av",FALSE, OGRSTypeString},
    {OGRSTLabelHColor,"h",FALSE,OGRSTypeString},
    {OGRSTLabelOColor,"o",FALSE,OGRSTypeString}
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
 * @param poDataSetStyleTable (currently unused, reserved for future use), pointer 
 * to OGRStyleTable. Pass NULL for now.
 */
OGRStyleMgr::OGRStyleMgr(OGRStyleTable *poDataSetStyleTable)
{
    m_poDataSetStyleTable = poDataSetStyleTable;
    m_pszStyleString = NULL;
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
    return (OGRStyleMgrH) new OGRStyleMgr( (OGRStyleTable *) hStyleTable );
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
    if ( m_pszStyleString )
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
    delete (OGRStyleMgr *) hSM;
}


/****************************************************************************/
/*      GBool OGRStyleMgr::SetFeatureStyleString(OGRFeature *poFeature,     */
/*                                       char *pszStyleString,              */
/*                                       GBool bNoMatching)                 */
/*      Set the gived representation to the feature,                        */
/*      if bNoMatching == TRUE, don't try to find it in the styletable      */
/*      otherwize, we will use the name defined in the styletable           */
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
 
GBool OGRStyleMgr::SetFeatureStyleString(OGRFeature *poFeature, 
                                         const char *pszStyleString,
                                         GBool bNoMatching)
{
    const char *pszName;
    if (poFeature == FALSE)
      return FALSE;
    
    if (pszStyleString == NULL)
      poFeature->SetStyleString("");
    else if (bNoMatching == TRUE)
      poFeature->SetStyleString(pszStyleString);
    else if ((pszName = GetStyleName(pszStyleString)) != NULL)
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

const char *OGRStyleMgr::InitFromFeature(OGRFeature *poFeature)
{
    CPLFree(m_pszStyleString);
    m_pszStyleString = NULL;

    if (poFeature)
      InitStyleString(poFeature->GetStyleString());
    else
      m_pszStyleString = NULL;

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

const char *OGR_SM_InitFromFeature(OGRStyleMgrH hSM, 
                                           OGRFeatureH hFeat)

{
    VALIDATE_POINTER1( hSM, "OGR_SM_InitFromFeature", NULL );
    VALIDATE_POINTER1( hFeat, "OGR_SM_InitFromFeature", NULL );

    return ((OGRStyleMgr *) hSM)->InitFromFeature((OGRFeature *)hFeat);
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
GBool OGRStyleMgr::InitStyleString(const char *pszStyleString)
{
    CPLFree(m_pszStyleString);
    m_pszStyleString = NULL;

    if (pszStyleString && pszStyleString[0] == '@')
      m_pszStyleString = CPLStrdup(GetStyleByName(pszStyleString));
    else
      m_pszStyleString = NULL;

    if (m_pszStyleString == NULL && pszStyleString)
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

int OGR_SM_InitStyleString(OGRStyleMgrH hSM, const char *pszStyleString)

{
    VALIDATE_POINTER1( hSM, "OGR_SM_InitStyleString", FALSE );

    return ((OGRStyleMgr *) hSM)->InitStyleString(pszStyleString);
}


/****************************************************************************/
/*      const char *OGRStyleMgr::GetStyleName(const char *pszStyleString)   */
/*                                                                          */
/****************************************************************************/

/**
 * \brief Get the name of a style from the style table.
 *
 * @param pszStyleString  the style to search for, or NULL to use the style
 *   currently stored in the manager.
 *
 * @return The name if found, or NULL on error.
 */

const char *OGRStyleMgr::GetStyleName(const char *pszStyleString)
{

    // SECURITY:  the unit and the value for all parameter should be the same,
    // a text comparaison is executed .

    const char *pszStyle;

    if (pszStyleString)
      pszStyle = pszStyleString;
    else
      pszStyle = m_pszStyleString;

    if (pszStyle)
    {
        if (m_poDataSetStyleTable)
          return  m_poDataSetStyleTable->GetStyleName(pszStyle);
    }
    return NULL;
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
const char *OGRStyleMgr::GetStyleByName(const char *pszStyleName)
{    
    if (m_poDataSetStyleTable)
    {
        return  m_poDataSetStyleTable->Find(pszStyleName);
    }
    return NULL;
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

GBool OGRStyleMgr::AddStyle(const char *pszStyleName, 
                            const char *pszStyleString)
{
    const char *pszStyle;

    if (pszStyleString)
      pszStyle = pszStyleString;
    else
      pszStyle = m_pszStyleString;

    if (m_poDataSetStyleTable)
    {
        return m_poDataSetStyleTable->AddStyle(pszStyleName, pszStyle);
    }
    return FALSE;
}


/************************************************************************/
/*                     OGR_SM_AddStyle()                         */
/************************************************************************/

/**
 * Add a style to the current style table.
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

int OGR_SM_AddStyle(OGRStyleMgrH hSM, const char *pszStyleName, 
                    const char *pszStyleString)
{
    VALIDATE_POINTER1( hSM, "OGR_SM_AddStyle", FALSE );
    VALIDATE_POINTER1( pszStyleName, "OGR_SM_AddStyle", FALSE );
    
    return ((OGRStyleMgr *) hSM)->AddStyle( pszStyleName, pszStyleString);
}


/****************************************************************************/
/*            const char *OGRStyleMgr::GetStyleString(OGRFeature *)         */
/*                                                                          */
/****************************************************************************/

const char *OGRStyleMgr::GetStyleString(OGRFeature *poFeature)
{
    if (poFeature == NULL)
      return m_pszStyleString;
    else
      return InitFromFeature(poFeature);
}

GBool OGRStyleMgr::AddPart(const char *pszPart)
{
    char *pszTmp; 
    if (pszPart)
    {
        if (m_pszStyleString)
        {
            pszTmp = CPLStrdup(CPLString().Printf("%s;%s",m_pszStyleString,
                                          pszPart));
            CPLFree(m_pszStyleString);
            m_pszStyleString = pszTmp;
        }
        else
        {
              pszTmp= CPLStrdup(CPLString().Printf("%s",pszPart));
              CPLFree(m_pszStyleString);
              m_pszStyleString = pszTmp;
        }
        return TRUE;
    }

    return FALSE;
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

GBool OGRStyleMgr::AddPart(OGRStyleTool *poStyleTool)
{
    char *pszTmp;
    if (poStyleTool)
    {
        if (m_pszStyleString)
        {
            pszTmp = CPLStrdup(CPLString().Printf("%s;%s",m_pszStyleString,
                                        poStyleTool->GetStyleString()));
            CPLFree(m_pszStyleString);
            m_pszStyleString = pszTmp;
        }
        else
        {
              pszTmp= CPLStrdup(CPLString().Printf("%s",
                                        poStyleTool->GetStyleString()));
              CPLFree(m_pszStyleString);
              m_pszStyleString = pszTmp;
        }
        return TRUE;
    }

    return FALSE;
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

int OGR_SM_AddPart(OGRStyleMgrH hSM, OGRStyleToolH hST)

{
    VALIDATE_POINTER1( hSM, "OGR_SM_InitStyleString", FALSE );
    VALIDATE_POINTER1( hST, "OGR_SM_InitStyleString", FALSE );

    return ((OGRStyleMgr *) hSM)->AddPart((OGRStyleTool *)hST);
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

int OGRStyleMgr::GetPartCount(const char *pszStyleString)
{
    const char *pszPart;
    int nPartCount = 1;
    const char *pszString;
    const char *pszStrTmp;

    if (pszStyleString != NULL)
      pszString = pszStyleString;
    else
      pszString = m_pszStyleString;

    if (pszString == NULL)
      return 0;

    pszStrTmp = pszString;
    // Search for parts separated by semicolons not counting the possible
    // semicolon at the and of string.
    while ((pszPart = strstr(pszStrTmp, ";")) != NULL && pszPart[1] != '\0')
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

int OGR_SM_GetPartCount(OGRStyleMgrH hSM, const char *pszStyleString)

{
    VALIDATE_POINTER1( hSM, "OGR_SM_InitStyleString", FALSE );

    return ((OGRStyleMgr *) hSM)->GetPartCount(pszStyleString);
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
 * @param nPartId the part number (0-based index).
 * @param pszStyleString (optional) the style string on which to operate.
 * If NULL then the current style string stored in the style manager is used.
 *
 * @return OGRStyleTool of the requested part (style tools) or NULL on error.
 */ 

OGRStyleTool *OGRStyleMgr::GetPart(int nPartId, 
                                   const char *pszStyleString)
{
    char **papszStyleString;
    const char *pszStyle;
    const char *pszString;
    OGRStyleTool    *poStyleTool = NULL;

    if (pszStyleString)
      pszStyle = pszStyleString; 
    else
      pszStyle = m_pszStyleString;

    if (pszStyle == NULL)
      return NULL;

    papszStyleString = CSLTokenizeString2(pszStyle, ";",
                                          CSLT_HONOURSTRINGS
                                          | CSLT_PRESERVEQUOTES
                                          | CSLT_PRESERVEESCAPES );

    pszString = CSLGetField( papszStyleString, nPartId );
    
    if ( strlen(pszString) > 0 )
    {
        poStyleTool = CreateStyleToolFromStyleString(pszString);
        if ( poStyleTool )
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
 * @param hSM handle to the style manager.
 * @param nPartId the part number (0-based index).
 * @param pszStyleString (optional) the style string on which to operate.
 * If NULL then the current style string stored in the style manager is used.
 *
 * @return OGRStyleToolH of the requested part (style tools) or NULL on error.
 */ 

OGRStyleToolH OGR_SM_GetPart(OGRStyleMgrH hSM, int nPartId, 
                             const char *pszStyleString)

{
    VALIDATE_POINTER1( hSM, "OGR_SM_InitStyleString", NULL );

    return (OGRStyleToolH) ((OGRStyleMgr *) hSM)->GetPart(nPartId, pszStyleString);
}


/****************************************************************************/
/* OGRStyleTool *CreateStyleToolFromStyleString(const char *pszStyleString) */
/*                                                                          */
/* create a Style tool from the gived StyleString, it should contain only a */
/* part of a StyleString                                                    */
/****************************************************************************/
OGRStyleTool *OGRStyleMgr::CreateStyleToolFromStyleString(const char *
                                                          pszStyleString)
{
    char **papszToken = CSLTokenizeString2(pszStyleString,"();",
                                           CSLT_HONOURSTRINGS
                                           | CSLT_PRESERVEQUOTES
                                           | CSLT_PRESERVEESCAPES );
    OGRStyleTool   *poStyleTool;
        
    if (CSLCount(papszToken) <2)
        poStyleTool = NULL;
    else if (EQUAL(papszToken[0],"PEN"))
        poStyleTool = new OGRStylePen();
    else if (EQUAL(papszToken[0],"BRUSH"))
        poStyleTool = new OGRStyleBrush();
    else if (EQUAL(papszToken[0],"SYMBOL"))
        poStyleTool = new OGRStyleSymbol();
    else if (EQUAL(papszToken[0],"LABEL"))
        poStyleTool = new OGRStyleLabel();
    else 
        poStyleTool = NULL;

    CSLDestroy( papszToken );

    return poStyleTool;
}

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
    m_papszStyleTable = NULL;
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
    return (OGRStyleTableH) new OGRStyleTable( );
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
    if (m_papszStyleTable)
      CSLDestroy(m_papszStyleTable);
    m_papszStyleTable = NULL;
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
    delete (OGRStyleTable *) hSTBL;
}

/****************************************************************************/
/*    const char *OGRStyleTable::GetStyleName(const char *pszStyleString)   */
/*                                                                          */
/*    return the Name of a gived stylestring otherwise NULL                 */
/****************************************************************************/

/**
 * \brief Get style name by style string.
 *
 * @param pszStyleString the style string to look up.
 *
 * @return the Name of the matching style string or NULL on error.
 */

const char *OGRStyleTable::GetStyleName(const char *pszStyleString)
{
    int i;
    const char *pszStyleStringBegin;

    for (i=0;i<CSLCount(m_papszStyleTable);i++)
    {
        pszStyleStringBegin = strstr(m_papszStyleTable[i],":");

        if (pszStyleStringBegin && EQUAL(&pszStyleStringBegin[1],
                                         pszStyleString))
        {
            int nColon;

            osLastRequestedStyleName = m_papszStyleTable[i];
            nColon = osLastRequestedStyleName.find( ':' );
            if( nColon != -1 )
                osLastRequestedStyleName = 
                    osLastRequestedStyleName.substr(0,nColon);

            return osLastRequestedStyleName;
        }
    }
        
    return NULL;
}

/****************************************************************************/
/*            GBool OGRStyleTable::AddStyle(char *pszName,                  */
/*                                          char *pszStyleString)           */
/*                                                                          */
/*   Add a new style in the table, no comparison will be done on the       */
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

GBool OGRStyleTable::AddStyle(const char *pszName, const char *pszStyleString)
{
    int nPos;
    
    if (pszName && pszStyleString)
    {
        if ((nPos = IsExist(pszName)) != -1)
          return FALSE;

        m_papszStyleTable = CSLAddString(m_papszStyleTable,
                              CPLString().Printf("%s:%s",pszName,pszStyleString));
        return TRUE;
    }
    return FALSE;
}

/****************************************************************************/
/*            GBool OGRStyleTable::RemoveStyle(char *pszName)               */
/*                                                                          */
/*    Remove the gived style in the table based on the name, return TRUE    */
/*    on success otherwise FALSE                                            */
/****************************************************************************/

/**
 * \brief Remove a style in the table by its name.
 *
 * @param pszName the name of the style to remove.
 *
 * @return TRUE on success, FALSE on error
 */

GBool OGRStyleTable::RemoveStyle(const char *pszName)
{
    int nPos;
    if ((nPos = IsExist(pszName)) != -1)
    {
        m_papszStyleTable = CSLRemoveStrings(m_papszStyleTable,nPos,1,NULL);
        return TRUE;
    }
    return FALSE;
}

/****************************************************************************/
/*            GBool OGRStyleTable::ModifyStyle(char *pszName,               */
/*                                             char *pszStyleString)        */
/*                                                                          */
/*    Modify the gived style, if the style doesn't exist, it will be added  */
/*    return TRUE on success otherwise return FALSE                         */
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

GBool OGRStyleTable::ModifyStyle(const char *pszName, 
                                 const char * pszStyleString)
{
    if (pszName == NULL || pszStyleString == NULL)
      return FALSE;

    RemoveStyle(pszName);
    return AddStyle(pszName, pszStyleString);

}    

/****************************************************************************/
/*            GBool OGRStyleTable::SaveStyleTable(char *)                   */
/*                                                                          */
/*    Save the StyleTable in the gived file, return TRUE on success         */
/*    otherwise return FALSE                                                */
/****************************************************************************/

/**
 * \brief Save a style table to a file.
 *
 * @param pszFilename the name of the file to save to.
 *
 * @return TRUE on success, FALSE on error
 */

GBool OGRStyleTable::SaveStyleTable(const char *pszFilename)
{
    if (pszFilename == NULL)
      return FALSE;

    if (CSLSave(m_papszStyleTable,pszFilename) == 0)
      return FALSE;
    else
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
    
    return ((OGRStyleTable *) hStyleTable)->SaveStyleTable( pszFilename );
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

GBool OGRStyleTable::LoadStyleTable(const char *pszFilename)
{
    if (pszFilename == NULL)
      return FALSE;

    CSLDestroy(m_papszStyleTable);
        
    m_papszStyleTable = CSLLoad(pszFilename);
   
    if (m_papszStyleTable == NULL)
      return FALSE;
    else
      return TRUE;
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
    
    return ((OGRStyleTable *) hStyleTable)->LoadStyleTable( pszFilename );
}

/****************************************************************************/
/*             const char *OGRStyleTable::Find(const char *pszName)         */
/*                                                                          */
/*             return the StyleString based on the gived name,              */
/*             otherwise return NULL                                        */
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
    const char *pszDash = NULL;
    const char *pszOutput = NULL;

    int nPos;
    if ((nPos = IsExist(pszName)) != -1)
    {

        pszOutput = CSLGetField(m_papszStyleTable,nPos);
         
        pszDash = strstr(pszOutput,":");
        
        if (pszDash)
          return &pszDash[1];
    }
    return NULL;
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
    VALIDATE_POINTER1( hStyleTable, "OGR_STBL_Find", FALSE );
    VALIDATE_POINTER1( pszName, "OGR_STBL_Find", FALSE );
    
    return ((OGRStyleTable *) hStyleTable)->Find( pszName );
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

void OGRStyleTable::Print(FILE *fpOut)
{
    
    VSIFPrintf(fpOut,"#OFS-Version: 1.0\n");
    VSIFPrintf(fpOut,"#StyleField: style\n");
    if (m_papszStyleTable)
    {
        CSLPrint(m_papszStyleTable,fpOut);
    }
}

/****************************************************************************/
/*             int OGRStyleTable::IsExist(const char *pszName)            */
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

int OGRStyleTable::IsExist(const char *pszName)
{
    int i;
    int nCount;
    const char *pszNewString;

    if (pszName == NULL)
      return -1;

    nCount = CSLCount(m_papszStyleTable);
    pszNewString = CPLSPrintf("%s:",pszName);

    for (i=0;i<nCount;i++)
    {
        if (strstr(m_papszStyleTable[i],pszNewString) != NULL)
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
 * The newly created style table is owned by the caller, and will have it's
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

    ((OGRStyleTable *) hStyleTable)->ResetStyleStringReading();
}

/************************************************************************/
/*                           GetNextStyle()                             */
/************************************************************************/

const char *OGRStyleTable::GetNextStyle()
{
    const char *pszDash = NULL;
    const char *pszOutput = NULL;

    while( iNextStyle < CSLCount(m_papszStyleTable) )
    {

        if ( NULL == (pszOutput = CSLGetField(m_papszStyleTable,iNextStyle++)))
            continue;

        pszDash = strstr(pszOutput,":");

        int nColon;

        osLastRequestedStyleName = pszOutput;
        nColon = osLastRequestedStyleName.find( ':' );
        if( nColon != -1 )
            osLastRequestedStyleName = 
                osLastRequestedStyleName.substr(0,nColon);

        if (pszDash)
            return pszDash + 1;
    }
    return NULL;
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
    VALIDATE_POINTER1( hStyleTable, "OGR_STBL_GetNextStyle", NULL );
    
    return ((OGRStyleTable *) hStyleTable)->GetNextStyle();
}

/************************************************************************/
/*                           GetLastStyleName()                         */
/************************************************************************/

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
    VALIDATE_POINTER1( hStyleTable, "OGR_STBL_GetLastStyleName", NULL );
    
    return ((OGRStyleTable *) hStyleTable)->GetLastStyleName();
}


/****************************************************************************/
/*                          OGRStyleTool::OGRStyleTool()                    */
/*                                                                          */
/****************************************************************************/
OGRStyleTool::OGRStyleTool(OGRSTClassId eClassId)
{
    m_eClassId = eClassId; 
    m_dfScale = 1.0; 
    m_eUnit = OGRSTUMM;
    m_pszStyleString = NULL;
    m_bModified = FALSE;
    m_bParsed = FALSE;
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
        return (OGRStyleToolH) new OGRStylePen();
      case OGRSTCBrush:
        return (OGRStyleToolH) new OGRStyleBrush();
      case OGRSTCSymbol:
        return (OGRStyleToolH) new OGRStyleSymbol();
      case OGRSTCLabel:
        return (OGRStyleToolH) new OGRStyleLabel();
      default:
        return NULL;
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
    delete (OGRStyleTool *) hST; 
}


/****************************************************************************/
/*      void OGRStyleTool::SetStyleString(const char *pszStyleString)       */
/*                                                                          */
/****************************************************************************/
void OGRStyleTool::SetStyleString(const char *pszStyleString)
{
    m_pszStyleString = CPLStrdup(pszStyleString);
}

/****************************************************************************/
/*const char *OGRStyleTool::GetStyleString( OGRStyleParamId *pasStyleParam ,*/
/*                          OGRStyleValue *pasStyleValue, int nSize)        */
/*                                                                          */
/****************************************************************************/
const char *OGRStyleTool::GetStyleString(const OGRStyleParamId *pasStyleParam,
                                         OGRStyleValue *pasStyleValue,
                                         int nSize)
{
    if (IsStyleModified())
    {
        int i;
        GBool bFound;
        const char *pszClass;
        // FIXME: we should use CPLString instead of static buffer:
        char szCurrent[8192];
        szCurrent[0] = '\0';
    
        CPLFree(m_pszStyleString);
        
        switch (GetType())
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

        strcat(szCurrent,pszClass);

        bFound = FALSE;
        for (i=0;i< nSize;i++)
        {
            if (pasStyleValue[i].bValid == FALSE)
              continue;

            if (bFound)
              strcat(szCurrent,",");
            bFound = TRUE;
            
            strcat(szCurrent,pasStyleParam[i].pszToken);
            switch (pasStyleParam[i].eType)
            {
              case OGRSTypeString:
                strcat(szCurrent,":");
                strcat(szCurrent,pasStyleValue[i].pszValue);
                break;
              case OGRSTypeDouble:
                strcat(szCurrent,CPLString().Printf(":%f",pasStyleValue[i].dfValue));
                break;
              case OGRSTypeInteger:
                strcat(szCurrent,CPLString().Printf(":%d",pasStyleValue[i].nValue));
                break;
              default:
                break;
            }       
            if (pasStyleParam[i].bGeoref)
              switch (pasStyleValue[i].eUnit)
              {
                case OGRSTUGround:
                  strcat(szCurrent,"g");
                  break;
                case OGRSTUPixel:
                  strcat(szCurrent,"px");
                  break;
                case OGRSTUPoints:
                  strcat(szCurrent,"pt");
                  break;
                case OGRSTUCM:
                  strcat(szCurrent,"cm");
                  break;
                case OGRSTUInches:
                  strcat(szCurrent,"in");
                  break;
                case OGRSTUMM:
                  //strcat(szCurrent,"mm");
                default:
                  break;    //imp
              }
        }
        strcat(szCurrent,")");

        m_pszStyleString = CPLStrdup(szCurrent);

        m_bModified = FALSE;
    }
    
    return m_pszStyleString;
}   

/************************************************************************/
/*                          GetRGBFromString()                          */
/************************************************************************/

GBool OGRStyleTool::GetRGBFromString(const char *pszColor, int &nRed, 
                                     int &nGreen ,int & nBlue, 
                                     int &nTransparance)
{
   int nCount=0;
   
   nTransparance = 255;

   // FIXME: should we really use sscanf here?
   if (pszColor)
       nCount  = sscanf(pszColor,"#%2x%2x%2x%2x",&nRed,&nGreen,&nBlue, 
                        &nTransparance);
   
   if (nCount >=3)
     return TRUE;
   else
     return FALSE;
}

/************************************************************************/
/*                           GetSpecificId()                            */
/*                                                                      */
/*      return -1, if the wanted type is not found, ex:                 */
/*      if you want ogr-pen value, pszWanted should be ogr-pen(case     */
/*      sensitive)                                                      */
/************************************************************************/

int OGRStyleTool::GetSpecificId(const char *pszId, const char *pszWanted)
{
    const char *pszRealWanted = pszWanted;
    const char *pszFound;
    int nValue  = -1;

    if (pszWanted == NULL || strlen(pszWanted) == 0)
      pszRealWanted = "ogr-pen";

    if (pszId == NULL)
      return -1;
    
    if ((pszFound = strstr(pszId, pszRealWanted)) != NULL)
    {
        // We found the string, it could be no value after it, use default one
        nValue = 0;
        
        if (pszFound[strlen(pszRealWanted)] == '-' )
          nValue =atoi(&pszFound[strlen(pszRealWanted)+1]);
    }
    
    return nValue;

}

/************************************************************************/
/*                              GetType()                               */
/************************************************************************/
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
    return ((OGRStyleTool *) hST)->GetType();
}


/************************************************************************/
/*                           OGR_ST_GetUnit()                           */
/************************************************************************/
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
    return ((OGRStyleTool *) hST)->GetUnit();
}


/************************************************************************/
/*                              SetUnit()                               */
/************************************************************************/
void OGRStyleTool::SetUnit(OGRSTUnitId eUnit,double dfScale)
{
    m_dfScale = dfScale;
    m_eUnit = eUnit;
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
    ((OGRStyleTool *) hST)->SetUnit(eUnit, dfGroundPaperScale);
}

/************************************************************************/
/*                               Parse()                                */
/************************************************************************/
GBool OGRStyleTool::Parse(const OGRStyleParamId *pasStyle,
                          OGRStyleValue *pasValue,
                          int nCount)
{
    char **papszToken; // Token to contains StyleString Type and content
    char **papszToken2; // Token that will contains StyleString elements


    OGRSTUnitId  eLastUnit;
    
    if (IsStyleParsed() == TRUE)
      return TRUE;

    StyleParsed();

    if (m_pszStyleString == NULL)
      return FALSE;
    
    // Tokenize the String to get the Type and the content
    // Example: Type(elem1:val2,elem2:val2)
    papszToken  = CSLTokenizeString2(m_pszStyleString,"()",
                                     CSLT_HONOURSTRINGS
                                     | CSLT_PRESERVEQUOTES
                                     | CSLT_PRESERVEESCAPES );

    if (CSLCount(papszToken) > 2 || CSLCount(papszToken) == 0)
    {
        CSLDestroy( papszToken );
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "Error in the format of the StyleTool %s\n",m_pszStyleString);
        return FALSE;
    }
    
    // Tokenize the content of the StyleString to get paired components in it.
    papszToken2 = CSLTokenizeString2( papszToken[1], ",",
                                      CSLT_HONOURSTRINGS
                                      | CSLT_PRESERVEQUOTES
                                      | CSLT_PRESERVEESCAPES );
    
    // Valid that we have the right StyleString for this feature type.
    switch (GetType())
    {
      case OGRSTCPen:
        if (!EQUAL(papszToken[0],"PEN"))
        {
            CPLError(CE_Failure, CPLE_AppDefined, 
                     "Error in the Type of StyleTool %s should be a PEN Type\n",
                     papszToken[0]);
            CSLDestroy( papszToken );
            CSLDestroy( papszToken2 );
            return FALSE;
        }
        break;
      case OGRSTCBrush:
        if (!EQUAL(papszToken[0],"BRUSH"))
        {
            CPLError(CE_Failure, CPLE_AppDefined, 
                     "Error in the Type of StyleTool %s should be a BRUSH Type\n",
                     papszToken[0]);
            CSLDestroy( papszToken );
            CSLDestroy( papszToken2 );
            return FALSE;
        }
        break;
      case OGRSTCSymbol:
        if (!EQUAL(papszToken[0],"SYMBOL"))
        {
            CPLError(CE_Failure, CPLE_AppDefined, 
                     "Error in the Type of StyleTool %s should be a SYMBOL Type\n",
                     papszToken[0]);
            CSLDestroy( papszToken );
            CSLDestroy( papszToken2 );
            return FALSE;
        }
        break;
      case OGRSTCLabel:
        if (!EQUAL(papszToken[0],"LABEL"))
        {
            CPLError(CE_Failure, CPLE_AppDefined, 
                     "Error in the Type of StyleTool %s should be a LABEL Type\n",
                     papszToken[0]);
            CSLDestroy( papszToken );
            CSLDestroy( papszToken2 );
            return FALSE;
        }
        break;
      default:
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "Error in the Type of StyleTool, Type undetermined\n");
        CSLDestroy( papszToken );
        CSLDestroy( papszToken2 );
        return FALSE;
        break;
    }
    
    ////////////////////////////////////////////////////////////////////////
    // Here we will loop on each element in the StyleString. If it's 
    // a valid element, we will add it in the StyleTool with 
    // SetParamStr().
    //
    // It's important to note that the SetInternalUnit...() is use to update 
    // the unit of the StyleTool param (m_eUnit). 
    // See OGRStyleTool::SetParamStr().
    // There's a StyleTool unit (m_eUnit), which is the output unit, and each 
    // parameter of the style have its own unit value (the input unit). Here we
    // set m_eUnit to the input unit and in SetParamStr(), we will use this
    // value to set the input unit. Then after the loop we will reset m_eUnit 
    // to it's original value. (Yes it's a side effect / black magic)
    //
    // The pasStyle variable is a global variable passed in argument to the
    // function. See at the top of this file the four OGRStyleParamId 
    // variable. They are used to register the valid parameter of each 
    // StyleTool.
    ////////////////////////////////////////////////////////////////////////

    // Save Scale and output Units because the parsing code will alter 
    // the values
    eLastUnit = m_eUnit;
    double  dSavedScale = m_dfScale;
    int     i, nElements = CSLCount(papszToken2);

    for ( i = 0; i < nElements; i++ )
    {
        char    **papszStylePair =
            CSLTokenizeString2( papszToken2[i], ":", CSLT_HONOURSTRINGS
                                                     | CSLT_STRIPLEADSPACES
                                                     | CSLT_STRIPENDSPACES );
        int     j, nTokens = CSLCount(papszStylePair);

        if ( nTokens < 1 || nTokens > 2 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Error in the StyleTool String %s", m_pszStyleString );
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Malformed element #%d (\"%s\") skipped",
                      i, papszToken2[i] );
            CSLDestroy(papszStylePair);
            continue;
        }
        
        for ( j = 0; j < nCount; j++ )
        {
            if ( EQUAL(pasStyle[j].pszToken, papszStylePair[0]) )
            {
                if (nTokens == 2 && pasStyle[j].bGeoref == TRUE)
                    SetInternalInputUnitFromParam(papszStylePair[1]);

                // Set either the actual value of style parameter or "1"
                // for boolean parameters which do not have values.
                // "1" means that boolean parameter is present in the style
                // string.
                OGRStyleTool::SetParamStr( pasStyle[j], pasValue[j],
                                    (nTokens == 2) ? papszStylePair[1] : "1" );

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

/************************************************************************/
/*                   SetInternalInputUnitFromParam()                    */
/************************************************************************/

void OGRStyleTool::SetInternalInputUnitFromParam(char *pszString)
{

    char *pszUnit;

    if (pszString == NULL)
      return;
    pszUnit = strstr(pszString,"g");
    if (pszUnit)
    {
        SetUnit(OGRSTUGround);
        pszUnit[0]= '\0';
        return;
    }
    pszUnit = strstr(pszString,"px");
    if (pszUnit)
    {
        SetUnit(OGRSTUPixel);
        pszUnit[0]= '\0';
        return;
    }
    pszUnit = strstr(pszString,"pt");
    if (pszUnit)
    {
        SetUnit(OGRSTUPoints);
        pszUnit[0]= '\0';
        return;
    }
    pszUnit = strstr(pszString,"mm");
    if (pszUnit)
    {
        SetUnit(OGRSTUMM);
        pszUnit[0]= '\0';
        return;
    }
    pszUnit = strstr(pszString,"cm");
    if (pszUnit)
    {
        SetUnit(OGRSTUCM);
        pszUnit[0]= '\0';
        return;
    }
    pszUnit = strstr(pszString,"in");
    if (pszUnit)
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
double OGRStyleTool::ComputeWithUnit(double dfValue, OGRSTUnitId eInputUnit)
{
    OGRSTUnitId eOutputUnit = GetUnit();

    double dfNewValue = dfValue;        // dfValue in  Meter;


    if (eOutputUnit == eInputUnit)
      return dfValue;

    switch (eInputUnit)
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
        break;    //imp
    }

    switch (eOutputUnit)
    {
      case OGRSTUGround:
        dfNewValue *= m_dfScale;
        break;
      case OGRSTUPixel:
        dfNewValue *= (72.0 * 39.37);
        break;
      case OGRSTUPoints:
        dfNewValue *= (72.0 * 39.37);
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
        break;  // imp
    }
    return dfNewValue;
}

/************************************************************************/
/*                          ComputeWithUnit()                           */
/************************************************************************/
int   OGRStyleTool::ComputeWithUnit(int nValue, OGRSTUnitId eUnit)
{
    return (int) ComputeWithUnit((double )nValue, eUnit);
}

/************************************************************************/
/*                            GetParamStr()                             */
/************************************************************************/
const char *OGRStyleTool::GetParamStr(const OGRStyleParamId &sStyleParam ,
                                      OGRStyleValue &sStyleValue,
                                      GBool &bValueIsNull)
{

    if (!Parse())
    {
        bValueIsNull = TRUE;
        return NULL;
    }

    bValueIsNull = !sStyleValue.bValid;
    
    if (bValueIsNull == TRUE)
      return NULL;

    switch (sStyleParam.eType)
    {
      
        // if sStyleParam.bGeoref == TRUE , need to convert to output value;
      case OGRSTypeString:
        return sStyleValue.pszValue;
      case OGRSTypeDouble:
        if (sStyleParam.bGeoref)
          return CPLSPrintf("%f",ComputeWithUnit(sStyleValue.dfValue,
                                                 sStyleValue.eUnit));
        else
          return CPLSPrintf("%f",sStyleValue.dfValue);
                            
      case OGRSTypeInteger:
        if (sStyleParam.bGeoref)
          return CPLSPrintf("%d",ComputeWithUnit(sStyleValue.nValue,
                                                 sStyleValue.eUnit));
        else
          return CPLSPrintf("%d",sStyleValue.nValue);
      case OGRSTypeBoolean:
        return CPLSPrintf("%d",sStyleValue.nValue);
      default:
        bValueIsNull = TRUE;
        return NULL;
    }
}

/****************************************************************************/
/*    int OGRStyleTool::GetParamNum(OGRStyleParamId sStyleParam ,           */
/*                               OGRStyleValue sStyleValue,                 */
/*                               GBool &bValueIsNull)                       */
/*                                                                          */
/****************************************************************************/
int OGRStyleTool::GetParamNum(const OGRStyleParamId &sStyleParam ,
                              OGRStyleValue &sStyleValue,
                              GBool &bValueIsNull)
{
    return (int)GetParamDbl(sStyleParam,sStyleValue,bValueIsNull);
}

/****************************************************************************/
/*       double OGRStyleTool::GetParamDbl(OGRStyleParamId sStyleParam ,     */
/*                               OGRStyleValue sStyleValue,                 */
/*                               GBool &bValueIsNull)                       */
/*                                                                          */
/****************************************************************************/
double OGRStyleTool::GetParamDbl(const OGRStyleParamId &sStyleParam ,
                                 OGRStyleValue &sStyleValue,
                                 GBool &bValueIsNull)
{
    if (!Parse())
    {
        bValueIsNull = TRUE;
        return 0;
    }

    bValueIsNull = !sStyleValue.bValid;
    
    if (bValueIsNull == TRUE)
      return 0;

    switch (sStyleParam.eType)
    {
      
        // if sStyleParam.bGeoref == TRUE , need to convert to output value;
      case OGRSTypeString:
        if (sStyleParam.bGeoref)
          return ComputeWithUnit(atof(sStyleValue.pszValue),
                                 sStyleValue.eUnit);
        else
          return atof(sStyleValue.pszValue);
      case OGRSTypeDouble:
        if (sStyleParam.bGeoref)
          return ComputeWithUnit(sStyleValue.dfValue,
                                      sStyleValue.eUnit);
        else
          return sStyleValue.dfValue;
      case OGRSTypeInteger:
        if (sStyleParam.bGeoref)
          return (double)ComputeWithUnit(sStyleValue.nValue,
                                         sStyleValue.eUnit);
        else    
          return (double)sStyleValue.nValue;
      case OGRSTypeBoolean:
        return (double)sStyleValue.nValue;
      default:
        bValueIsNull = TRUE;
        return 0;
    }
}

/****************************************************************************/
/*      void OGRStyleTool::SetParamStr(OGRStyleParamId &sStyleParam ,       */
/*                             OGRStyleValue &sStyleValue,                  */
/*                             const char *pszParamString)                  */
/*                                                                          */
/****************************************************************************/
void OGRStyleTool::SetParamStr(const OGRStyleParamId &sStyleParam ,
                               OGRStyleValue &sStyleValue,
                               const char *pszParamString)
{
    Parse();
    StyleModified();
    sStyleValue.bValid = TRUE;
    sStyleValue.eUnit = GetUnit();
    switch (sStyleParam.eType)
    {
      
        // if sStyleParam.bGeoref == TRUE , need to convert to output value;
      case OGRSTypeString:
        sStyleValue.pszValue = CPLStrdup(pszParamString);
        break;
      case OGRSTypeDouble:
        sStyleValue.dfValue = atof(pszParamString);
        break;
      case OGRSTypeInteger:
      case OGRSTypeBoolean:
        sStyleValue.nValue  = atoi(pszParamString);
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
void OGRStyleTool::SetParamNum(const OGRStyleParamId &sStyleParam ,
                               OGRStyleValue &sStyleValue,
                               int nParam)
{
    Parse();
    StyleModified();
    sStyleValue.bValid = TRUE;
    sStyleValue.eUnit = GetUnit();
    switch (sStyleParam.eType)
    {
        
        // if sStyleParam.bGeoref == TRUE , need to convert to output value;
      case OGRSTypeString:
        sStyleValue.pszValue = CPLStrdup(CPLString().Printf("%d",nParam));
        break;
      case OGRSTypeDouble:
        sStyleValue.dfValue = (double)nParam;
        break;
      case OGRSTypeInteger:
      case OGRSTypeBoolean:
        sStyleValue.nValue  = nParam;
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
void OGRStyleTool::SetParamDbl(const OGRStyleParamId &sStyleParam ,
                               OGRStyleValue &sStyleValue,
                               double dfParam)
{ 
    Parse();
    StyleModified();
    sStyleValue.bValid = TRUE;
    sStyleValue.eUnit = GetUnit();
    switch (sStyleParam.eType)
    {
        
        // if sStyleParam.bGeoref == TRUE , need to convert to output value;
      case OGRSTypeString:
        sStyleValue.pszValue = CPLStrdup(CPLString().Printf("%f",dfParam));
        break;
      case OGRSTypeDouble:
        sStyleValue.dfValue = dfParam;
        break;
      case OGRSTypeInteger:
      case OGRSTypeBoolean:
        sStyleValue.nValue  = (int)dfParam;
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

const char *OGR_ST_GetParamStr( OGRStyleToolH hST, int eParam, int *bValueIsNull )
{
    GBool bIsNull = TRUE;
    const char *pszVal = "";

    VALIDATE_POINTER1( hST, "OGR_ST_GetParamStr", "" );
    VALIDATE_POINTER1( bValueIsNull, "OGR_ST_GetParamStr", "" );

    switch( ((OGRStyleTool *) hST)->GetType() )
    {
      case OGRSTCPen:
        pszVal = ((OGRStylePen *) hST)->GetParamStr((OGRSTPenParam)eParam, 
                                                    bIsNull);
        break;
      case OGRSTCBrush:
        pszVal = ((OGRStyleBrush *) hST)->GetParamStr((OGRSTBrushParam)eParam, 
                                                      bIsNull);
        break;
      case OGRSTCSymbol:
        pszVal = ((OGRStyleSymbol *) hST)->GetParamStr((OGRSTSymbolParam)eParam,
                                                       bIsNull);
        break;
      case OGRSTCLabel:
        pszVal = ((OGRStyleLabel *) hST)->GetParamStr((OGRSTLabelParam)eParam, 
                                                      bIsNull);
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
    GBool bIsNull = TRUE;
    int nVal = 0;

    VALIDATE_POINTER1( hST, "OGR_ST_GetParamNum", 0 );
    VALIDATE_POINTER1( bValueIsNull, "OGR_ST_GetParamNum", 0 );

    switch( ((OGRStyleTool *) hST)->GetType() )
    {
      case OGRSTCPen:
        nVal = ((OGRStylePen *) hST)->GetParamNum((OGRSTPenParam)eParam, 
                                                    bIsNull);
        break;
      case OGRSTCBrush:
        nVal = ((OGRStyleBrush *) hST)->GetParamNum((OGRSTBrushParam)eParam, 
                                                      bIsNull);
        break;
      case OGRSTCSymbol:
        nVal = ((OGRStyleSymbol *) hST)->GetParamNum((OGRSTSymbolParam)eParam,
                                                       bIsNull);
        break;
      case OGRSTCLabel:
        nVal = ((OGRStyleLabel *) hST)->GetParamNum((OGRSTLabelParam)eParam, 
                                                      bIsNull);
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
    GBool bIsNull = TRUE;
    double dfVal = 0.0;

    VALIDATE_POINTER1( hST, "OGR_ST_GetParamDbl", 0.0 );
    VALIDATE_POINTER1( bValueIsNull, "OGR_ST_GetParamDbl", 0.0 );

    switch( ((OGRStyleTool *) hST)->GetType() )
    {
      case OGRSTCPen:
        dfVal = ((OGRStylePen *) hST)->GetParamDbl((OGRSTPenParam)eParam, 
                                                  bIsNull);
        break;
      case OGRSTCBrush:
        dfVal = ((OGRStyleBrush *) hST)->GetParamDbl((OGRSTBrushParam)eParam, 
                                                    bIsNull);
        break;
      case OGRSTCSymbol:
        dfVal = ((OGRStyleSymbol *) hST)->GetParamDbl((OGRSTSymbolParam)eParam,
                                                     bIsNull);
        break;
      case OGRSTCLabel:
        dfVal = ((OGRStyleLabel *) hST)->GetParamDbl((OGRSTLabelParam)eParam, 
                                                    bIsNull);
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

    switch( ((OGRStyleTool *) hST)->GetType() )
    {
      case OGRSTCPen:
        ((OGRStylePen *) hST)->SetParamStr((OGRSTPenParam)eParam, 
                                           pszValue);
        break;
      case OGRSTCBrush:
        ((OGRStyleBrush *) hST)->SetParamStr((OGRSTBrushParam)eParam, 
                                             pszValue);
        break;
      case OGRSTCSymbol:
        ((OGRStyleSymbol *) hST)->SetParamStr((OGRSTSymbolParam)eParam,
                                              pszValue);
        break;
      case OGRSTCLabel:
        ((OGRStyleLabel *) hST)->SetParamStr((OGRSTLabelParam)eParam, 
                                             pszValue);
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

    switch( ((OGRStyleTool *) hST)->GetType() )
    {
      case OGRSTCPen:
        ((OGRStylePen *) hST)->SetParamNum((OGRSTPenParam)eParam, 
                                           nValue);
        break;
      case OGRSTCBrush:
        ((OGRStyleBrush *) hST)->SetParamNum((OGRSTBrushParam)eParam, 
                                             nValue);
        break;
      case OGRSTCSymbol:
        ((OGRStyleSymbol *) hST)->SetParamNum((OGRSTSymbolParam)eParam,
                                              nValue);
        break;
      case OGRSTCLabel:
        ((OGRStyleLabel *) hST)->SetParamNum((OGRSTLabelParam)eParam, 
                                             nValue);
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

    switch( ((OGRStyleTool *) hST)->GetType() )
    {
      case OGRSTCPen:
        ((OGRStylePen *) hST)->SetParamDbl((OGRSTPenParam)eParam, 
                                           dfValue);
        break;
      case OGRSTCBrush:
        ((OGRStyleBrush *) hST)->SetParamDbl((OGRSTBrushParam)eParam, 
                                             dfValue);
        break;
      case OGRSTCSymbol:
        ((OGRStyleSymbol *) hST)->SetParamDbl((OGRSTSymbolParam)eParam,
                                              dfValue);
        break;
      case OGRSTCLabel:
        ((OGRStyleLabel *) hST)->SetParamDbl((OGRSTLabelParam)eParam, 
                                             dfValue);
        break;
      default:
        break;
    }
}


/************************************************************************/
/*                           OGR_ST_GetStyleString()                    */
/************************************************************************/
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

    switch( ((OGRStyleTool *) hST)->GetType() )
    {
      case OGRSTCPen:
        pszVal = ((OGRStylePen *) hST)->GetStyleString();
        break;
      case OGRSTCBrush:
        pszVal = ((OGRStyleBrush *) hST)->GetStyleString();
        break;
      case OGRSTCSymbol:
        pszVal = ((OGRStyleSymbol *) hST)->GetStyleString();
        break;
      case OGRSTCLabel:
        pszVal = ((OGRStyleLabel *) hST)->GetStyleString();
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
 * \brief Return the r,g,b,a components of a color encoded in \#RRGGBB[AA] format
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
 * @return TRUE if the color could be succesfully parsed, or FALSE in case of
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

    return ((OGRStyleTool *) hST)->GetRGBFromString(pszColor, *pnRed, *pnGreen,
                                                    *pnBlue, *pnAlpha );
}


/* ======================================================================== */
/*                OGRStylePen                                               */
/*       Specific parameter (Set/Get) for the StylePen                      */
/* ======================================================================== */


/****************************************************************************/
/*                      OGRStylePen::OGRStylePen()                          */
/*                                                                          */
/****************************************************************************/
OGRStylePen::OGRStylePen() : OGRStyleTool(OGRSTCPen)
{
    m_pasStyleValue = (OGRStyleValue *)CPLCalloc(OGRSTPenLast, 
                                                 sizeof(OGRStyleValue));
}




/****************************************************************************/
/*                      OGRStylePen::~OGRStylePen()                         */
/*                                                                          */
/****************************************************************************/
OGRStylePen::~OGRStylePen()
{
    for (int i = 0; i < OGRSTPenLast; i++)
    {
        if (m_pasStyleValue[i].pszValue != NULL)
        {
            CPLFree(m_pasStyleValue[i].pszValue);
            m_pasStyleValue[i].pszValue = NULL;
        }
    }

    CPLFree(m_pasStyleValue);
}

/************************************************************************/
/*                         OGRStylePen::Parse()                         */
/************************************************************************/
GBool OGRStylePen::Parse()

{ 
    return OGRStyleTool::Parse(asStylePen,m_pasStyleValue,(int)OGRSTPenLast);
}

/************************************************************************/
/*                            GetParamStr()                             */
/************************************************************************/
const char *OGRStylePen::GetParamStr(OGRSTPenParam eParam, GBool &bValueIsNull)
{   
    return OGRStyleTool::GetParamStr(asStylePen[eParam],
                                     m_pasStyleValue[eParam],
                                     bValueIsNull);
}

/************************************************************************/
/*                            GetParamNum()                             */
/************************************************************************/
int OGRStylePen::GetParamNum(OGRSTPenParam eParam,GBool &bValueIsNull)
{  
    return OGRStyleTool::GetParamNum(asStylePen[eParam],
                                     m_pasStyleValue[eParam],bValueIsNull);
}

/************************************************************************/
/*                            GetParamDbl()                             */
/************************************************************************/
double OGRStylePen::GetParamDbl(OGRSTPenParam eParam,GBool &bValueIsNull)
{  
    return OGRStyleTool::GetParamDbl(asStylePen[eParam],
                                     m_pasStyleValue[eParam],bValueIsNull);
}

/************************************************************************/
/*                            SetParamStr()                             */
/************************************************************************/

void OGRStylePen::SetParamStr(OGRSTPenParam eParam, const char *pszParamString)
{   
    OGRStyleTool::SetParamStr(asStylePen[eParam],m_pasStyleValue[eParam],
                              pszParamString);
}

/************************************************************************/
/*                            SetParamNum()                             */
/************************************************************************/
void OGRStylePen::SetParamNum(OGRSTPenParam eParam, int nParam)
{  
    OGRStyleTool::SetParamNum(asStylePen[eParam],
                              m_pasStyleValue[eParam],nParam);
}
    
/************************************************************************/
/*                            SetParamDbl()                             */
/************************************************************************/
void OGRStylePen::SetParamDbl(OGRSTPenParam eParam, double dfParam)
{   
    OGRStyleTool::SetParamDbl(asStylePen[eParam],
                              m_pasStyleValue[eParam],dfParam);
}

/************************************************************************/
/*                           GetStyleString()                           */
/************************************************************************/
const char *OGRStylePen::GetStyleString()
{   
    return OGRStyleTool::GetStyleString(asStylePen,m_pasStyleValue,
                                        (int)OGRSTPenLast);
}

/****************************************************************************/
/*                      OGRStyleBrush::OGRStyleBrush()                      */
/*                                                                          */
/****************************************************************************/
OGRStyleBrush::OGRStyleBrush() : OGRStyleTool(OGRSTCBrush)
{
    m_pasStyleValue = (OGRStyleValue *)CPLCalloc(OGRSTBrushLast, 
                                                 sizeof(OGRStyleValue));
}

/****************************************************************************/
/*                      OGRStyleBrush::~OGRStyleBrush()                     */
/*                                                                          */
/****************************************************************************/
OGRStyleBrush::~OGRStyleBrush() 
{
    for (int i = 0; i < OGRSTBrushLast; i++)
    {
        if (m_pasStyleValue[i].pszValue != NULL)
        {
            CPLFree(m_pasStyleValue[i].pszValue);
            m_pasStyleValue[i].pszValue = NULL;
        }
    }

    CPLFree(m_pasStyleValue);
}

/************************************************************************/
/*                               Parse()                                */
/************************************************************************/
GBool OGRStyleBrush::Parse()
{ 
    return OGRStyleTool::Parse(asStyleBrush,m_pasStyleValue,
                               (int)OGRSTBrushLast);
}

/************************************************************************/
/*                            GetParamStr()                             */
/************************************************************************/
const char *OGRStyleBrush::GetParamStr(OGRSTBrushParam eParam, GBool &bValueIsNull)
{  
    return OGRStyleTool::GetParamStr(asStyleBrush[eParam],
                                     m_pasStyleValue[eParam],
                                     bValueIsNull);
}

/************************************************************************/
/*                            GetParamNum()                             */
/************************************************************************/
int OGRStyleBrush::GetParamNum(OGRSTBrushParam eParam,GBool &bValueIsNull)
{  
    return OGRStyleTool::GetParamNum(asStyleBrush[eParam],
                                     m_pasStyleValue[eParam],bValueIsNull);
}

/************************************************************************/
/*                            GetParamDbl()                             */
/************************************************************************/
double OGRStyleBrush::GetParamDbl(OGRSTBrushParam eParam,GBool &bValueIsNull)
{  
    return OGRStyleTool::GetParamDbl(asStyleBrush[eParam],
                                     m_pasStyleValue[eParam],bValueIsNull);
}

/************************************************************************/
/*                            SetParamStr()                             */
/************************************************************************/
void OGRStyleBrush::SetParamStr(OGRSTBrushParam eParam, const char *pszParamString)
{   
    OGRStyleTool::SetParamStr(asStyleBrush[eParam],m_pasStyleValue[eParam],
                              pszParamString);
}

/************************************************************************/
/*                            SetParamNum()                             */
/************************************************************************/
void OGRStyleBrush::SetParamNum(OGRSTBrushParam eParam, int nParam)
{  
    OGRStyleTool::SetParamNum(asStyleBrush[eParam],
                              m_pasStyleValue[eParam],nParam);
}
    
/************************************************************************/
/*                            SetParamDbl()                             */
/************************************************************************/
void OGRStyleBrush::SetParamDbl(OGRSTBrushParam eParam, double dfParam)
{   
    OGRStyleTool::SetParamDbl(asStyleBrush[eParam],
                              m_pasStyleValue[eParam],dfParam);
}

/************************************************************************/
/*                           GetStyleString()                           */
/************************************************************************/
const char *OGRStyleBrush::GetStyleString()
{   
    return OGRStyleTool::GetStyleString(asStyleBrush,m_pasStyleValue,
                                        (int)OGRSTBrushLast);
}

/****************************************************************************/
/*                      OGRStyleSymbol::OGRStyleSymbol()                    */
/****************************************************************************/
OGRStyleSymbol::OGRStyleSymbol() : OGRStyleTool(OGRSTCSymbol)
{
    m_pasStyleValue = (OGRStyleValue *)CPLCalloc(OGRSTSymbolLast, 
                                                 sizeof(OGRStyleValue));
}

/****************************************************************************/
/*                      OGRStyleSymbol::~OGRStyleSymbol()                   */
/*                                                                          */
/****************************************************************************/
OGRStyleSymbol::~OGRStyleSymbol()
{
    for (int i = 0; i < OGRSTSymbolLast; i++)
    {
        if (m_pasStyleValue[i].pszValue != NULL)
        {
            CPLFree(m_pasStyleValue[i].pszValue);
            m_pasStyleValue[i].pszValue = NULL;
        }
    }

    CPLFree(m_pasStyleValue);
}

/************************************************************************/
/*                               Parse()                                */
/************************************************************************/
GBool OGRStyleSymbol::Parse()
{ 
    return OGRStyleTool::Parse(asStyleSymbol,m_pasStyleValue,
                               (int)OGRSTSymbolLast);
}

/************************************************************************/
/*                            GetParamStr()                             */
/************************************************************************/
const char *OGRStyleSymbol::GetParamStr(OGRSTSymbolParam eParam, GBool &bValueIsNull)
{   return OGRStyleTool::GetParamStr(asStyleSymbol[eParam],
                                     m_pasStyleValue[eParam],
                                     bValueIsNull);
}
/************************************************************************/
/*                            GetParamNum()                             */
/************************************************************************/
int OGRStyleSymbol::GetParamNum(OGRSTSymbolParam eParam,GBool &bValueIsNull)
{  return OGRStyleTool::GetParamNum(asStyleSymbol[eParam],
                                    m_pasStyleValue[eParam],bValueIsNull);
}
/************************************************************************/
/*                            GetParamDbl()                             */
/************************************************************************/
double OGRStyleSymbol::GetParamDbl(OGRSTSymbolParam eParam,GBool &bValueIsNull)
{  return OGRStyleTool::GetParamDbl(asStyleSymbol[eParam],
                                    m_pasStyleValue[eParam],bValueIsNull);
}

/************************************************************************/
/*                            SetParamStr()                             */
/************************************************************************/
void OGRStyleSymbol::SetParamStr(OGRSTSymbolParam eParam, const char *pszParamString)
{   OGRStyleTool::SetParamStr(asStyleSymbol[eParam],m_pasStyleValue[eParam],
                              pszParamString);
}

/************************************************************************/
/*                            SetParamNum()                             */
/************************************************************************/
void OGRStyleSymbol::SetParamNum(OGRSTSymbolParam eParam, int nParam)
{  OGRStyleTool::SetParamNum(asStyleSymbol[eParam],
                             m_pasStyleValue[eParam],nParam);
}

/************************************************************************/
/*                            SetParamDbl()                             */
/************************************************************************/
void OGRStyleSymbol::SetParamDbl(OGRSTSymbolParam eParam, double dfParam)
        {   OGRStyleTool::SetParamDbl(asStyleSymbol[eParam],
                                      m_pasStyleValue[eParam],dfParam);
        }
/************************************************************************/
/*                           GetStyleString()                           */
/************************************************************************/
const char *OGRStyleSymbol::GetStyleString()
{   
    return OGRStyleTool::GetStyleString(asStyleSymbol,m_pasStyleValue,
                                        (int)OGRSTSymbolLast);
}


/****************************************************************************/
/*                      OGRStyleLabel::OGRStyleLabel()                      */
/*                                                                          */
/****************************************************************************/
OGRStyleLabel::OGRStyleLabel() : OGRStyleTool(OGRSTCLabel)
{
    m_pasStyleValue = (OGRStyleValue *)CPLCalloc(OGRSTLabelLast, 
                                                 sizeof(OGRStyleValue));
}

/****************************************************************************/
/*                      OGRStyleLabel::~OGRStyleLabel()                     */
/*                                                                          */
/****************************************************************************/
OGRStyleLabel::~OGRStyleLabel()
{
    for (int i = 0; i < OGRSTLabelLast; i++)
    {
        if (m_pasStyleValue[i].pszValue != NULL)
        {
            CPLFree(m_pasStyleValue[i].pszValue);
            m_pasStyleValue[i].pszValue = NULL;
        }
    }

    CPLFree(m_pasStyleValue);
}

/************************************************************************/
/*                               Parse()                                */
/************************************************************************/
GBool OGRStyleLabel::Parse()
{ return OGRStyleTool::Parse(asStyleLabel,m_pasStyleValue,
                             (int)OGRSTLabelLast);
}

/************************************************************************/
/*                            GetParamStr()                             */
/************************************************************************/
const char *OGRStyleLabel::GetParamStr(OGRSTLabelParam eParam, GBool &bValueIsNull)
{   return OGRStyleTool::GetParamStr(asStyleLabel[eParam],
                                     m_pasStyleValue[eParam],
                                     bValueIsNull);
}
/************************************************************************/
/*                            GetParamNum()                             */
/************************************************************************/
int OGRStyleLabel::GetParamNum(OGRSTLabelParam eParam,GBool &bValueIsNull)
{  return OGRStyleTool::GetParamNum(asStyleLabel[eParam],
                                    m_pasStyleValue[eParam],bValueIsNull);
}
/************************************************************************/
/*                            GetParamDbl()                             */
/************************************************************************/
double OGRStyleLabel::GetParamDbl(OGRSTLabelParam eParam,GBool &bValueIsNull)
{  return OGRStyleTool::GetParamDbl(asStyleLabel[eParam],
                                    m_pasStyleValue[eParam],bValueIsNull);
}
/************************************************************************/
/*                            SetParamStr()                             */
/************************************************************************/
void OGRStyleLabel::SetParamStr(OGRSTLabelParam eParam, const char *pszParamString)
{   OGRStyleTool::SetParamStr(asStyleLabel[eParam],m_pasStyleValue[eParam],
                              pszParamString);
}
/************************************************************************/
/*                            SetParamNum()                             */
/************************************************************************/
void OGRStyleLabel::SetParamNum(OGRSTLabelParam eParam, int nParam)
{  OGRStyleTool::SetParamNum(asStyleLabel[eParam],
                             m_pasStyleValue[eParam],nParam);
}

/************************************************************************/
/*                            SetParamDbl()                             */
/************************************************************************/
void OGRStyleLabel::SetParamDbl(OGRSTLabelParam eParam, double dfParam)
{   OGRStyleTool::SetParamDbl(asStyleLabel[eParam],
                              m_pasStyleValue[eParam],dfParam);
}
/************************************************************************/
/*                           GetStyleString()                           */
/************************************************************************/
const char *OGRStyleLabel::GetStyleString()
{   return OGRStyleTool::GetStyleString(asStyleLabel,m_pasStyleValue,
                                        (int)OGRSTLabelLast);
}

