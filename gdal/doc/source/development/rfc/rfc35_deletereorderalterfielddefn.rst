.. _rfc-35:

================================================================================
RFC 35: Delete, reorder and alter field definitions of OGR layers
================================================================================

Authors: Even Rouault

Contact: even dot rouault at spatialys.com

Status: Adopted

Summary
-------

This document proposes changes in OGR to add the capability to delete
fields, reorder fields and alter field definitions, in OGR layer
definitions.

Rationale
---------

Currently, an OGR layer definition can only be altered to add a new
field definition with OGRLayer::CreateField().

It is desirable to extend OGR capabilities to be able to delete, reorder
and alter field definitions of existing layers. Such wish has been
expressed in ticket #2671 and comes back regularly on QGIS mailing list
(e.g.
`http://lists.osgeo.org/pipermail/qgis-user/2011-May/011935.html <http://lists.osgeo.org/pipermail/qgis-user/2011-May/011935.html>`__).
QGIS currently has a "Table Manager" extension to work around the lack
of DeleteField(), so a proper solution is clearly needed.

Planned Changes
---------------

The OGRLayer class will be extended with the following methods :

::

       virtual OGRErr      DeleteField( int iField );
       virtual OGRErr      ReorderFields( int* panMap );
       virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags );

       /* non virtual : conveniency wrapper for ReorderFields() */
       OGRErr              ReorderField( int iOldFieldPos, int iNewFieldPos );

The documentation of those new methods is :

::


   /**
   \fn OGRErr OGRLayer::DeleteField( int iField );

   \brief Delete an existing field on a layer.

   You must use this to delete existing fields
   on a real layer. Internally the OGRFeatureDefn for the layer will be updated
   to reflect the deleted field.  Applications should never modify the OGRFeatureDefn
   used by a layer directly.

   This method should not be called while there are feature objects in existence that
   were obtained or created with the previous layer definition.

   Not all drivers support this method. You can query a layer to check if it supports it
   with the OLCDeleteField capability. Some drivers may only support this method while
   there are still no features in the layer. When it is supported, the existings features of the
   backing file/database should be updated accordingly.

   This function is the same as the C function OGR_L_DeleteField().

   @param iField index of the field to delete.

   @return OGRERR_NONE on success.

   @since OGR 1.9.0
   */

   /**
   \fn OGRErr OGRLayer::ReorderFields( int* panMap );

   \brief Reorder all the fields of a layer.

   You must use this to reorder existing fields
   on a real layer. Internally the OGRFeatureDefn for the layer will be updated
   to reflect the reordering of the fields.  Applications should never modify the OGRFeatureDefn
   used by a layer directly.

   This method should not be called while there are feature objects in existence that
   were obtained or created with the previous layer definition.

   panMap is such that,for each field definition at position i after reordering,
   its position before reordering was panMap[i].

   For example, let suppose the fields were "0","1","2","3","4" initially.
   ReorderFields([0,2,3,1,4]) will reorder them as "0","2","3","1","4".

   Not all drivers support this method. You can query a layer to check if it supports it
   with the OLCReorderFields capability. Some drivers may only support this method while
   there are still no features in the layer. When it is supported, the existings features of the
   backing file/database should be updated accordingly.

   This function is the same as the C function OGR_L_ReorderFields().

   @param panMap an array of GetLayerDefn()->GetFieldCount() elements which
   is a permutation of [0, GetLayerDefn()->GetFieldCount()-1].

   @return OGRERR_NONE on success.

   @since OGR 1.9.0
   */

   /**
   \fn OGRErr OGRLayer::ReorderField( int iOldFieldPos, int iNewFieldPos );

   \brief Reorder an existing field on a layer.

   This method is a conveniency wrapper of ReorderFields() dedicated to move a single field.
   It is a non-virtual method, so drivers should implement ReorderFields() instead.

   You must use this to reorder existing fields
   on a real layer. Internally the OGRFeatureDefn for the layer will be updated
   to reflect the reordering of the fields.  Applications should never modify the OGRFeatureDefn
   used by a layer directly.

   This method should not be called while there are feature objects in existence that
   were obtained or created with the previous layer definition.

   The field definition that was at initial position iOldFieldPos will be moved at
   position iNewFieldPos, and elements between will be shuffled accordingly.

   For example, let suppose the fields were "0","1","2","3","4" initially.
   ReorderField(1, 3) will reorder them as "0","2","3","1","4".

   Not all drivers support this method. You can query a layer to check if it supports it
   with the OLCReorderFields capability. Some drivers may only support this method while
   there are still no features in the layer. When it is supported, the existings features of the
   backing file/database should be updated accordingly.

   This function is the same as the C function OGR_L_ReorderField().

   @param iOldFieldPos previous position of the field to move. Must be in the range [0,GetFieldCount()-1].
   @param iNewFieldPos new position of the field to move. Must be in the range [0,GetFieldCount()-1].

   @return OGRERR_NONE on success.

   @since OGR 1.9.0
   */

   /**
   \fn OGRErr OGRLayer::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags );

   \brief Alter the definition of an existing field on a layer.

   You must use this to alter the definition of an existing field of a real layer.
   Internally the OGRFeatureDefn for the layer will be updated
   to reflect the altered field.  Applications should never modify the OGRFeatureDefn
   used by a layer directly.

   This method should not be called while there are feature objects in existence that
   were obtained or created with the previous layer definition.

   Not all drivers support this method. You can query a layer to check if it supports it
   with the OLCAlterFieldDefn capability. Some drivers may only support this method while
   there are still no features in the layer. When it is supported, the existings features of the
   backing file/database should be updated accordingly. Some drivers might also not support
   all update flags.

   This function is the same as the C function OGR_L_AlterFieldDefn().

   @param iField index of the field whose definition must be altered.
   @param poNewFieldDefn new field definition
   @param nFlags combination of ALTER_NAME_FLAG, ALTER_TYPE_FLAG and ALTER_WIDTH_PRECISION_FLAG
   to indicate which of the name and/or type and/or width and precision fields from the new field
   definition must be taken into account.

   @return OGRERR_NONE on success.

   @since OGR 1.9.0
   */

Three new layer capabilities are added :

::

   OLCDeleteField / "DeleteField": TRUE if this layer can delete
   existing fields on the current layer using DeleteField(), otherwise FALSE.

   OLCReorderFields / "ReorderFields": TRUE if this layer can reorder
   existing fields on the current layer using ReorderField() or ReorderFields(), otherwise FALSE.

   OLCAlterFieldDefn / "AlterFieldDefn": TRUE if this layer can alter
   the definition of an existing field on the current layer using AlterFieldDefn(), otherwise FALSE.

The new methods are mapped to the C API :

::

   OGRErr CPL_DLL OGR_L_DeleteField( OGRLayerH, int iField );
   OGRErr CPL_DLL OGR_L_ReorderFields( OGRLayerH, int* panMap );
   OGRErr CPL_DLL OGR_L_ReorderField( OGRLayerH, int iOldFieldPos, int iNewFieldPos );
   OGRErr CPL_DLL OGR_L_AlterFieldDefn( OGRLayerH, int iField, OGRFieldDefnH hNewFieldDefn, int nFlags );

For the purpose of the implementation, new methods are also added to the
OGRFeatureDefn class :

::

       OGRErr      DeleteFieldDefn( int iField );
       OGRErr      ReorderFieldDefns( int* panMap );

A OGRErr OGRCheckPermutation(int\* panPermutation, int nSize) function
is added to ogrutils.cpp to check that the array is a permutation of
[0,nSize-1]. It is used by OGRFeatureDefn::ReorderFieldDefns() and can
be used by all drivers implementing OGRLayer::ReorderFields() to
validate the panMap argument.

Altering field types
--------------------

This RFC does not attempt to guarantee which type conversions will be
possible. It will depend on the capabilities of the implementing
drivers. For example, for database drivers, the operation will be
directly done on the server side (through a 'ALTER TABLE my_table ALTER
COLUMN my_column TYPE new_type' command for the PG driver). So some
conversions might be possible, others not...

It is however expected that converting from any type to OFTString will
be supported in most cases when AlterFieldDefn() is supported.

Drivers that don't support a conversion and that were required to do it
(ALTER_TYPE_FLAG set and new_type != old_type) should emit an explicit
error.

Compatibility Issues
--------------------

None

Changed drivers
---------------

The shapefile driver will implement DeleteField(), ReorderFields() and
AlterFieldDefn(). Shapelib will be extended with DBFReorderFields() and
DBFAlterFieldDefn().

Note: The implementation of AlterFieldDefn() in the Shapefile driver
does not support altering the field type, except when converting to
OFTString. It will not reformat numeric values of existing features if
width or precision are changed. However, appropriate field truncation or
expansion will occur if the width is altered.

Other drivers, mainly database drivers (PG, MySQL, SQLite), could be
easily extended to implement the new API by issuing the appropriate SQL
command (ALTER TABLE foo DROP COLUMN bar, ALTER TABLE foo ALTER COLUMN
bar, ...). The implementation of DeleteField() and AlterFieldDefn() in
the PG driver is indeed planned, provided this RFC is adopted. The
Memory driver will also updated to support DeleteField(),
ReorderFields() and AlterFieldDefn().

SWIG bindings
-------------

DeleteField(), ReorderField(), ReorderFields() and AlterFieldDefn() will
be mapped to SWIG.

Test Suite
----------

The autotest suite will be extended to test the implementation of the
new API for the Shapefile driver. An example of the use of the new API
is attached to ticket #2671
(`rfc35_test.py <http://trac.osgeo.org/gdal/attachment/ticket/2671/rfc35_test.py>`__)
and will be turned into unit tests.

Implementation
--------------

Implementation will be done by Even Rouault in GDAL/OGR trunk. Changes
in Shapelib will need to be pushed into upstream CVS by a Shapelib
committer. The proposed implementation is attached as a patch in ticket
#2671
(`rfc35_v3.patch <http://trac.osgeo.org/gdal/attachment/ticket/2671/rfc35_v3.patch>`__).

Voting history
--------------

+1 from FrankW, DanielM, HowardB, TamasS and EvenR
