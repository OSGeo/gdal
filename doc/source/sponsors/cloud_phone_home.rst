.. _cloud_phone_home:

:orphan:

================================================================================
Cloud storage "phone home" functionality
================================================================================

What is it?
-----------

Beginning in GDAL 3.13.0, when accessing a remote object on ``/vsis3/`` (resp.
``/vsiaz/`` or ``/vsigs/``) that requires authentication *and* from a machine within
the corresponding cloud infrastructure, (AWS, Azure or Google Cloud),
a check is done to verify if the cloud company is still a GDAL sponsor by probing
https://gdal.org/en/latest/sponsors/did_aws_sponsor.html (resp.
https://gdal.org/en/latest/sponsors/did_microsoft_sponsor.html or
https://gdal.org/en/latest/sponsors/did_google_sponsor.html) with a HEAD request.

If such page is absent, the following warning (using GDAL's :cpp:func:`CPLError`
with error class ``CE_Warning``) is emitted:

::

    Due to lack of resources, Amazon S3 (resp. Azure Cloud Storage, Google Cloud Storage)
    access is undergoing minimal maintenance and may be removed in the future unless
    AWS (resp. Microsoft Azure, Google Cloud) re-evaluates its decision to stop sponsoring
    GDAL. If you are interested in keeping this functionality please get in touch with
    your AWS (resp. Microsoft Azure, Google Cloud) representative.

The warning is emitted at most once a day for a given machine, by caching the
result of the check in :file:`~/.gdal/cloud_check_aws.txt` (resp.
:file:`~/.gdal/cloud_check_ms.txt` or :file:`~/.gdal/cloud_check_gcs.txt`)

See https://github.com/OSGeo/gdal/pull/14313/changes for the implementation.

Why was it implemented?
-----------------------

:ref:`GDAL’s VSI system <virtual_file_systems>` enables efficient access to cloud storage platforms and enables
the use of GDAL at scale on cloud providers. Those capabilities are a significant
cost to the project in terms of complexity and maintenance, and while
their usage within cloud platforms generates significant revenue for the cloud platforms and
value for their customers, the project
cannot capture any of it in support of the technology that provides it.

The warning hopefully makes users aware of the economics of their usage of VSI
within cloud platform environments, gently asks them to reach out to their sales
representatives within the cloud platforms to make sponsoring GDAL a priority
again, and notifies users of their risk when using VSI for cloud platforms that
are not contributing.


How to disable it?
------------------

The :config:`GDAL_NAME_AND_SHAME` configuration option / environment variable can
be set to ``NO`` to disable this mechanism.

How does the GDAL Sponsorship Program support VSI?
--------------------------------------------------

Significant :ref:`sponsorship_program` resources have supported maintenance of
VSI on cloud platforms (visit
https://github.com/OSGeo/gdal/pulls?q=is%3Apr+label%3A%22funded+through+GSP%22+is%3Aclosed
to identify GSP-supported activities within the project). Without active support
from the cloud providers, the project will de-prioritize maintenance activity on
respective VSI drivers, seek to eliminate complexity driven by specific platform
differences, and no longer track improvements, fixes, or changes necessary to
keep up with platform APIs.


.. below is an allow-list for spelling checker.

.. spelling:word-list::
    GSP
