.. _rfc-97:

===================================================================
RFC 97: OGRFeatureDefn, OGRFieldDefn and OGRGeomFieldDefn "sealing"
===================================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2023-Nov-16
Status:        Adopted, implemented
Target:        GDAL 3.9
============== =============================================

Summary
-------

This RFC aims at avoiding common misuse of the setter methods of the
OGRFeatureDefn, OGRFieldDefn and OGRGeomFieldDefn classes. Indeed, the setter
methods of those classes should not be used directly by user code (that is
non driver implementations), on instances that are owned by a OGRLayer. It is
quite frequent for users (even seasoned ones) to neglect that constraint. Hence
this RFC introduces an optional "sealing" capability that drivers can enable to
avoid users modifying instances that they should not.

Motivation
----------

Let's take an example to clarify. We want to prevent a user from doing the
following:

.. code-block:: c++

    poLayer->GetLayerDefn()->GetFieldDefn(iFieldIdx)->SetName("new_name");

The above code will not raise any warning or error at runtime, but will not
change the underlying on-disk dataset to reflect the new field name.
Indeed poLayer->GetLayerDefn() and its child objects should be considered
immutable, unless using dedicated methods of OGRLayer to modify them.
The correct way of renaming an existing field is (for drivers that support such
capability):

.. code-block:: c++

    OGRFieldDefn oRenamedField("new_name", poLayer->GetLayerDefn()->GetFieldDefn(iFieldIdx)->GetType());
    poLayer->AlterFieldDefn(iFieldIdx, &oRenamedField, ALTER_NAME_FLAG);

For other operations, such as calling :cpp:func:`OGRFieldDefn::AddFieldDefn()`
or :cpp:func:`OGRFieldDefn::DeleteFieldDefn()` on an instance of OGRLayer::GetLayerDefn(),
crashes could potentially occur in drivers that are not ready to see the number
of fields to change behind their back. The correct way of adding or deleting
fields to a layer is to use :cpp:func:`OGRLayer::CreateField()` or
:cpp:func:`OGRLayer::DeleteField()`

Details
-------

A ``bool m_bSealed`` member variable is added to the
:cpp:class:`OGRFieldDefn` and :cpp:class:`OGRGeomFieldDefn` classes. Its default
value is false, meaning that calling setters method on instances of those classes
is allowed by default.

The following methods are added on :cpp:class:`OGRFieldDefn` (and similarly
for :cpp:class:`OGRGeomFieldDefn`)

.. code-block:: c++

    /** Seal a OGRFieldDefn.
     *
     * A sealed OGRFieldDefn can not be modified while it is sealed.
     *
     * This method should only be called by driver implementations.
     *
     * @since GDAL 3.9
     */
    void OGRFieldDefn::Seal()
    {
        m_bSealed = true;
    }

    /** Unseal a OGRFieldDefn.
     *
     * Undo OGRFieldDefn::Seal()
     *
     * Using GetTemporaryUnsealer() is recommended for most use cases.
     *
     * This method should only be called by driver implementations.
     *
     * @since GDAL 3.9
     */
    void OGRFieldDefn::Unseal()
    {
        m_bSealed = false;
    }


All setter methods of those classes are modified to check the value of
``m_bSealed``. If it is set, a CE_Failure CPLError() is emitted indicating that
the object is sealed. Unfortunately most setters return ``void``, so there is
no way to advertise the error through an error code. However, when using the
Python bindings with exceptions enabled, a Python exception will be thrown.

A convenience method is also offered to use the Resource Acquisition Is Initialization (RAII)
paradygm to temporary unseal an instance, which is an operation that drivers
implementing AlterFieldDefn() / AlterGeomFieldDefn() will need to do on fields
they have priorly sealed.

.. code-block:: c++

    /** Return an object that temporary unseals the OGRFieldDefn
     *
     * The returned object calls Unseal() initially, and when it is destroyed
     * it calls Seal().
     *
     * This method should only be called by driver implementations.
     *
     * @since GDAL 3.9
     */
    OGRFieldDefn::TemporaryUnsealer OGRFieldDefn::GetTemporaryUnsealer()
    {
        return TemporaryUnsealer(this);
    }

Typical usage is by AlterFieldDefn() / AlterGeomFieldDefn() is:

.. code-block:: c++

    OGRErr OGRPGTableLayer::AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn, int nFlagsIn)
    {
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
        auto oTemporaryUnsealer(poFieldDefn->GetTemporaryUnsealer());
        // modify poFieldDefn to reflect changed properties of poNewFieldDefn
        // according to nFlagsIn.
        ...
    }


For punctual changes, a convenience ``whileUnsealing`` function is also
provided.

It can be used as in the following:

.. code-block:: c++

    whileUnsealing(poFieldDefn)->SetType(eNewType);


For OGRFeatureDefn, similar changes are done but with an extra subtelty.
For convenience of drivers, we want a driver to be able to call GetTemporaryUnsealer()
in a nested way, where only the first/most external call does something, and
next/nested ones are a no-operation. This is similar to using a recursive mutex
from the same thread. The user can also indicate if it wishes fields and geometry
fields owned by the OGRFeatureDefn to be sealed/unsealed at the same time.

.. code-block:: c++

    /** Seal a OGRFeatureDefn.
     *
     * A sealed OGRFeatureDefn can not be modified while it is sealed.
     *
     * This method also call OGRFieldDefn::Seal() and OGRGeomFieldDefn::Seal()
     * on its fields and geometry fields.
     *
     * This method should only be called by driver implementations.
     *
     * @param bSealFields Whether fields and geometry fields should be sealed.
     *                    This is generally desirabled, but in case of deferred
     *                    resolution of them, this parameter should be set to false.
     * @since GDAL 3.9
     */
    void OGRFeatureDefn::Seal(bool bSealFields);


    /** Unseal a OGRFeatureDefn.
     *
     * Undo OGRFeatureDefn::Seal()
     *
     * This method also call OGRFieldDefn::Unseal() and OGRGeomFieldDefn::Unseal()
     * on its fields and geometry fields.
     *
     * Using GetTemporaryUnsealer() is recommended for most use cases.
     *
     * This method should only be called by driver implementations.
     *
     * @param bUnsealFields Whether fields and geometry fields should be unsealed.
     *                      This is generally desirabled, but in case of deferred
     *                      resolution of them, this parameter should be set to
     * false.
     * @since GDAL 3.9
     */
    void OGRFeatureDefn::Unseal(bool bUnsealFields);

    /** Return an object that temporary unseals the OGRFeatureDefn
     *
     * The returned object calls Unseal() initially, and when it is destroyed
     * it calls Seal().
     * This method should be called on a OGRFeatureDefn that has been sealed
     * previously.
     * GetTemporaryUnsealer() calls may be nested, in which case only the first
     * one has an effect (similarly to a recursive mutex locked in a nested way
     * from the same thread).
     *
     * This method should only be called by driver implementations.
     *
     * @param bSealFields Whether fields and geometry fields should be unsealed and
     *                    resealed.
     *                    This is generally desirabled, but in case of deferred
     *                    resolution of them, this parameter should be set to false.
     * @since GDAL 3.9
     */
    OGRFeatureDefn::TemporaryUnsealer
    OGRFeatureDefn::GetTemporaryUnsealer(bool bSealFields = true);


For punctual changes, a convenience ``whileUnsealing`` function is also
provided.

In practice, the only Seal() invocation in driver core should be done on
the OGRFeatureDefn instance they return with GetLayerDefn(). All subsequent
sealing/unsealing operations should be done through
OGRFeatureDefn::GetTemporaryUnsealer()

Example of a typical driver
---------------------------

Constructor of the OGRLayer subclass:

.. code-block:: c++

    OGRMyLayer::OGRMyLayer(...)
    {
        m_poFeatureDefn = new OGRFeatureDefn("layer_name");
        m_poFeatureDefn->Reference();
        SetDescription(m_poFeatureDefn->GetName());
        ... add fields with m_poFeatureDefn->AddFieldDefn() ...
        m_poFeatureDefn->Seal(true);
    }


Simple CreateField() implementation:

.. code-block:: c++

    OGRErr OGRMyLayer::CreateField(OGRFieldDefn* poNewFieldDefn, int bApproxOK)
    {
        whileUnsealing(m_poFeatureDefn)->AddFieldDefn(poNewFieldDefn);
        return OGRERR_NONE
    }

Discussion
----------

- Why not just having a ``const OGRFeatureDefn* OGRLayer::GetLayerDefn() const``
  method ?

  That would only work when using the C++ API (and would require changes in all
  drivers to modify the signature, as well as doing changes at places where
  drivers require a non-const OGRFeatureDefn*), because const correctness is not
  available in the C API and the SWIG bindings.

SWIG bindings
-------------

No impact. Those C++ methods are intended to be used by driver implementation
only.

Updated drivers
---------------

For the initial implementation, the following drivers are updated to seal their
layer definition:
GeoPackage, PostgreSQL, Shapefile, OpenFileGDB, MITAB, Memory, GeoJSON, JSONFG,
TopoJSON, ESRIJSON, ODS, XLSX.

Backward compatibility
----------------------

C API is unchanged. Backwards compatible addition to the C++ API (ABI change)

There is the possibility to break user code that mis-used the API. For example,
this was the case of a few tests in the autotest suite that have had to be
modified.

MIGRATION_GUIDE.TXT will mention that and point to this RFC.

Risks
-----

Drivers that implement sealing should make sure they unseal at the appropriate
places: OGRLayer::Rename(), CreateField(), DeleteField(), CreateGeomField(),
DeleteGeomField(), ReorderFields(), AlterFieldDefn() AlterGeomFieldDefn()
and any other places where they might modify objects.
Failure to do so will result in failures, and potentially crashes. Hence
implementation of sealing should only be done on drivers that have sufficient
test coverage.

Documentation
-------------

The documentation of the setters as well as the introduction text of
OGRFeatureDefn, OGRFieldDefn and OGRGeomFieldDefn is modified to reflect that
setters of those classes should not be called on instances returned by
OGRLayer::GetLayerDefn().

Testing
-------

- The autotest suite is modified to comply with sealing
- Calls to setters on sealed instances will be done to test that an error
  is triggered.

Related issues and PRs
----------------------

Candidate implementation in https://github.com/OSGeo/gdal/pull/8733

Voting history
--------------

+1 from PSC members JukkaR, JavierJS and EvenR
