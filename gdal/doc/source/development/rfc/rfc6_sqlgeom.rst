.. _rfc-6:

=======================================================================================
RFC 6: Geometry and Feature Style as OGR Special Fields
=======================================================================================

Author: Tamas Szekeres

Contact: szekerest@gmail.com

Status: Adopted

Summary
-------

This proposal addresses and issue have been discovered long ago, and OGR
provides no equivalent solution so far.

Some of the supported formats like Mapinfo.tab may contain multiple
geometry types and style information. In order to handle this kind of
data sources properly a support for selecting the layers by geometry
type or by the style info would be highly required. For more details see
the following MapServer related bugs later in this document.

All of the proposed changes can be found at the tracking bug of this RFC
referenced later in this document.

Main concepts
-------------

The most reasonable way to support this feature is to extend the
currently existing 'special field' approach to allow specifying more
than one fields. Along with the already defined 'FID' field we will add
the following ones:

-  'OGR_GEOMETRY' containing the geometry type like 'POINT' or
   'POLYGON'.
-  'OGR_STYLE' containing the style string.
-  'OGR_GEOM_WKT' containing the full WKT of the geometry.

By providing the aforementioned fields one can make for example the
following selections:

-  select FID, OGR_GEOMETRY, OGR_STYLE, OGR_GEOM_WKT, \* from MyTable
   where OGR_GEOMETRY='POINT' OR OGR_GEOMETRY='POLYGON'
-  select FID, OGR_GEOMETRY, OGR_STYLE, OGR_GEOM_WKT, \* from MyTable
   where OGR_STYLE LIKE '%BRUSH%'
-  select FID, OGR_GEOMETRY, OGR_STYLE, OGR_GEOM_WKT, \* from MyTable
   where OGR_GEOM_WKT LIKE 'POLYGON%'
-  select distinct OGR_GEOMETRY from MyTable order by OGR_GEOMETRY desc

Implementation
--------------

There are two distinct areas where this feature plays a role

-  Feature query implemented at ogrfeaturequery.cpp

-  SQL based selection implemented at ogr_gensql.cpp and
   ogrdatasource.cpp

To specify arbitrary number of special fields we will declare an array
for the field names and types in ogrfeaturequery.cpp as

::

   char* SpecialFieldNames[SPECIAL_FIELD_COUNT] 
       = {"FID", "OGR_GEOMETRY", "OGR_STYLE", "OGR_GEOM_WKT"};
   swq_field_type SpecialFieldTypes[SPECIAL_FIELD_COUNT] 
       = {SWQ_INTEGER, SWQ_STRING, SWQ_STRING, SWQ_STRING};

So as to make this array accessible to the other files the followings
will be added to ogr_p.h

::

   CPL_C_START
   #include "ogr_swq.h"
   CPL_C_END

   #define SPF_FID 0
   #define SPF_OGR_GEOMETRY 1
   #define SPF_OGR_STYLE 2
   #define SPF_OGR_GEOM_WKT 3
   #define SPECIAL_FIELD_COUNT 4

   extern char* SpecialFieldNames[SPECIAL_FIELD_COUNT];
   extern swq_field_type SpecialFieldTypes[SPECIAL_FIELD_COUNT];

In ogrfeature.cpp the field accessor functions (GetFieldAsString,
GetFieldAsInteger, GetFieldAsDouble) will be modified providing the
values of the special fields by the field index

The following code will be added to the beginning of
OGRFeature::GetFieldAsInteger:

::

   int iSpecialField = iField - poDefn->GetFieldCount();
   if (iSpecialField >= 0)
   {
   // special field value accessors
       switch (iSpecialField)
       {
       case SPF_FID:
           return GetFID();
       default:
           return 0;
       }
   }

The following code will be added to the beginning of
OGRFeature::GetFieldAsDouble:

::

   int iSpecialField = iField - poDefn->GetFieldCount();
   if (iSpecialField >= 0)
   {
   // special field value accessors
       switch (iSpecialField)
       {
       case SPF_FID:
           return GetFID();
       default:
           return 0.0;
       }
   }

The following code will be added to the beginning of
OGRFeature::GetFieldAsString:

::

   int iSpecialField = iField - poDefn->GetFieldCount();
   if (iSpecialField >= 0)
   {
   // special field value accessors
       switch (iSpecialField)
       {
       case SPF_FID:
           sprintf( szTempBuffer, "%d", GetFID() );
           return m_pszTmpFieldValue = CPLStrdup( szTempBuffer );
       case SPF_OGR_GEOMETRY:
           return poGeometry->getGeometryName();
       case SPF_OGR_STYLE:
           return GetStyleString();
       case SPF_OGR_GEOM_WKT:
           {
               if (poGeometry->exportToWkt( &m_pszTmpFieldValue ) == OGRERR_NONE )
                   return m_pszTmpFieldValue;
               else
                   return "";
           }
       default:
           return "";
       }
   }

The current implementation of OGRFeature::GetFieldAsString uses a static
string to hold the const char\* return value that is highly avoidable
and makes the code thread unsafe. In this regard the 'static char
szTempBuffer[80]' will be changed to non static and a new member will be
added to OGRFeature in ogrfeature.h as:

::

   char * m_pszTmpFieldValue; 

This member will be initialized to NULL at the constructor, and will be
freed using CPLFree() at the destructor of OGRFeature.

In OGRFeature::GetFieldAsString all of the occurrences of 'return
szTempBuffer;' will be changed to 'return m_pszTmpFieldValue =
CPLStrdup( szTempBuffer );'

OGRFeature::GetFieldAsString is responsible to destroy the old value of
m_pszTmpFieldValue at the beginning of the function:

::

   CPLFree(m_pszTmpFieldValue);
   m_pszTmpFieldValue = NULL; 

In ogrfeaturequery.cpp we should change OGRFeatureQuery::Compile to add
the special fields like:

::

   iField = 0;
   while (iField < SPECIAL_FIELD_COUNT)
   {
       papszFieldNames[poDefn->GetFieldCount() + iField] = SpecialFieldNames[iField];
       paeFieldTypes[poDefn->GetFieldCount() + iField] = SpecialFieldTypes[iField];
       ++iField;
   }

In ogrfeaturequery.cpp OGRFeatureQueryEvaluator() should be modifyed
according to the field specific actions like

::

   int iSpecialField = op->field_index - poFeature->GetDefnRef()->GetFieldCount();
   if( iSpecialField >= 0 )
   {
       if ( iSpecialField < SPECIAL_FIELD_COUNT )
       {
           switch ( SpecialFieldTypes[iSpecialField] )
           {
           case SWQ_INTEGER:
               sField.Integer = poFeature->GetFieldAsInteger( op->field_index );
           case SWQ_STRING:
               sField.String = (char*) poFeature->GetFieldAsString( op->field_index );
           }      
       }
       else
       {
           CPLDebug( "OGRFeatureQuery", "Illegal special field index.");
           return FALSE;
       }
       psField = &sField;
   }
   else
       psField = poFeature->GetRawFieldRef( op->field_index );

In ogrfeaturequery.cpp OGRFeatureQuery::FieldCollector should be
modifyed to add the field names like:

::

   if( op->field_index >= poTargetDefn->GetFieldCount()
           && op->field_index < poTargetDefn->GetFieldCount() + SPECIAL_FIELD_COUNT) 
           pszFieldName = SpecialFieldNames[op->field_index];

In ogrdatasource.cpp ExecuteSQL() will allocate the arrays according to
the number of the special fields:

::

   sFieldList.names = (char **) 
           CPLMalloc( sizeof(char *) * (nFieldCount+SPECIAL_FIELD_COUNT) );
   sFieldList.types = (swq_field_type *)  
           CPLMalloc( sizeof(swq_field_type) * (nFieldCount+SPECIAL_FIELD_COUNT) );
   sFieldList.table_ids = (int *) 
           CPLMalloc( sizeof(int) * (nFieldCount+SPECIAL_FIELD_COUNT) );
   sFieldList.ids = (int *) 
           CPLMalloc( sizeof(int) * (nFieldCount+SPECIAL_FIELD_COUNT) );

And the fields will be added as

::

   for (iField = 0; iField < SPECIAL_FIELD_COUNT; iField++)
   {
       sFieldList.names[sFieldList.count] = SpecialFieldNames[iField];
       sFieldList.types[sFieldList.count] = SpecialFieldTypes[iField];
       sFieldList.table_ids[sFieldList.count] = 0;
       sFieldList.ids[sFieldList.count] = nFIDIndex + iField;
       sFieldList.count++;
   }

For supporting the SQL based queries we should also modify the
constructor of OGRGenSQLResultsLayer in ogr_gensql.cpp and set the field
type properly:

::

   else if ( psColDef->field_index >= iFIDFieldIndex )
   {
       switch ( SpecialFieldTypes[psColDef->field_index - iFIDFieldIndex] )
       {
       case SWQ_INTEGER:
           oFDefn.SetType( OFTInteger );
           break;
       case SWQ_STRING:
           oFDefn.SetType( OFTString );
           break;
       case SWQ_FLOAT:
           oFDefn.SetType( OFTReal );
           break;
       }
   }

Some of the queries will require to modify
OGRGenSQLResultsLayer::PrepareSummary in ogr_gensql.cpp will be
simplified (GetFieldAsString will be used in all cases to access the
field values):

::

   pszError = swq_select_summarize( psSelectInfo, iField, 
   poSrcFeature->GetFieldAsString( psColDef->field_index ) );

OGRGenSQLResultsLayer::TranslateFeature should also be modifyed when
copying the fields from primary record to the destination feature

::

    if ( psColDef->field_index >= iFIDFieldIndex &&
               psColDef->field_index < iFIDFieldIndex + SPECIAL_FIELD_COUNT )
   {
       switch (SpecialFieldTypes[psColDef->field_index - iFIDFieldIndex])
       {
       case SWQ_INTEGER:
           poDstFeat->SetField( iField, poSrcFeat->GetFieldAsInteger(psColDef->field_index) );
       case SWQ_STRING:
           poDstFeat->SetField( iField, poSrcFeat->GetFieldAsString(psColDef->field_index) );
       }
   }

For supporting the 'order by' queries we should also modify
OGRGenSQLResultsLayer::CreateOrderByIndex() as:

::


   if ( psKeyDef->field_index >= iFIDFieldIndex)
   {
       if ( psKeyDef->field_index < iFIDFieldIndex + SPECIAL_FIELD_COUNT )
       {
           switch (SpecialFieldTypes[psKeyDef->field_index - iFIDFieldIndex])
           {
           case SWQ_INTEGER:
               psDstField->Integer = poSrcFeat->GetFieldAsInteger(psKeyDef->field_index);
           case SWQ_STRING:
               psDstField->String = CPLStrdup( poSrcFeat->GetFieldAsString(psKeyDef->field_index) );
           }
       }
       continue;
   }

All of the strings allocated previously should be deallocated later in
the same function as:

::


   if ( psKeyDef->field_index >= iFIDFieldIndex )
   {
       /* warning: only special fields of type string should be deallocated */
       if (SpecialFieldTypes[psKeyDef->field_index - iFIDFieldIndex] == SWQ_STRING)
       {
           for( i = 0; i < nIndexSize; i++ )
           {
               OGRField *psField = pasIndexFields + iKey + i * nOrderItems;
               CPLFree( psField->String );
           }
       }
       continue;
   }

When ordering by the field values the OGRGenSQLResultsLayer::Compare
should also be modifyed:

::

   if( psKeyDef->field_index >= iFIDFieldIndex )
       poFDefn = NULL;
   else
       poFDefn = poSrcLayer->GetLayerDefn()->GetFieldDefn( 
           psKeyDef->field_index );

   if( (pasFirstTuple[iKey].Set.nMarker1 == OGRUnsetMarker 
           && pasFirstTuple[iKey].Set.nMarker2 == OGRUnsetMarker)
       || (pasSecondTuple[iKey].Set.nMarker1 == OGRUnsetMarker 
           && pasSecondTuple[iKey].Set.nMarker2 == OGRUnsetMarker) )
       nResult = 0;
   else if ( poFDefn == NULL )
   {
       switch (SpecialFieldTypes[psKeyDef->field_index - iFIDFieldIndex])
       {
       case SWQ_INTEGER:
           if( pasFirstTuple[iKey].Integer < pasSecondTuple[iKey].Integer )
               nResult = -1;
           else if( pasFirstTuple[iKey].Integer > pasSecondTuple[iKey].Integer )
               nResult = 1;
           break;
       case SWQ_STRING:
           nResult = strcmp(pasFirstTuple[iKey].String,
                           pasSecondTuple[iKey].String);
           break;
       }
   }

Adding New Special Fields
-------------------------

Adding a new special field in a subsequent development phase is fairly
straightforward and the following steps should be made:

1. In ogr_p.h a new constant should be added with the value of the
   SPECIAL_FIELD_COUNT and SPECIAL_FIELD_COUNT should be incremented by
   one.

2. In ogrfeaturequery.cpp the special field string and the type should
   be added to SpecialFieldNames and SpecialFieldTypes respectively

3. The field value accessors (OGRFeature::GetFieldAsString,
   OGRFeature::GetFieldAsInteger, OGRFeature::GetFieldAsDouble) should
   be modifyed to provide the value of the new special field. All of
   these functions provide const return values so GetFieldAsString
   should retain the value in the m_pszTmpFieldValue member.

4. When adding a new value with a type other than SWQ_INTEGER and
   SWQ_STRING the following functions might also be modified
   accordingly:

-  OGRGenSQLResultsLayer::OGRGenSQLResultsLayer
-  OGRGenSQLResultsLayer::TranslateFeature
-  OGRGenSQLResultsLayer::CreateOrderByIndex
-  OGRGenSQLResultsLayer::Compare
-  OGRFeatureQueryEvaluator

Backward Compatibility
----------------------

In most cases the backward compatibility of the OGR library will be
retained. However the special fields will potentially conflict with
regard fields with the given names. When accessing the field values the
special fields will take pecedence over the other fields with the same
names.

When using OGRFeature::GetFieldAsString the returned value will be
stored as a member variable instead of a static variable. The string
will be deallocated and will no longer be usable after the destruction
of the feature.

Regression Testing
------------------

A new gdalautotest/ogr/ogr_sqlspecials.py script to test support for all
special fields in the ExecuteSQL() call and with WHERE clauses.

Documentation
-------------

The OGR SQL document will be updated to reflect the support for special
fields.

Implementation Staffing
-----------------------

Tamas Szekeres will implement the bulk of the RFC in time for GDAL/OGR
1.4.0.

Frank Warmerdam will consider how the backward compatibility issues
(with special regard to the modified lifespan of the GetFieldAsString
returned value) will affect the other parts of the OGR project and will
write the Python regression testing script.

References
----------

-  Tracking bug for this feature (containing all of the proposed code
   changes): #1333

-  MapServer related bugs:

   -  `1129 <http://trac.osgeo.org/mapserver/ticket/1129>`__
   -  `1438 <http://trac.osgeo.org/mapserver/ticket/1438>`__

Voting History
--------------

Frank Warmerdam +1

Daniel Morissette +1

Howard Butler +0

Andrey Kiselev +1
