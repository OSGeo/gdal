.. _development_practices:

================================================================================
Development practices
================================================================================

Making changes to GDAL
----------------------

Minor changes to GDAL, such as bug fixes, may be made by opening a GitHub pull request.

Major changes should be discussed on the |gdal-dev| listserv and may require the drafting
of a RFC (request for comment) document.

GDAL's policy on substantial code additions is documented at :ref:`rfc-85`.

Portability
-----------

GDAL strives to be widely portable to 32 bit and 64 bit computing
environments, as well as little-endian and big-endian ordered CPUs.
CPL functions in the port directory provide services to abstract platform
specific operations.

Generally speaking, where available CPL functions should be used in
preference to operating system functions for operations like memory
allocation, path parsing, filesystem I/O (using VSILFILE* / VSIVirtualFile*),
ODBC access, etc.

C/C++ standards
---------------

The current C and C++ standards adopted by GDAL/OGR are C99 and C++17
(last updated per :ref:`rfc-98`).

Variable naming
---------------

Much of the existing GDAL/OGR code uses an adapted Hungarian naming
convention. Use of this convention is not mandatory, but when
maintaining code using this convention it is desirable to continue
adhering to it with changes. Most importantly, please avoiding using it
improperly as that can be very confusing.

In Hungarian prefixing the prefix tells something about about the type,
and potentially semantics of a variable. The following are some prefixes
used in GDAL/OGR.

-  *a*: array
-  *b*: C/C++ bool. In C code that pre-dates C99 adoption, it is also used for ints with only TRUE/FALSE values.
-  *by*: byte (GByte / unsigned char).
-  *df*: floating point value (double precision)
-  *e*: enumeration
-  *i*: integer number used as a zero based array or loop index.
-  *f*: floating point value (single precision)
-  *h*: an opaque handle (such as GDALDatasetH).
-  *n*: integer number (size unspecified)
-  *o*: C++ object
-  *os*: CPLString or std::string
-  *p*: pointer
-  *psz*: pointer to a null-terminated string. (eg. "char \*pszName;")
-  *sz*: null-terminated string (eg." char szName[100];")
-  *k*: compile-time constant

Prefixes can be stacked. The following are some examples of meaningful
variables.

-  \*char !\*\ *papszTokens*: Pointer to an array of strings.
-  \*int *panBands*: Pointer to the first element of an array of
   numbers.
-  \*double *padfScanline*: Pointer to the first element of an array of
   doubles.
-  \*double *pdfMeanRet*: Pointer to a single double.
-  \*GDALRasterBand *poBand*: Pointer to a single object.
-  \*GByte *pabyHeader*: Pointer to an array of bytes.

It may also be noted that the standard convention for variable names is
to capitalize each word in a variable name.

Function and class naming
-------------------------

- Functions and classes should have a selective enough namespace ("GDAL" or "OGR" prefix, or use of C++ namespace) to avoid symbol collision.

File naming and code formatting
-------------------------------

- All source files (.h, .c, .cpp, .py, etc.) should have a header with copyright attribution and the text of the GDAL X/MIT license.
- Use lower case filenames.
- Use .cpp extension for C++ files (not .cc).
- C/C++ code formatting rules are defined in :source_file:`.clang-format`. Python code formatting
  is enforced by Black. The pre-commit utility should be used to enforce them automatically. See :ref:`commit_hooks`.

Memory allocation
-----------------

Large memory allocations should be performed using the :cpp:func:`VSIMalloc` family of functions, which will return ``nullptr`` on allocation failure.
As per :ref:`rfc-19`, you can use ``VSIMalloc2(x, y)`` instead of
doing ``CPLMalloc(x * y)`` or ``VSIMalloc(x * y)``. :cpp:func:`VSIMalloc2` will detect
potential overflows in the multiplication and return a NULL pointer if
it happens. This can be useful in GDAL raster drivers where x and y are
related to the raster dimensions or raster block sizes. Similarly,
``VSIMalloc3(x, y, z)`` can be used as a replacement for ``CPLMalloc(x * y * z)``.

When working with standard library data structures such as ``std::vector`` that may throw ``std::bad_alloc``, a try/catch block should be used around blocks that may allocate a large amount of memory.

Adding a new driver
-------------------

- If the driver depends on a third-party library, compilation of the driver must be made conditional on the presence of the library. Drivers should try to re-use existing library dependencies as much as possible, e.g. Expat for SAX XML parsing.
- For a vector driver, check that the Open() method of the driver (often delegated to a Open() method of the datasource) is selective enough (i.e. it will not accept data files that are not meant for the driver), and robust enough (it will not crash for small variations w.r.t content that it would recognize). Check that it can deal with unusual filenames. For a GDAL driver, similar checks, as well for the optional Identify() method.
- A set of tests covering the driver should be added to the Python test suite. If appropriate, small sample data files may be added to autotest/gdrivers/data or autotest/ogr/data. The ``test_ogrsf`` utility and ``GDALTest`` class may simplify testing of basic driver functionality.
- A documentation page should be created for the driver. Documentation should, at a minimum, briefly describe the format handled by the driver and, when relevant, describe the particular syntax for the connection string, creation options, configuration options, etc. The documentation should provide a link to a more detailed format description and mention needed third-party libraries.

Writing tests
-------------

See :ref:`writing_tests`.

Git usage
---------

This section collects a few best practices for git usage for GDAL development.

Initiating your work repository
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Fork |OSGeo/gdal| from the GitHub UI, and then run:

.. code-block:: bash

    git clone https://github.com/OSGeo/gdal
    cd gdal
    git remote add my_user_name git@github.com:my_user_name/gdal.git

Working with a feature branch
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

    git checkout master
    # potentially update your local master against upstream, as described above
    git checkout -b my_new_feature_branch

    # do work. For example:
    git add my_new_file
    git add my_modifid_message
    git rm old_file
    git commit -a

    # you may need to resynchronize against master if you need some bugfix
    # or new capability that has been added since you created your branch
    git fetch origin
    git rebase origin/master

    # At end of your work, make sure history is reasonable by folding non
    # significant commits into a consistent set
    git rebase -i master
    # use 'fixup' for example to merge several commits together,
    # and 'reword' to modify commit messages

    # or alternatively, in case there is a big number of commits and marking
    # all them as 'fixup' is tedious
    git fetch origin
    git rebase origin/master
    git reset --soft origin/master
    git commit -a -m "Put here the synthetic commit message"

    # push your branch
    git push my_user_name my_new_feature_branch


From the GitHub UI, issue a pull request.

If the pull request discussion or automated checks require changes, commit
locally and push. To get a reasonable history, you may need to combine commits
using ``git rebase -i master``, in which case you will have to force-push your
branch with ``git push -f my_user_name my_new_feature_branch``.


Updating your local master against upstream master
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

    git checkout master
    git fetch origin

    # Be careful: this will lose all local changes you might have done now
    git reset --hard origin/master

Commit messages
^^^^^^^^^^^^^^^

Commit messages should indicate a component name (eg a driver name), a short
description, and when relevant, a reference to a issue (with 'fixes #' if it
actually fixes it)

::

    COMPONENT_NAME: fix bla bla (fixes #1234)

    Details here...

.. _commit_hooks:

Commit hooks
^^^^^^^^^^^^

GDAL provides pre-commit hooks to run code formatters and linters before a commit is made. The
hooks are cloned with the repository and can be installed using
`pre-commit <https://pre-commit.com>`_:

.. code-block:: bash

    python3 -m pip install pre-commit
    pre-commit install


Once installed, the hooks can be run manually via ``pre-commit run --all-files``.

Blame ignore file
^^^^^^^^^^^^^^^^^

Due to whole-tree code reformatting done during GDAL 3.7 development,
``git blame`` information might be misleading. To avoid that, you need
to modify your git configuration as following to ignore the revision of
the whole-tree reformatting:

.. code-block:: bash

    git config blame.ignoreRevsFile .git-blame-ignore-revs


Backporting bugfixes from master to a stable branch
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

    git checkout master
    With git log, identify the sha1sum of the commit you want to backport
    git checkout 2.2 # if you want to backport to 2.2
    git pull origin 2.2
    # git checkout -b branch_name  # if you intend to submit the backport as a pull request
    git cherry-pick the_sha1_sum
    git push ...

If changes are needed, do them and ``git commit -a --amend``


Things you should NOT do
^^^^^^^^^^^^^^^^^^^^^^^^

(For anyone with push rights to |OSGeo/gdal|) Never modify a commit or
the history of anything that has been
committed to https://github.com/OSGeo/gdal

Committing symbolic links is allowed only under the .github directory in order to
avoid potential problems on Windows.
