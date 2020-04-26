.. _rfc-18:

================================================================================
RFC 18: OGR Style Support in C API
================================================================================

Author: Daniel Morissette

Contact: dmorissette@mapgears.com

Status: Adopted (2007-12-05)

Summary
-------

OGR has a number of C++ classes that deal with the encoding of style
information and attaching that to features. More information is
available in the :ref:`ogr_feature_style` document.

With GDAL/OGR version 1.4.x and older, it was not possible to deal with
style information using the C API. This RFC proposes the addition of
functions to the C API to manipulate style information in GDAL/OGR 1.5.

Implementation Details
----------------------

-  The following enums will be moved from ogr_featurestyle.h to
   ogr_core.h:

::

       OGRSTClassId;
       OGRSTUnitId;
       OGRSTPenParam;
       OGRSTBrushParam;
       OGRSTSymbolParam;
       OGRSTLabelParam;

-  The OGRStyleMgrH (corresponding to the OGRStyleMgr C++ class) will be
   added to the C API:

::

       OGRStyleMgrH  OGR_SM_Create()
       void          OGR_SM_Destroy(OGRStyleMgrH hSM)

       const char   *OGR_SM_InitFromFeature(OGRStyleMgrH hSM)
       int           OGR_SM_InitFromStyleString(const char *pszStyleString)
       int           OGR_SM_GetPartCount(OGRStyleMgrH hSM)
       OGRStyleToolH OGR_SM_GetPart(OGRStyleMgrH hSM)
       int           OGR_SM_AddPart(OGRStyleMgrH hSM, OGRStyleTool *sPart)

-  The OGRStyleToolH (corresponding to the OGRStyleTool C++ class) will
   be added to the C API:

::

        OGRStyleToolH OGR_ST_Create(OGRSTClassId eClassId)
        void          OGR_ST_Destroy(OGRStyleToolH hST)
        OGRSTClassId  OGR_ST_GetType(OGRStyleToolH hST)

        OGRSTUnitId   OGR_ST_GetUnit(OGRStyleToolH hST)
        void          OGR_ST_SetUnit(OGRStyleToolH hST, OGRSTUnitId eUnit, double dfGroundPaperScale)

        int           OGR_ST_GetParamIsNull(OGRStyleToolH hST, int eParam)
        const char   *OGR_ST_GetParamStr(OGRStyleToolH hST, int eParam)
        int           OGR_ST_GetParamNum(OGRStyleToolH hST, int eParam)
        double        OGR_ST_GetParamDbl(OGRStyleToolH hST, int eParam)
        void          OGR_ST_SetParamStr(OGRStyleToolH hST, int eParam, const char *pszParamString)
        void          OGR_ST_SetParamNum(OGRStyleToolH hST, int eParam, int nParam)
        void          OGR_ST_SetParamDbl(OGRStyleToolH hST, int eParam, double dfParam)
        const char   *OGR_ST_GetStyleString(OGRStyleToolH hST)

        int           OGR_ST_GetRGBFromString(OGRStyleToolH hST, const char *pszColor, 
                                             int *nRed, int *nGreen, int *nBlue, int *nAlpha);

Note: at implementation time, the OGR_ST_GetParamIsNull() has been
removed and replaced by an 'int \*bValueIsNull' argument on all the
OGR_ST_GetParam...() functions in order to map more closely to the C++
methods.

-  NO wrappers will be needed for the following C++ classes which are
   handled internally by the OGR\_ST\_\* wrappers above:

::

       class OGRStylePen : public OGRStyleTool
       class OGRStyleBrush : public OGRStyleTool
       class OGRStyleSymbol : public OGRStyleTool
       class OGRStyleLabel : public OGRStyleTool

-  Note that ogr_featurestyle.h also contains a OGRSTVectorParam enum
   and a corresponding OGRStyleVector class but this class is currently
   unused and may eventually be removed, so we will not implement
   support for it in the C API (and the OGRSTVectorParam enum will NOT
   be moved to ogr_core.h).

Python and other language bindings
----------------------------------

The initial implementation will be for the C API only and will not be
ported/tested with the Python and other scripting language bindings.
This will have to wait for a later release.

Implementation
--------------

Daniel Morissette will implement the changes to the C API described in
this RFC for the GDAL/OGR 1.5.0 release.

The first test of the new C API functions will be the conversion of
MapServer's mapogr.cpp to use them.

Related Ticket(s)
-----------------

#2061

Voting History
--------------

+1 from all PSC members (FrankW, DanielM, HowardB, TamasS, AndreyK)
