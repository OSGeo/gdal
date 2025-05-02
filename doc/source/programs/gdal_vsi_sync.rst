.. _gdal_vsi_sync:

================================================================================
``gdal vsi sync``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Synchronize source and target file/directory located on GDAL Virtual System Interface (VSI)

.. Index:: gdal vsi sync

Synopsis
--------

.. program-output:: gdal vsi sync --help-doc

Description
-----------

:program:`gdal vsi sync` synchronize files and directories located on :ref:`virtual_file_systems`.

This is an analog to the UNIX :program:`rsync` command. In the current implementation,
:program:`rsync` would be more efficient for local file copying, but :program:`gdal vsi sync` main
interest is when the source or target is a remote
file system like /vsis3/ or /vsigs/, in which case it can take into account
the timestamps of the files (or optionally the ETag/MD5Sum) to avoid
unneeded copy operations.

This is implemented efficiently for:

- local filesystem <--> remote filesystem.

- remote filesystem <--> remote filesystem, where the source and target remote
  filesystems are the same and one of /vsis3/, /vsigs/ or /vsiaz/.
  Or when the target is /vsiaz/ and the source is /vsis3/, /vsigs/, /vsiadls/ or /vsicurl/

Similarly to :program:`rsync` behavior, if the source filename ends with a slash,
it means that the content of the directory must be copied, but not the
directory name. For example, assuming ``/home/even/foo`` contains a file ``bar``,
``gdal vsi sync -r /home/even/foo/ /mnt/media`` will create a ``/mnt/media/bar``
file. Whereas ``gdal vsi sync -r /home/even/foo /mnt/media`` will create a
``/mnt/media/foo`` directory which contains a ``bar`` file.

This is implemented by :cpp:func:`VSISync`.

Options
+++++++

.. option:: -r, --recursive

    Synchronize recursively.

.. option:: --strategy timestamp|ETag|overwrite

   Determines which criterion is used to determine if a target file must be
   replaced when it already exists and has the same file size as the source.
   Only applies for a source or target being a network filesystem.

   The default is ``timestamp`` (similarly to how :program:`aws s3 sync` works).
   For an upload operation, a remote file is replaced if it has a different size
   or if it is older than the source.
   For a download operation, a local file is  replaced if it has a different
   size or if it is newer than the remote file.

   The ``ETag`` strategy assumes that the ETag metadata of the remote file is
   the MD5Sum of the file content, which is only true in the case of /vsis3/
   for files not using KMS server side encryption and uploaded in a single
   PUT operation (so smaller than 50 MB given the default used by GDAL).
   Only to be used for /vsis3/, /vsigs/ or other filesystems using a
   MD5Sum as ETAG.

   The ``overwrite`` strategy will always overwrite the target file with the
   source one.

.. option:: -j, --num-threads <value>

   Number of jobs to run at once

Examples
--------

.. example::
   :title: Synchronize a local directory onto a S3 bucket

   .. code-block:: console

       $ gdal vsi sync -r my_directory/ /vsis3/bucket/my_directory
