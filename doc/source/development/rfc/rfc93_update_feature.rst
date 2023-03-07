.. _rfc-93:

=============================================================
RFC 93: OGRLayer::UpdateFeature() method
=============================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2023-Feb-15
Status:        Adopted, implemented
Target:        GDAL 3.7
============== =============================================

Summary
-------

This RFC adds a new method in the OGRLayer class, UpdateFeature(), to be
able to update a subset of the attributes and geometry fields of a feature.

Motivation
----------

There are currently 2 ways of updating (in a broad sense) existing features in
drivers that support editing an existing feature:

- through the SetFeature() method, which has roughly a Replace semantics. All
  the fields of the passed feature are used to update the feature
- through the UpsertFeature() method. UpsertFeature() semantics is: "if the
  passed feature doesn't exist, considering its feature id, then create it,
  otherwise replace it with the existing feature". So UpsertFeature() is
  conceptually CreateFeature() or UpdateFeature().

However a number of use cases only involve updating a subset of the fields of
a feature, and not a full replacement:

- Typically QGIS changeAttributeValues() and changeGeometryValues() currently
  have to call GetFeature() on features they want to edit, modify the desired
  fields and call SetFeature() to update the feature. This is suboptimal from
  a performance point of view. Similarly pgsql-ogr-fdw could potentially
  be enhanced to map its implementation of the UPDATE on a OGR foreign table to
  use UpdateFeature() instead of a full replacement with SetFeature().
- SQL based drivers implement SetFeature() through the SQL UPDATE statement,
  which does not require the full set of attributes to be provided. Consequently
  UpdateFeature() would be a better match with their native capability.
  SetFeature() updating all fields, even those not modified, may cause triggers
  to be unnecessarily run (for example the GeoPackage triggers related to
  updating the RTree of the spatial index)
- no-SQL based drivers such as MongoDB have separate API to replace, upsert or
  update features.
- The transactional profile of WFS 2 also distinguishes Update and Replace
  commands.

Details
-------

Similarly to CreateFeature(), SetFeature() and UpsertFeature(), two methods are
added in OGRLayer:

- UpdateFeature(), non-virtual, and aimed at being the one called by the end
  user. This method does a few sanity checks on the passed arguments (the
  arrays containing the indices of the attributes to updated) and convert
  curve geometries to linear ones if needed for layers that do not support
  curve geometries (similarly as CreateFeature(), SetFeature() and
  UpdateFeature())

- IUpdateFeature(), virtual method implemented by drivers. A default
  implementation is provided in the base OGRLayer class for drivers that
  expose the OLCRandomWrite capability. The default implementation calls
  GetFeature() to retrieve the current version of the feature, updates it
  with the feature passed as argument to IUpdateFeature() and calls
  ISetFeature() to replace the feature.

The prototype of UpdateFeature() is the following one:

.. code-block:: cpp

     /**
     \brief Update (part of) an existing feature.

     This method will update the specified attribute and geometry fields of a
     feature to the layer, based on the feature id within the OGRFeature.

     Use OGRLayer::TestCapability(OLCRandomWrite) to establish if this layer
     supports random access writing via UpdateFeature(). And to know if the
     driver supports a dedicated/efficient UpdateFeature() method, test for the
     OLCUpdateFeature capability.

     The way unset fields in the provided poFeature are processed is driver dependent:
     <ul>
     <li>
     SQL based drivers which implement SetFeature() through SQL UPDATE will skip
     unset fields, and thus the content of the existing feature will be preserved.
     </li>
     <li>
     The shapefile driver will write a NULL value in the DBF file.
     </li>
     <li>
     The GeoJSON driver will take into account unset fields to remove the corresponding
     JSON member.
     </li>
     </ul>

     This method is the same as the C function OGR_L_UpdateFeature().

     To fully replace a feature, see OGRLayer::SetFeature().

     Note that after this call the content of hFeat might have changed, and will
     *not* reflect the content you would get with GetFeature().
     In particular for performance reasons, passed geometries might have been "stolen",
     in particular for the default implementation of UpdateFeature() which relies
     on GetFeature() + SetFeature().

     @param poFeature the feature to update.

     @param nUpdatedFieldsCount number of attribute fields to update. May be 0

     @param panUpdatedFieldsIdx array of nUpdatedFieldsCount values, each between
                                0 and GetLayerDefn()->GetFieldCount() - 1, indicating
                                which fields of poFeature must be updated in the
                                layer.

     @param nUpdatedGeomFieldsCount number of geometry fields to update. May be 0

     @param panUpdatedGeomFieldsIdx array of nUpdatedGeomFieldsCount values, each between
                                    0 and GetLayerDefn()->GetGeomFieldCount() - 1, indicating
                                    which geometry fields of poFeature must be updated in the
                                    layer.

     @param bUpdateStyleString whether the feature style string in the layer should
                               be updated with the one of poFeature.

     @return OGRERR_NONE if the operation works, otherwise an appropriate error
     code (e.g OGRERR_NON_EXISTING_FEATURE if the feature does not exist).
     */

     OGRErr OGRLayer::UpdateFeature( OGRFeature * poFeature,
                                     int nUpdatedFieldsCount,
                                     const int *panUpdatedFieldsIdx,
                                     int nUpdatedGeomFieldsCount,
                                     const int *panUpdatedGeomFieldsIdx,
                                     bool bUpdateStyleString );


The corresponding C function ``OGR_L_UpdateFeature`` is added.

The OGRLayerDecorator, OGRLayerPool, OGRMutexedLayer, OGRUnionLayer,
OGRWarpedLayer and OGREditableLayer utility classes that wrap other layers
are modified to take the new IUpdateFeature() virtual method into account.

Impacted drivers
----------------

The proposed implementation adds a dedicated implementation of IUpdateFeature()
in the following drivers: memory driver, PostgreSQL, GPKG, MongoDB.

Other drivers could potentially benefit from it, e.g Shapefile, where separate
updates of the .shp (geometries) and .dbf (attributes) files. But not in scope
of the initial implementation.

Design choices
--------------

- Q: Why is it necessary to specify which fields of the feature passed to
  UpdateFeature() should be taken into account?

- A: For attributes, it could have been possible to rely on the set/unset status
  of the fields, but for drivers that distinguish unset from NULL (typically
  all JSON based drivers, or no-SQL driver which map to key/value documents with
  a non-fixed schema), this would have make it impossible to unset a field in
  the stored feature. And geometry fields don't have a unset status, so it would
  be otherwise be impossible to distinguish between setting the geometry to NULL
  or not modifying the existing geometry.

- Q: Why having a default implementation of IUpdateFeature() in the OGRLayer base
  class and not just returning OGRERR_UNSUPPORTED_OPERATION ?

- A: The rationale is to make it easier for code to use UpdateFeature(), even
  when the underlying driver does not have a specialized implementation. User
  code that needs to know if a specialized implementation is available (in particular
  if atomicity of changes is a requirement) can test the OLCUpdateFeature
  capability (UpdateFeature() is available as soon as the OLCRandomWrite
  capability is advertized).

- Q: Is it appropriate that IUpdateFeature() implementations may alter the
  feature passed to them ?

- A: given that the purpose of UpdateFeature() is to only require the modified
  fields to be specified, the feature generally cannot be used to reflect the
  full state of the corresponding stored object. Consequently modifying it
  has no anticipated drawbacks for intended use cases.

Bindings
--------

OGR_L_UpdateFeature() is mapped to SWIG's ogr.Feature.UpdateFeature().

Benchmark
---------

A benchmark using the below Python script has been run to compare updating
a single field on a GeoPackage layer with 3.2 million features, each with 13
attributes and small polygons, of a total size of 1.6 GB.

The runtime using the new UpdateFeature() method is 38.5 seconds, versus 168
seconds using GetFeature() + SetFeature(), hence a x4.4 speed-up.
The speed-up should generally increase with the number of attributes per feature.

This is a good simulation of the benefit QGIS changeAttributeValues() implementation
could get by using UpdateFeature().

.. code-block:: python

    from osgeo import ogr

    # Set to False to use SetFeature()
    use_update_feature = True

    ds = ogr.Open("test.gpkg", update=1)
    lyr = ds.GetLayer(0)
    field_idx = lyr.GetLayerDefn().GetFieldIndex("capture_source_id")

    lyr.StartTransaction()
    if use_update_feature:
        f = ogr.Feature(lyr.GetLayerDefn())
        sql_lyr = ds.ExecuteSQL("SELECT %s FROM \"%s\"" %(lyr.GetFIDColumn(), lyr.GetName()))
        fids = [f.GetFID() for f in sql_lyr]
        ds.ReleaseResultSet(sql_lyr)
        for idx, fid in enumerate(fids):
            if (idx % 10000) == 0:
                print(idx)
            f.SetFID(fid)
            f.SetField(field_idx, 1)
            assert lyr.UpdateFeature(f, [field_idx], [], False) == ogr.OGRERR_NONE
    else:
        for idx, f in enumerate(lyr):
            if (idx % 10000) == 0:
                print(idx)
            f.SetField(field_idx, 1)
            assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    lyr.CommitTransaction()

Backward compatibility
----------------------

None, new functionality.

C++ ABI breakage due to a new virtual method, typical of functionality introduced
in GDAL minor versions.

Documentation
-------------

The new functions and methods are documented.

Testing
-------

The API, base implementation and dedicated UpdateFeature() are tested by
new autotest checks.

Related issues and PRs
----------------------

- https://github.com/OSGeo/gdal/issues/6544: Extend OGR API SetFeature to
  control Replace vs Update behavior

- https://github.com/qgis/QGIS/issues/46355: Saving a large edited point layer
  takes far too long

- Proposed implementation in
  https://github.com/OSGeo/gdal/compare/master...rouault:gdal:update_feature?expand=1

Voting history
--------------

+1 from PSC members Jukka, MateuszL and EvenR
