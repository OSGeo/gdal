.. _rfc-71:

===========================
RFC 71: Migration to GitHub
===========================

======== ==========================
Author:  Even Rouault
Contact: even.rouault@spatialys.com
Started: March 2018
Status:  Adopted, implemented
======== ==========================

Summary
-------

It is proposed that the GDAL source tree and ticket database moves from
the OSGeo hosted Subversion repository/Trac database to GitHub. Full
source code history will be preserved. To make the migration simpler,
existing tickets will remain in OSGeo Trac and will not be migrated to
GitHub. New tickets will have to be opened in GitHub.

Motivations
-----------

1. It is considered that most developers interested by GDAL development
   are nowadays more used to git than Subversion, and the use of
   Subversion as the main source control management makes contributions
   less attractive.
2. The `https://github.com/OSGeo/gdal <https://github.com/OSGeo/gdal>`__
   mirror has existed since 2012 and has over time become the preferred
   way for contributors without direct SVN access (or even those with
   SVN access) to submit their contributions, in particular because of
   the coupling with the continuous integratations services of Travis-CI
   and !AppVeyor that enable maintainers to check that the contribution
   doesn't introduce known regressions + the friendly way of commenting
   a pull request. However the manual porting of !GitHub pull requests
   to Trac is a bit painful for GDAL maintainers.
3. GitHub has become the de-facto hosting platform for a lot of
   open-source projects.

Details of the migration
------------------------

0. The existing GitHub git repository will be pushed to
   `https://github.com/OSGeo/gdal_svn_mirror_backup <https://github.com/OSGeo/gdal_svn_mirror_backup>`__
   (eventually removed once we are confident further steps have not
   messed things up)
1. As GitHub also uses the syntax "#1234" to link commit messages to its
   issues that was also used in Trac, currently when following links in
   !GitHub that point to a Trac ticket, one ends up to a non-existing or
   unrelated !GitHub issue/pull request. So the commit messages of the
   current !GitHub mirror will be rewritten by a "git filter-branch
   --msg-filter 'python rewrite.py' -- --all" command to replace "#1234"
   with
   "`https://trac.osgeo.org/gdal/ticket/1234 <https://trac.osgeo.org/gdal/ticket/1234>`__"
2. The git 'trunk' branch will be renamed 'master' to follow git best
   practices
3. The existing 'tag/x.y.z' branches will be replaced by proper git
   tags.
4. This modified repository will be forced push to
   `https://github.com/OSGeo/gdal <https://github.com/OSGeo/gdal>`__
   This will have the consequence of invalidating existing pull request
   or forks of repository that will have to be rebased to the new one.
   From that point, "svn commit" should be avoided and changes should go
   to the git repository.
5. The cron job on the OSGeo server that refreshes the website from
   sources will be modified to pull from !GitHub rather than SVN.
6. Ticket creation permissions will be removed in Trac. Modification or
   closing of existing open tickets will still be possible. From that
   point, if closing a Trac ticket, one will have manually to reference
   the github commit.
7. The settings of the GDAL GitHub repository will be changed allow
   tickets to be filed. Labels and Milestones will be populated with
   relevant content

Further actions required, in no particular order, and for which help
from other GDAL developers/contributors would be welcome:

-  Most visible Trac wiki documentation will have to be revised to point
   to GitHub
-  HOWTO-RELEASE will have to be revised.
-  Existing SVN committers still interested in the project will have to
   request commit access to the GitHub repo.
-  Some support from OSGeo SAC will be needed to turn the GDAL SVN
   repository to read-only (a complementary option would be to rename it
   to gdal_historical so that people pulling from the old one are well
   aware of the migration by having their scripts 'cleanly' error out)
-  Some guidelines on how we intend to use git/GitHub features will have
   to be rewritten.

Exit strategy
-------------

GitHub is a closed platform. In case it would close or would start askin
to pay unreasonable fees, some backup strategy of the tickets would be
needed. The solutions might be:

-  `https://github.com/josegonzalez/python-github-backup <https://github.com/josegonzalez/python-github-backup>`__
-  GitLab has an import module from GitHub. Although some
   experimentation has been done with those, this RFC does *not* cover
   setting up those solutions as a regular backup system.

Not covered by this RFC
-----------------------

-  Migration of Trac wiki content to GitHub wiki is not in the scope of
   this RFC. Can be done later

Previous related discussions
----------------------------

-  `https://lists.osgeo.org/pipermail/gdal-dev/2018-March/048240.html <https://lists.osgeo.org/pipermail/gdal-dev/2018-March/048240.html>`__
-  `https://lists.osgeo.org/pipermail/gdal-dev/2017-September/047060.html <https://lists.osgeo.org/pipermail/gdal-dev/2017-September/047060.html>`__

Voting history
--------------

+1 from HowardB, JukkaR, KurtS and EvenR
