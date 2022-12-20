/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference interface to OGC XML (014r4).
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam (warmerdam@pobox.com)
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_srs_api.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"

/************************************************************************/
/*                              parseURN()                              */
/*                                                                      */
/*      Parses requested sections out of URN.  The passed in URN        */
/*      *is* altered but the returned values point into the             */
/*      original string.                                                */
/************************************************************************/

static bool parseURN(char *pszURN, const char **ppszObjectType,
                     const char **ppszAuthority, const char **ppszCode,
                     const char **ppszVersion = nullptr)

{
    if (ppszObjectType != nullptr)
        *ppszObjectType = "";
    if (ppszAuthority != nullptr)
        *ppszAuthority = "";
    if (ppszCode != nullptr)
        *ppszCode = "";
    if (ppszVersion != nullptr)
        *ppszVersion = "";

    /* -------------------------------------------------------------------- */
    /*      Verify prefix.                                                  */
    /* -------------------------------------------------------------------- */
    if (!STARTS_WITH_CI(pszURN, "urn:ogc:def:"))
        return false;

    /* -------------------------------------------------------------------- */
    /*      Extract object type                                             */
    /* -------------------------------------------------------------------- */
    if (ppszObjectType != nullptr)
        *ppszObjectType = pszURN + 12;

    int i = 12;
    while (pszURN[i] != ':' && pszURN[i] != '\0')
        i++;

    if (pszURN[i] == '\0')
        return false;

    pszURN[i] = '\0';
    i++;

    /* -------------------------------------------------------------------- */
    /*      Extract authority                                               */
    /* -------------------------------------------------------------------- */
    if (ppszAuthority != nullptr)
        *ppszAuthority = pszURN + i;

    while (pszURN[i] != ':' && pszURN[i] != '\0')
        i++;

    if (pszURN[i] == '\0')
        return false;

    pszURN[i] = '\0';
    i++;

    /* -------------------------------------------------------------------- */
    /*      Extract version                                                 */
    /* -------------------------------------------------------------------- */
    if (ppszVersion != nullptr)
        *ppszVersion = pszURN + i;

    while (pszURN[i] != ':' && pszURN[i] != '\0')
        i++;

    if (pszURN[i] == '\0')
        return false;

    pszURN[i] = '\0';
    i++;

    /* -------------------------------------------------------------------- */
    /*      Extract code.                                                   */
    /* -------------------------------------------------------------------- */
    if (ppszCode != nullptr)
        *ppszCode = pszURN + i;

    return true;
}

/************************************************************************/
/*                               addURN()                               */
/************************************************************************/

static void addURN(CPLXMLNode *psTarget, const char *pszAuthority,
                   const char *pszObjectType, int nCode,
                   const char *pszVersion = "")

{
    if (pszVersion == nullptr)
        pszVersion = "";

    char szURN[200] = {};
    CPLAssert(strlen(pszAuthority) + strlen(pszObjectType) <
              sizeof(szURN) - 30);

    snprintf(szURN, sizeof(szURN), "urn:ogc:def:%s:%s:%s:", pszObjectType,
             pszAuthority, pszVersion);

    if (nCode != 0)
        snprintf(szURN + strlen(szURN), sizeof(szURN) - strlen(szURN), "%d",
                 nCode);

    CPLCreateXMLNode(CPLCreateXMLNode(psTarget, CXT_Attribute, "xlink:href"),
                     CXT_Text, szURN);
}

/************************************************************************/
/*                         AddValueIDWithURN()                          */
/*                                                                      */
/*      Adds element of the form <ElementName                           */
/*      xlink:href="urn_without_id">id</ElementName>"                   */
/************************************************************************/

static CPLXMLNode *AddValueIDWithURN(CPLXMLNode *psTarget,
                                     const char *pszElement,
                                     const char *pszAuthority,
                                     const char *pszObjectType, int nCode,
                                     const char *pszVersion = "")

{
    CPLXMLNode *psElement = CPLCreateXMLNode(psTarget, CXT_Element, pszElement);
    addURN(psElement, pszAuthority, pszObjectType, nCode, pszVersion);

    return psElement;
}

/************************************************************************/
/*                          addAuthorityIDBlock()                          */
/*                                                                      */
/*      Creates a structure like:                                       */
/*      <srsId>                                                         */
/*        <name codeSpace="urn">code</name>                             */
/*      </srsId>                                                        */
/************************************************************************/
static CPLXMLNode *addAuthorityIDBlock(CPLXMLNode *psTarget,
                                       const char *pszElement,
                                       const char *pszAuthority,
                                       const char *pszObjectType, int nCode,
                                       const char *pszVersion = "")

{
    /* -------------------------------------------------------------------- */
    /*      Prepare partial URN without the actual code.                    */
    /* -------------------------------------------------------------------- */
    if (pszVersion == nullptr)
        pszVersion = "";

    char szURN[200] = {};
    CPLAssert(strlen(pszAuthority) + strlen(pszObjectType) <
              sizeof(szURN) - 30);

    snprintf(szURN, sizeof(szURN), "urn:ogc:def:%s:%s:%s:", pszObjectType,
             pszAuthority, pszVersion);

    /* -------------------------------------------------------------------- */
    /*      Prepare the base name, eg. <srsID>.                             */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psElement = CPLCreateXMLNode(psTarget, CXT_Element, pszElement);

    /* -------------------------------------------------------------------- */
    /*      Prepare the name element.                                       */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psName = CPLCreateXMLNode(psElement, CXT_Element, "gml:name");

    /* -------------------------------------------------------------------- */
    /*      Prepare the codespace attribute.                                */
    /* -------------------------------------------------------------------- */
    CPLCreateXMLNode(CPLCreateXMLNode(psName, CXT_Attribute, "codeSpace"),
                     CXT_Text, szURN);

    /* -------------------------------------------------------------------- */
    /*      Attach code value to name node.                                 */
    /* -------------------------------------------------------------------- */
    char szCode[32] = {};
    snprintf(szCode, sizeof(szCode), "%d", nCode);

    CPLCreateXMLNode(psName, CXT_Text, szCode);

    return psElement;
}

/************************************************************************/
/*                              addGMLId()                              */
/************************************************************************/

static void addGMLId(CPLXMLNode *psParent)
{
    static CPLMutex *hGMLIdMutex = nullptr;
    CPLMutexHolderD(&hGMLIdMutex);

    static int nNextGMLId = 1;
    char szIdText[40] = {};

    snprintf(szIdText, sizeof(szIdText), "ogrcrs%d", nNextGMLId++);

    CPLCreateXMLNode(CPLCreateXMLNode(psParent, CXT_Attribute, "gml:id"),
                     CXT_Text, szIdText);
}

/************************************************************************/
/*                        exportAuthorityToXML()                        */
/************************************************************************/

static CPLXMLNode *exportAuthorityToXML(const OGR_SRSNode *poAuthParent,
                                        const char *pszTagName,
                                        CPLXMLNode *psXMLParent,
                                        const char *pszObjectType,
                                        int bUseSubName = TRUE)

{
    /* -------------------------------------------------------------------- */
    /*      Get authority node from parent.                                 */
    /* -------------------------------------------------------------------- */
    const int nAuthority = poAuthParent->FindChild("AUTHORITY");
    if (nAuthority == -1)
        return nullptr;

    const OGR_SRSNode *poAuthority = poAuthParent->GetChild(nAuthority);

    /* -------------------------------------------------------------------- */
    /*      Create identification.                                          */
    /* -------------------------------------------------------------------- */
    if (poAuthority->GetChildCount() < 2)
        return nullptr;

    const char *pszCodeSpace = poAuthority->GetChild(0)->GetValue();
    const char *pszCode = poAuthority->GetChild(1)->GetValue();
    const char *pszEdition = nullptr;

    if (bUseSubName)
        return addAuthorityIDBlock(psXMLParent, pszTagName, pszCodeSpace,
                                   pszObjectType, atoi(pszCode), pszEdition);

    return AddValueIDWithURN(psXMLParent, pszTagName, pszCodeSpace,
                             pszObjectType, atoi(pszCode), pszEdition);
}

/************************************************************************/
/*                             addProjArg()                             */
/************************************************************************/

static void addProjArg(const OGRSpatialReference *poSRS, CPLXMLNode *psBase,
                       const char *pszMeasureType, double dfDefault,
                       int nParameterID, const char *pszWKTName)

{
    CPLXMLNode *psNode = CPLCreateXMLNode(psBase, CXT_Element, "gml:usesValue");

    /* -------------------------------------------------------------------- */
    /*      Handle the UOM.                                                 */
    /* -------------------------------------------------------------------- */
    const char *pszUOMValue = EQUAL(pszMeasureType, "Angular")
                                  ? "urn:ogc:def:uom:EPSG::9102"
                                  : "urn:ogc:def:uom:EPSG::9001";

    CPLXMLNode *psValue = CPLCreateXMLNode(psNode, CXT_Element, "gml:value");

    CPLCreateXMLNode(CPLCreateXMLNode(psValue, CXT_Attribute, "uom"), CXT_Text,
                     pszUOMValue);

    /* -------------------------------------------------------------------- */
    /*      Add the parameter value itself.                                 */
    /* -------------------------------------------------------------------- */
    double dfParamValue =
        poSRS->GetNormProjParm(pszWKTName, dfDefault, nullptr);

    CPLCreateXMLNode(psValue, CXT_Text,
                     CPLString().Printf("%.16g", dfParamValue));

    /* -------------------------------------------------------------------- */
    /*      Add the valueOfParameter.                                       */
    /* -------------------------------------------------------------------- */
    AddValueIDWithURN(psNode, "gml:valueOfParameter", "EPSG", "parameter",
                      nParameterID);
}

/************************************************************************/
/*                              addAxis()                               */
/*                                                                      */
/*      Added the <usesAxis> element and down.                          */
/************************************************************************/

static CPLXMLNode *addAxis(CPLXMLNode *psXMLParent,
                           const char *pszAxis,  // "Lat", "Long", "E" or "N"
                           const OGR_SRSNode * /* poUnitsSrc */)

{
    CPLXMLNode *psAxisXML = CPLCreateXMLNode(
        CPLCreateXMLNode(psXMLParent, CXT_Element, "gml:usesAxis"), CXT_Element,
        "gml:CoordinateSystemAxis");
    if (!psAxisXML)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "addAxis failed.");
        return nullptr;
    }
    addGMLId(psAxisXML);

    if (EQUAL(pszAxis, "Lat"))
    {
        CPLCreateXMLNode(CPLCreateXMLNode(psAxisXML, CXT_Attribute, "gml:uom"),
                         CXT_Text, "urn:ogc:def:uom:EPSG::9102");

        CPLCreateXMLElementAndValue(psAxisXML, "gml:name", "Geodetic latitude");
        addAuthorityIDBlock(psAxisXML, "gml:axisID", "EPSG", "axis", 9901);
        CPLCreateXMLElementAndValue(psAxisXML, "gml:axisAbbrev", "Lat");
        CPLCreateXMLElementAndValue(psAxisXML, "gml:axisDirection", "north");
    }
    else if (EQUAL(pszAxis, "Long"))
    {
        CPLCreateXMLNode(CPLCreateXMLNode(psAxisXML, CXT_Attribute, "gml:uom"),
                         CXT_Text, "urn:ogc:def:uom:EPSG::9102");

        CPLCreateXMLElementAndValue(psAxisXML, "gml:name",
                                    "Geodetic longitude");
        addAuthorityIDBlock(psAxisXML, "gml:axisID", "EPSG", "axis", 9902);
        CPLCreateXMLElementAndValue(psAxisXML, "gml:axisAbbrev", "Lon");
        CPLCreateXMLElementAndValue(psAxisXML, "gml:axisDirection", "east");
    }
    else if (EQUAL(pszAxis, "E"))
    {
        CPLCreateXMLNode(CPLCreateXMLNode(psAxisXML, CXT_Attribute, "gml:uom"),
                         CXT_Text, "urn:ogc:def:uom:EPSG::9001");

        CPLCreateXMLElementAndValue(psAxisXML, "gml:name", "Easting");
        addAuthorityIDBlock(psAxisXML, "gml:axisID", "EPSG", "axis", 9906);
        CPLCreateXMLElementAndValue(psAxisXML, "gml:axisAbbrev", "E");
        CPLCreateXMLElementAndValue(psAxisXML, "gml:axisDirection", "east");
    }
    else if (EQUAL(pszAxis, "N"))
    {
        CPLCreateXMLNode(CPLCreateXMLNode(psAxisXML, CXT_Attribute, "gml:uom"),
                         CXT_Text, "urn:ogc:def:uom:EPSG::9001");

        CPLCreateXMLElementAndValue(psAxisXML, "gml:name", "Northing");
        addAuthorityIDBlock(psAxisXML, "gml:axisID", "EPSG", "axis", 9907);
        CPLCreateXMLElementAndValue(psAxisXML, "gml:axisAbbrev", "N");
        CPLCreateXMLElementAndValue(psAxisXML, "gml:axisDirection", "north");
    }
    else
    {
        CPLAssert(false);
    }

    return psAxisXML;
}

/************************************************************************/
/*                         exportGeogCSToXML()                          */
/************************************************************************/

static CPLXMLNode *exportGeogCSToXML(const OGRSpatialReference *poSRS)

{
    const OGR_SRSNode *poGeogCS = poSRS->GetAttrNode("GEOGCS");

    if (poGeogCS == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Establish initial infrastructure.                               */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psGCS_XML =
        CPLCreateXMLNode(nullptr, CXT_Element, "gml:GeographicCRS");
    addGMLId(psGCS_XML);

    /* -------------------------------------------------------------------- */
    /*      Attach symbolic name (srsName).                                 */
    /* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue(psGCS_XML, "gml:srsName",
                                poGeogCS->GetChild(0)->GetValue());

    /* -------------------------------------------------------------------- */
    /*      Does the overall coordinate system have an authority?  If so    */
    /*      attach as an identification section.                            */
    /* -------------------------------------------------------------------- */
    exportAuthorityToXML(poGeogCS, "gml:srsID", psGCS_XML, "crs");

    /* -------------------------------------------------------------------- */
    /*      Insert a big whack of fixed stuff defining the                  */
    /*      ellipsoidalCS.  Basically this defines the axes and their       */
    /*      units.                                                          */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psECS = CPLCreateXMLNode(
        CPLCreateXMLNode(psGCS_XML, CXT_Element, "gml:usesEllipsoidalCS"),
        CXT_Element, "gml:EllipsoidalCS");

    addGMLId(psECS);

    CPLCreateXMLElementAndValue(psECS, "gml:csName", "ellipsoidal");

    addAuthorityIDBlock(psECS, "gml:csID", "EPSG", "cs", 6402);

    addAxis(psECS, "Lat", nullptr);
    addAxis(psECS, "Long", nullptr);

    /* -------------------------------------------------------------------- */
    /*      Start with the datum.                                           */
    /* -------------------------------------------------------------------- */
    const OGR_SRSNode *poDatum = poGeogCS->GetNode("DATUM");

    if (poDatum == nullptr)
    {
        CPLDestroyXMLNode(psGCS_XML);
        return nullptr;
    }

    CPLXMLNode *psDatumXML = CPLCreateXMLNode(
        CPLCreateXMLNode(psGCS_XML, CXT_Element, "gml:usesGeodeticDatum"),
        CXT_Element, "gml:GeodeticDatum");

    addGMLId(psDatumXML);

    /* -------------------------------------------------------------------- */
    /*      Set the datumName.                                              */
    /* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue(psDatumXML, "gml:datumName",
                                poDatum->GetChild(0)->GetValue());

    /* -------------------------------------------------------------------- */
    /*      Set authority id info if available.                             */
    /* -------------------------------------------------------------------- */
    exportAuthorityToXML(poDatum, "gml:datumID", psDatumXML, "datum");

    /* -------------------------------------------------------------------- */
    /*      Setup prime meridian information.                               */
    /* -------------------------------------------------------------------- */
    const OGR_SRSNode *poPMNode = poGeogCS->GetNode("PRIMEM");
    const char *pszPMName = "Greenwich";
    double dfPMOffset = poSRS->GetPrimeMeridian(&pszPMName);

    CPLXMLNode *psPM = CPLCreateXMLNode(
        CPLCreateXMLNode(psDatumXML, CXT_Element, "gml:usesPrimeMeridian"),
        CXT_Element, "gml:PrimeMeridian");

    addGMLId(psPM);

    CPLCreateXMLElementAndValue(psPM, "gml:meridianName", pszPMName);

    if (poPMNode)
        exportAuthorityToXML(poPMNode, "gml:meridianID", psPM, "meridian");

    CPLXMLNode *psAngle = CPLCreateXMLNode(
        CPLCreateXMLNode(psPM, CXT_Element, "gml:greenwichLongitude"),
        CXT_Element, "gml:angle");

    CPLCreateXMLNode(CPLCreateXMLNode(psAngle, CXT_Attribute, "uom"), CXT_Text,
                     "urn:ogc:def:uom:EPSG::9102");

    CPLCreateXMLNode(psAngle, CXT_Text,
                     CPLString().Printf("%.16g", dfPMOffset));

    /* -------------------------------------------------------------------- */
    /*      Translate the ellipsoid.                                        */
    /* -------------------------------------------------------------------- */
    const OGR_SRSNode *poEllipsoid = poDatum->GetNode("SPHEROID");

    if (poEllipsoid != nullptr)
    {
        CPLXMLNode *psEllipseXML = CPLCreateXMLNode(
            CPLCreateXMLNode(psDatumXML, CXT_Element, "gml:usesEllipsoid"),
            CXT_Element, "gml:Ellipsoid");

        addGMLId(psEllipseXML);

        CPLCreateXMLElementAndValue(psEllipseXML, "gml:ellipsoidName",
                                    poEllipsoid->GetChild(0)->GetValue());

        exportAuthorityToXML(poEllipsoid, "gml:ellipsoidID", psEllipseXML,
                             "ellipsoid");

        CPLXMLNode *psParamXML =
            CPLCreateXMLNode(psEllipseXML, CXT_Element, "gml:semiMajorAxis");

        CPLCreateXMLNode(CPLCreateXMLNode(psParamXML, CXT_Attribute, "uom"),
                         CXT_Text, "urn:ogc:def:uom:EPSG::9001");

        CPLCreateXMLNode(psParamXML, CXT_Text,
                         poEllipsoid->GetChild(1)->GetValue());

        psParamXML =
            CPLCreateXMLNode(CPLCreateXMLNode(psEllipseXML, CXT_Element,
                                              "gml:secondDefiningParameter"),
                             CXT_Element, "gml:inverseFlattening");

        CPLCreateXMLNode(CPLCreateXMLNode(psParamXML, CXT_Attribute, "uom"),
                         CXT_Text, "urn:ogc:def:uom:EPSG::9201");

        CPLCreateXMLNode(psParamXML, CXT_Text,
                         poEllipsoid->GetChild(2)->GetValue());
    }

    return psGCS_XML;
}

/************************************************************************/
/*                         exportProjCSToXML()                          */
/************************************************************************/

static CPLXMLNode *exportProjCSToXML(const OGRSpatialReference *poSRS)

{
    const OGR_SRSNode *poProjCS = poSRS->GetAttrNode("PROJCS");

    if (poProjCS == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Establish initial infrastructure.                               */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psCRS_XML =
        CPLCreateXMLNode(nullptr, CXT_Element, "gml:ProjectedCRS");
    addGMLId(psCRS_XML);

    /* -------------------------------------------------------------------- */
    /*      Attach symbolic name (a name in a nameset).                     */
    /* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue(psCRS_XML, "gml:srsName",
                                poProjCS->GetChild(0)->GetValue());

    /* -------------------------------------------------------------------- */
    /*      Add authority info if we have it.                               */
    /* -------------------------------------------------------------------- */
    exportAuthorityToXML(poProjCS, "gml:srsID", psCRS_XML, "crs");

    /* -------------------------------------------------------------------- */
    /*      Use the GEOGCS as a <baseCRS>                                   */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psBaseCRSXML =
        CPLCreateXMLNode(psCRS_XML, CXT_Element, "gml:baseCRS");

    CPLAddXMLChild(psBaseCRSXML, exportGeogCSToXML(poSRS));

    /* -------------------------------------------------------------------- */
    /*      Our projected coordinate system is "defined by Conversion".     */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psDefinedBy =
        CPLCreateXMLNode(psCRS_XML, CXT_Element, "gml:definedByConversion");

    /* -------------------------------------------------------------------- */
    /*      Projections are handled as ParameterizedTransformations.        */
    /* -------------------------------------------------------------------- */
    const char *pszProjection = poSRS->GetAttrValue("PROJECTION");
    CPLXMLNode *psConv =
        CPLCreateXMLNode(psDefinedBy, CXT_Element, "gml:Conversion");
    addGMLId(psConv);

    CPLCreateXMLNode(
        CPLCreateXMLNode(psConv, CXT_Element, "gml:coordinateOperationName"),
        CXT_Text, pszProjection);

    /* -------------------------------------------------------------------- */
    /*      Transverse Mercator                                             */
    /* -------------------------------------------------------------------- */
    if (pszProjection == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "No projection method");
    }
    else if (EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR))
    {
        AddValueIDWithURN(psConv, "gml:usesMethod", "EPSG", "method", 9807);

        addProjArg(poSRS, psConv, "Angular", 0.0, 8801,
                   SRS_PP_LATITUDE_OF_ORIGIN);
        addProjArg(poSRS, psConv, "Angular", 0.0, 8802,
                   SRS_PP_CENTRAL_MERIDIAN);
        addProjArg(poSRS, psConv, "Unitless", 1.0, 8805, SRS_PP_SCALE_FACTOR);
        addProjArg(poSRS, psConv, "Linear", 0.0, 8806, SRS_PP_FALSE_EASTING);
        addProjArg(poSRS, psConv, "Linear", 0.0, 8807, SRS_PP_FALSE_NORTHING);
    }
    /* -------------------------------------------------------------------- */
    /*      Lambert Conformal Conic                                         */
    /* -------------------------------------------------------------------- */
    else if (EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP))
    {
        AddValueIDWithURN(psConv, "gml:usesMethod", "EPSG", "method", 9801);

        addProjArg(poSRS, psConv, "Angular", 0.0, 8801,
                   SRS_PP_LATITUDE_OF_ORIGIN);
        addProjArg(poSRS, psConv, "Angular", 0.0, 8802,
                   SRS_PP_CENTRAL_MERIDIAN);
        addProjArg(poSRS, psConv, "Unitless", 1.0, 8805, SRS_PP_SCALE_FACTOR);
        addProjArg(poSRS, psConv, "Linear", 0.0, 8806, SRS_PP_FALSE_EASTING);
        addProjArg(poSRS, psConv, "Linear", 0.0, 8807, SRS_PP_FALSE_NORTHING);
    }
    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Unhandled projection method %s", pszProjection);
    }

    /* -------------------------------------------------------------------- */
    /*      Define the cartesian coordinate system.                         */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psCCS = CPLCreateXMLNode(
        CPLCreateXMLNode(psCRS_XML, CXT_Element, "gml:usesCartesianCS"),
        CXT_Element, "gml:CartesianCS");

    addGMLId(psCCS);

    CPLCreateXMLElementAndValue(psCCS, "gml:csName", "Cartesian");
    addAuthorityIDBlock(psCCS, "gml:csID", "EPSG", "cs", 4400);
    addAxis(psCCS, "E", nullptr);
    addAxis(psCCS, "N", nullptr);

    return psCRS_XML;
}

/************************************************************************/
/*                            exportToXML()                             */
/************************************************************************/

/**
 * \brief Export coordinate system in XML format.
 *
 * Converts the loaded coordinate reference system into XML format
 * to the extent possible.  The string returned in ppszRawXML should be
 * deallocated by the caller with CPLFree() when no longer needed.
 *
 * LOCAL_CS coordinate systems are not translatable.  An empty string
 * will be returned along with OGRERR_NONE.
 *
 * This method is the equivalent of the C function OSRExportToXML().
 *
 * @param ppszRawXML pointer to which dynamically allocated XML definition
 * will be assigned.
 * @param pszDialect currently ignored. The dialect used is GML based.
 *
 * @return OGRERR_NONE on success or an error code on failure.
 */

OGRErr OGRSpatialReference::exportToXML(char **ppszRawXML,
                                        CPL_UNUSED const char *pszDialect) const
{
    CPLXMLNode *psXMLTree = nullptr;

    if (IsGeographic())
    {
        psXMLTree = exportGeogCSToXML(this);
    }
    else if (IsProjected())
    {
        psXMLTree = exportProjCSToXML(this);
    }
    else
        return OGRERR_UNSUPPORTED_SRS;

    *ppszRawXML = CPLSerializeXMLTree(psXMLTree);
    CPLDestroyXMLNode(psXMLTree);

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRExportToXML()                           */
/************************************************************************/
/**
 * \brief Export coordinate system in XML format.
 *
 * This function is the same as OGRSpatialReference::exportToXML().
 */

OGRErr OSRExportToXML(OGRSpatialReferenceH hSRS, char **ppszRawXML,
                      const char *pszDialect)

{
    VALIDATE_POINTER1(hSRS, "OSRExportToXML", OGRERR_FAILURE);

    return OGRSpatialReference::FromHandle(hSRS)->exportToXML(ppszRawXML,
                                                              pszDialect);
}

#ifdef notdef
/************************************************************************/
/*                           importXMLUnits()                           */
/************************************************************************/

static void importXMLUnits(CPLXMLNode *psSrcXML, const char *pszClass,
                           OGRSpatialReference *poSRS, const char *pszTarget)

{
    OGR_SRSNode *poNode = poSRS->GetAttrNode(pszTarget);

    CPLAssert(EQUAL(pszClass, "AngularUnit") || EQUAL(pszClass, "LinearUnit"));

    psSrcXML = CPLGetXMLNode(psSrcXML, pszClass);

    OGR_SRSNode *poUnits = NULL;
    const char *pszUnitName = NULL;
    const char *pszUnitsPer = NULL;

    // TODO(schwehr): Remove the goto.
    if (psSrcXML == NULL)
        goto DefaultTarget;

    pszUnitName = CPLGetXMLValue(psSrcXML, "NameSet.name", "unnamed");

    pszUnitsPer = EQUAL(pszClass, "AngularUnit")
                      ? CPLGetXMLValue(psSrcXML, "radiansPerUnit", NULL)
                      : CPLGetXMLValue(psSrcXML, "metresPerUnit", NULL);

    if (pszUnitsPer == NULL)
    {
        CPLDebug("OGR_SRS_XML", "Missing PerUnit value for %s.", pszClass);
        goto DefaultTarget;
    }

    if (poNode == NULL)
    {
        CPLDebug("OGR_SRS_XML", "Can't find %s in importXMLUnits.", pszTarget);
        goto DefaultTarget;
    }

    if (poNode->FindChild("UNIT") != -1)
    {
        poUnits = poNode->GetChild(poNode->FindChild("UNIT"));
        poUnits->GetChild(0)->SetValue(pszUnitName);
        poUnits->GetChild(1)->SetValue(pszUnitsPer);
    }
    else
    {
        poUnits = new OGR_SRSNode("UNIT");
        poUnits->AddChild(new OGR_SRSNode(pszUnitName));
        poUnits->AddChild(new OGR_SRSNode(pszUnitsPer));

        poNode->AddChild(poUnits);
    }
    return;

DefaultTarget:
    poUnits = new OGR_SRSNode("UNIT");
    if (EQUAL(pszClass, "AngularUnit"))
    {
        poUnits->AddChild(new OGR_SRSNode(SRS_UA_DEGREE));
        poUnits->AddChild(new OGR_SRSNode(SRS_UA_DEGREE_CONV));
    }
    else
    {
        poUnits->AddChild(new OGR_SRSNode(SRS_UL_METER));
        poUnits->AddChild(new OGR_SRSNode("1.0"));
    }

    poNode->AddChild(poUnits);
}
#endif

/************************************************************************/
/*                         importXMLAuthority()                         */
/************************************************************************/

static void importXMLAuthority(CPLXMLNode *psSrcXML, OGRSpatialReference *poSRS,
                               const char *pszSourceKey,
                               const char *pszTargetKey)

{
    CPLXMLNode *psIDNode = CPLGetXMLNode(psSrcXML, pszSourceKey);
    CPLXMLNode *psNameNode = CPLGetXMLNode(psIDNode, "name");
    CPLXMLNode *psCodeSpace = CPLGetXMLNode(psNameNode, "codeSpace");

    if (psIDNode == nullptr || psNameNode == nullptr || psCodeSpace == nullptr)
        return;

    char *pszURN = CPLStrdup(CPLGetXMLValue(psCodeSpace, "", ""));

    const char *pszAuthority;
    const char *pszCode;
    if (!parseURN(pszURN, nullptr, &pszAuthority, &pszCode))
    {
        CPLFree(pszURN);
        return;
    }

    if (strlen(pszCode) == 0)
        pszCode = CPLGetXMLValue(psNameNode, "", "");

    const int nCode = pszCode != nullptr ? atoi(pszCode) : 0;

    if (nCode != 0)
        poSRS->SetAuthority(pszTargetKey, pszAuthority, nCode);

    CPLFree(pszURN);
}

/************************************************************************/
/*                           ParseOGCDefURN()                           */
/*                                                                      */
/*      Parse out fields from a URN of the form:                        */
/*        urn:ogc:def:parameter:EPSG:6.3:9707                           */
/************************************************************************/

static bool ParseOGCDefURN(const char *pszURN, CPLString *poObjectType,
                           CPLString *poAuthority, CPLString *poVersion,
                           CPLString *poValue)

{
    if (poObjectType != nullptr)
        *poObjectType = "";

    if (poAuthority != nullptr)
        *poAuthority = "";

    if (poVersion != nullptr)
        *poVersion = "";

    if (poValue != nullptr)
        *poValue = "";

    if (pszURN == nullptr || !STARTS_WITH_CI(pszURN, "urn:ogc:def:"))
        return false;

    char **papszTokens =
        CSLTokenizeStringComplex(pszURN + 12, ":", FALSE, TRUE);

    if (CSLCount(papszTokens) != 4)
    {
        CSLDestroy(papszTokens);
        return false;
    }

    if (poObjectType != nullptr)
        *poObjectType = papszTokens[0];

    if (poAuthority != nullptr)
        *poAuthority = papszTokens[1];

    if (poVersion != nullptr)
        *poVersion = papszTokens[2];

    if (poValue != nullptr)
        *poValue = papszTokens[3];

    CSLDestroy(papszTokens);
    return true;
}

/************************************************************************/
/*                       getEPSGObjectCodeValue()                       */
/*                                                                      */
/*      Fetch a code value from the indicated node.  Should work on     */
/*      something of the form <elem xlink:href="urn:...:n" /> or        */
/*      something of the form <elem xlink:href="urn:...:">n</a>.        */
/************************************************************************/

static int getEPSGObjectCodeValue(CPLXMLNode *psNode,
                                  const char *pszEPSGObjectType, /*"method" */
                                  int nDefault)

{
    if (psNode == nullptr)
        return nDefault;

    const char *pszHrefVal = CPLGetXMLValue(psNode, "xlink:href", nullptr);
    if (pszHrefVal == nullptr)
        pszHrefVal = CPLGetXMLValue(psNode, "href", nullptr);

    CPLString osObjectType;
    CPLString osAuthority;
    CPLString osValue;
    if (!ParseOGCDefURN(pszHrefVal, &osObjectType, &osAuthority, nullptr,
                        &osValue))
        return nDefault;

    if (!EQUAL(osAuthority, "EPSG") || !EQUAL(osObjectType, pszEPSGObjectType))
        return nDefault;

    if (!osValue.empty())
        return atoi(osValue);

    const char *pszValue = CPLGetXMLValue(psNode, "", nullptr);
    if (pszValue != nullptr)
        return atoi(pszValue);

    return nDefault;
}

/************************************************************************/
/*                         getProjectionParam()                          */
/************************************************************************/

static double getProjectionParam(CPLXMLNode *psRootNode, int nParameterCode,
                                 const char * /*pszMeasureType */,
                                 double dfDefault)

{
    for (CPLXMLNode *psUsesParameter = psRootNode->psChild;
         psUsesParameter != nullptr; psUsesParameter = psUsesParameter->psNext)
    {
        if (psUsesParameter->eType != CXT_Element)
            continue;

        if (!EQUAL(psUsesParameter->pszValue, "usesParameterValue") &&
            !EQUAL(psUsesParameter->pszValue, "usesValue"))
            continue;

        if (getEPSGObjectCodeValue(
                CPLGetXMLNode(psUsesParameter, "valueOfParameter"), "parameter",
                0) == nParameterCode)
        {
            const char *pszValue =
                CPLGetXMLValue(psUsesParameter, "value", nullptr);

            if (pszValue == nullptr)
                return dfDefault;

            return CPLAtof(pszValue);
        }
    }

    return dfDefault;
}

/************************************************************************/
/*                         getNormalizedValue()                         */
/*                                                                      */
/*      Parse a node to get its numerical value, and then normalize     */
/*      into meters of degrees depending on the measure type.           */
/************************************************************************/

static double getNormalizedValue(CPLXMLNode *psNode, const char *pszPath,
                                 const char * /*pszMeasure*/, double dfDefault)

{
    CPLXMLNode *psTargetNode = pszPath == nullptr || strlen(pszPath) == 0
                                   ? psNode
                                   : CPLGetXMLNode(psNode, pszPath);

    if (psTargetNode == nullptr)
        return dfDefault;

    CPLXMLNode *psValueNode = psTargetNode->psChild;  // Used after for.
    for (; psValueNode != nullptr && psValueNode->eType != CXT_Text;
         psValueNode = psValueNode->psNext)
    {
    }

    if (psValueNode == nullptr)
        return dfDefault;

    // Add normalization later.

    return CPLAtof(psValueNode->pszValue);
}

/************************************************************************/
/*                        importGeogCSFromXML()                         */
/************************************************************************/

static OGRErr importGeogCSFromXML(OGRSpatialReference *poSRS, CPLXMLNode *psCRS)

{
    /* -------------------------------------------------------------------- */
    /*      Set the GEOGCS name from the srsName.                           */
    /* -------------------------------------------------------------------- */
    const char *pszGeogName =
        CPLGetXMLValue(psCRS, "srsName", "Unnamed GeogCS");

    /* -------------------------------------------------------------------- */
    /*      If we don't seem to have a detailed coordinate system           */
    /*      definition, check if we can define based on an EPSG code.       */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psDatum =
        CPLGetXMLNode(psCRS, "usesGeodeticDatum.GeodeticDatum");

    if (psDatum == nullptr)
    {
        OGRSpatialReference oIdSRS;

        oIdSRS.SetLocalCS("dummy");
        importXMLAuthority(psCRS, &oIdSRS, "srsID", "LOCAL_CS");

        if (oIdSRS.GetAuthorityCode("LOCAL_CS") != nullptr &&
            oIdSRS.GetAuthorityName("LOCAL_CS") != nullptr &&
            EQUAL(oIdSRS.GetAuthorityName("LOCAL_CS"), "EPSG"))
        {
            return poSRS->importFromEPSG(
                atoi(oIdSRS.GetAuthorityCode("LOCAL_CS")));
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Get datum name.                                                 */
    /* -------------------------------------------------------------------- */
    const char *pszDatumName =
        CPLGetXMLValue(psDatum, "datumName", "Unnamed Datum");

    /* -------------------------------------------------------------------- */
    /*      Get ellipsoid information.                                      */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psE = CPLGetXMLNode(psDatum, "usesEllipsoid.Ellipsoid");
    const char *pszEllipsoidName =
        CPLGetXMLValue(psE, "ellipsoidName", "Unnamed Ellipsoid");

    const double dfSemiMajor =
        getNormalizedValue(psE, "semiMajorAxis", "Linear", SRS_WGS84_SEMIMAJOR);

    const double dfInvFlattening = getNormalizedValue(
        psE, "secondDefiningParameter.inverseFlattening", "Unitless", 0.0);

    if (dfInvFlattening == 0.0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Ellipsoid inverseFlattening corrupt or missing.");
        return OGRERR_CORRUPT_DATA;
    }

    /* -------------------------------------------------------------------- */
    /*      Get the prime meridian.                                         */
    /* -------------------------------------------------------------------- */
    const char *pszPMName = nullptr;
    double dfPMOffset = 0.0;

    CPLXMLNode *psPM =
        CPLGetXMLNode(psDatum, "usesPrimeMeridian.PrimeMeridian");
    if (psPM == nullptr)
    {
        pszPMName = "Greenwich";
        dfPMOffset = 0.0;
    }
    else
    {
        pszPMName =
            CPLGetXMLValue(psPM, "meridianName", "Unnamed Prime Meridian");
        dfPMOffset = getNormalizedValue(psPM, "greenwichLongitude.angle",
                                        "Angular", 0.0);
    }

    /* -------------------------------------------------------------------- */
    /*      Set the geographic definition.                                  */
    /* -------------------------------------------------------------------- */
    poSRS->SetGeogCS(pszGeogName, pszDatumName, pszEllipsoidName, dfSemiMajor,
                     dfInvFlattening, pszPMName, dfPMOffset);

/* -------------------------------------------------------------------- */
/*      Look for angular units.  We don't check that all axes match     */
/*      at this time.                                                   */
/* -------------------------------------------------------------------- */
#if 0  // Does not compile.
    CPLXMLNode *psAxis =
        CPLGetXMLNode( psGeo2DCRS,
                       "EllipsoidalCoordinateSystem.CoordinateAxis" );
    importXMLUnits( psAxis, "AngularUnit", poSRS, "GEOGCS" );
#endif

    /* -------------------------------------------------------------------- */
    /*      Can we set authorities for any of the levels?                   */
    /* -------------------------------------------------------------------- */
    importXMLAuthority(psCRS, poSRS, "srsID", "GEOGCS");
    importXMLAuthority(psDatum, poSRS, "datumID", "GEOGCS|DATUM");
    importXMLAuthority(psE, poSRS, "ellipsoidID", "GEOGCS|DATUM|SPHEROID");
    importXMLAuthority(psDatum, poSRS,
                       "usesPrimeMeridian.PrimeMeridian.meridianID",
                       "GEOGCS|PRIMEM");

    return OGRERR_NONE;
}

/************************************************************************/
/*                        importProjCSFromXML()                         */
/************************************************************************/

static OGRErr importProjCSFromXML(OGRSpatialReference *poSRS, CPLXMLNode *psCRS)

{
    /* -------------------------------------------------------------------- */
    /*      Setup the PROJCS node with a name.                              */
    /* -------------------------------------------------------------------- */
    poSRS->SetProjCS(CPLGetXMLValue(psCRS, "srsName", "Unnamed"));

    /* -------------------------------------------------------------------- */
    /*      Get authority information if available.  If we got it, and      */
    /*      we seem to be lacking inline definition values, try and         */
    /*      define according to the EPSG code for the PCS.                  */
    /* -------------------------------------------------------------------- */
    importXMLAuthority(psCRS, poSRS, "srsID", "PROJCS");

    if (poSRS->GetAuthorityCode("PROJCS") != nullptr &&
        poSRS->GetAuthorityName("PROJCS") != nullptr &&
        EQUAL(poSRS->GetAuthorityName("PROJCS"), "EPSG") &&
        (CPLGetXMLNode(psCRS, "definedByConversion.Conversion") == nullptr ||
         CPLGetXMLNode(psCRS, "baseCRS.GeographicCRS") == nullptr))
    {
        return poSRS->importFromEPSG(atoi(poSRS->GetAuthorityCode("PROJCS")));
    }

    /* -------------------------------------------------------------------- */
    /*      Try to set the GEOGCS info.                                     */
    /* -------------------------------------------------------------------- */

    CPLXMLNode *psSubXML = CPLGetXMLNode(psCRS, "baseCRS.GeographicCRS");
    if (psSubXML != nullptr)
    {
        const OGRErr eErr = importGeogCSFromXML(poSRS, psSubXML);
        if (eErr != OGRERR_NONE)
            return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Get the conversion node.  It should be the only child of the    */
    /*      definedByConversion node.                                       */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psConv = CPLGetXMLNode(psCRS, "definedByConversion.Conversion");
    if (psConv == nullptr || psConv->eType != CXT_Element)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to find a conversion node under the "
                 "definedByConversion node of the ProjectedCRS.");
        return OGRERR_CORRUPT_DATA;
    }

    /* -------------------------------------------------------------------- */
    /*      Determine the conversion method in effect.                      */
    /* -------------------------------------------------------------------- */
    const int nMethod = getEPSGObjectCodeValue(
        CPLGetXMLNode(psConv, "usesMethod"), "method", 0);

    /* -------------------------------------------------------------------- */
    /*      Transverse Mercator.                                            */
    /* -------------------------------------------------------------------- */
    if (nMethod == 9807)
    {
        poSRS->SetTM(getProjectionParam(psConv, 8801, "Angular", 0.0),
                     getProjectionParam(psConv, 8802, "Angular", 0.0),
                     getProjectionParam(psConv, 8805, "Unitless", 1.0),
                     getProjectionParam(psConv, 8806, "Linear", 0.0),
                     getProjectionParam(psConv, 8807, "Linear", 0.0));
    }

    /* -------------------------------------------------------------------- */
    /*      Didn't recognise?                                               */
    /* -------------------------------------------------------------------- */
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Conversion method %d not recognised.", nMethod);
        return OGRERR_CORRUPT_DATA;
    }

    // Re-set authority as all editions above will have removed it
    importXMLAuthority(psCRS, poSRS, "srsID", "PROJCS");

    // Need to get linear units here!

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromXML()                            */
/************************************************************************/

/**
 * \brief Import coordinate system from XML format (GML only currently).
 *
 * This method is the same as the C function OSRImportFromXML()
 * @param pszXML XML string to import
 * @return OGRERR_NONE on success or OGRERR_CORRUPT_DATA on failure.
 */
OGRErr OGRSpatialReference::importFromXML(const char *pszXML)

{
    Clear();

    /* -------------------------------------------------------------------- */
    /*      Parse the XML.                                                  */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psTree = CPLParseXMLString(pszXML);

    if (psTree == nullptr)
        return OGRERR_CORRUPT_DATA;

    CPLStripXMLNamespace(psTree, "gml", TRUE);

    /* -------------------------------------------------------------------- */
    /*      Import according to the root node type.  We walk through        */
    /*      root elements as there is sometimes prefix stuff like           */
    /*      <?xml>.                                                         */
    /* -------------------------------------------------------------------- */
    OGRErr eErr = OGRERR_UNSUPPORTED_SRS;

    for (CPLXMLNode *psNode = psTree; psNode != nullptr;
         psNode = psNode->psNext)
    {
        if (EQUAL(psNode->pszValue, "GeographicCRS"))
        {
            eErr = importGeogCSFromXML(this, psNode);
            break;
        }

        else if (EQUAL(psNode->pszValue, "ProjectedCRS"))
        {
            eErr = importProjCSFromXML(this, psNode);
            break;
        }
    }

    CPLDestroyXMLNode(psTree);

    return eErr;
}

/************************************************************************/
/*                          OSRImportFromXML()                          */
/************************************************************************/

/**
 * \brief Import coordinate system from XML format (GML only currently).
 *
 * This function is the same as OGRSpatialReference::importFromXML().
 */
OGRErr OSRImportFromXML(OGRSpatialReferenceH hSRS, const char *pszXML)

{
    VALIDATE_POINTER1(hSRS, "OSRImportFromXML", OGRERR_FAILURE);
    VALIDATE_POINTER1(pszXML, "OSRImportFromXML", OGRERR_FAILURE);

    return OGRSpatialReference::FromHandle(hSRS)->importFromXML(pszXML);
}
