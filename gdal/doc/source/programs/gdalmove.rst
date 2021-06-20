.. _gdalmove:

================================================================================
gdalmove.py
================================================================================

.. only:: html

    Transform georeferencing of raster file in place.

.. Index:: gdalmove

Synopsis
--------

.. code-block::

    gdalmove.py [-s_srs <srs_defn>] -t_srs <srs_defn>
                [-et <max_pixel_err>] <target_file>

Description
-----------

The :program:`gdalmove.py` script transforms the bounds of a raster file from
one coordinate system to another, and then updates the coordinate system and
geotransform of the file. This is done without altering pixel values at all. It
is loosely similar to using gdalwarp to transform an image but avoiding the
resampling step in order to avoid image damage. It is generally only suitable
for transformations that are effectively linear in the area of the file.

If no error threshold value (:option:`-et`) is provided then the file is not
actually updated, but the errors that would be incurred are reported. If
:option:`-et` is provided then the file is only modify if the apparent error
being introduced is less than the indicate threshold (in pixels).

Currently the transformed geotransform is computed based on the transformation
of the top left, top right, and bottom left corners. A reduced overall error
could be produced using a least squares fit of at least all four corner points.

.. option:: -s_srs <srs_defn>

    Override the coordinate system of the file with the indicated coordinate
    system definition. Optional. If not provided the source coordinate system
    is read from the source file.

.. option:: -t_srs <srs_defn>

    Defines the target coordinate system. This coordinate system will be
    written to the file after an update.

.. option:: -et <max_pixel_err>

    The error threshold (in pixels) beyond which the file will not be updated.
    If not provided no update will be applied to the file, but errors will be
    reported.

<target_file>

    The file to be operated on. To update this must be a file format that
    supports in place updates of the geotransform and SRS.
