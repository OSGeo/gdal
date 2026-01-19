.. option:: --upsert

    .. versionadded:: 3.12

    Variant of :option:`--append` where the :cpp:func:`OGRLayer::UpsertFeature`
    operation is used to insert or update features instead of appending with
    :cpp:func:`OGRLayer::CreateFeature`.

    This is currently implemented only in a few drivers:
    :ref:`vector.gpkg`, :ref:`vector.elasticsearch` and :ref:`vector.mongodbv3`
    (drivers that implement upsert expose the :c:macro:`GDAL_DCAP_UPSERT`
    capability).

    The upsert operation uses the FID of the input feature, when it is set
    (and the FID column name is not the empty string),
    as the key to update existing features. It is crucial to make sure that
    the FID in the source and target layers are consistent.

    For the GPKG driver, it is also possible to upsert features whose FID is unset
    or non-significant (the ``--unset-fid`` option of :ref:`gdal_vector_edit`
    can be used to ignore the FID from the source feature), when there is a
    UNIQUE column that is not the integer primary key.

