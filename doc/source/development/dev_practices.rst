.. _development_practices:

.. include:: ../substitutions.rst

================================================================================
Development practices
================================================================================

Making changes to GDAL
----------------------

Minor changes to GDAL, such as bug fixes, may be made by opening a GitHub pull request.

Major changes should be discussed on the |gdal-dev| listserv and may require the drafting
of a RFC (request for comment) document.

GDAL's policy on substantial code additions is documented at :ref:`rfc-85`.


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

Commit hooks
^^^^^^^^^^^^

GDAL provides pre-commit hooks to run code linters before a commit is made. The
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
