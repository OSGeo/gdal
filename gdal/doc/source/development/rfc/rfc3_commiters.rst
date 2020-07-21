.. _rfc-3:

================================
RFC 3: GDAL Committer Guildlines
================================

Author: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Adopted

Purpose
-------

To formalize SVN (or CVS) commit access, and specify some guidelines for
SVN committers.

Election to SVN Commit Access
-----------------------------

Permission for SVN commit access shall be provided to new developers
only if accepted by the GDAL/OGR Project Steering Committee. A proposal
should be written to the PSC for new committers and voted on normally.
It is not necessary to write an RFC document for these votes, a proposal
to gdal-dev is sufficient.

Removal of SVN commit access should be handled by the same process.

The new committer should have demonstrated commitment to GDAL/OGR and
knowledge of the GDAL/OGR source code and processes to the committee's
satisfaction, usually by reporting bugs, submitting patches, and/or
actively participating in the GDAL/OGR mailing list(s).

The new committer should also be prepared to support any new feature or
changes that he/she commits to the GDAL/OGR source tree in future
releases, or to find someone to which to delegate responsibility for
them if he/she stops being available to support the portions of code
that he/she is responsible for.

All committers should also be a member of the gdal-dev mailing list so
they can stay informed on policies, technical developments and release
preparation.

New committers are responsible for having read, and understood this
document.

Committer Tracking
------------------

A list of all project committers will be kept in the main gdal directory
(called COMMITTERS) listing for each SVN committer:

-  Userid: the id that will appear in the SVN logs for this person.
-  Full name: the users actual name.
-  Email address: A current email address at which the committer can be
   reached. It may be altered in normal ways to make it harder to
   auto-harvest.
-  A brief indication of areas of responsibility.

SVN Administrator
-----------------

One member of the Project Steering Committee will be designed the SVN
Administrator. That person will be responsible for giving SVN commit
access to folks, updating the COMMITTERS file, and other SVN related
management. That person will need login access on the SVN server of
course.

Initially Frank Warmerdam will be the SVN Administrator.

SVN Commit Practices
--------------------

The following are considered good SVN commit practices for the GDAL/OGR
project.

-  Use meaningful descriptions for SVN commit log entries.
-  Add a bug reference like "(#1232)" at the end of SVN commit log
   entries when committing changes related to a ticket in Trac. The '#'
   character enables Trac to create a hyperlink from the changeset to
   the mentioned ticket.
-  After committing changes related to a ticket in Trac, write the tree
   and revision in which it was fixed in the ticket description. Such as
   "Fixed in trunk (r12345) and in branches/1.7 (r12346)". The 'r'
   character enables Trac to create a hyperlink from the ticket to the
   changeset.
-  Changes should not be committed in stable branches without a
   corresponding bug id. Any change worth pushing into the stable
   version is worth a bug entry.
-  Never commit new features to a stable branch without permission of
   the PSC or release manager. Normally only fixes should go into stable
   branches.
-  New features go in the main development trunk.
-  Only bug fixes should be committed to the code during pre-release
   code freeze, without permission from the PSC or release manager.
-  Significant changes to the main development version should be
   discussed on the gdal-dev list before you make them, and larger
   changes will require a RFC approved by the PSC.
-  Do not create new branches without the approval of the PSC. Release
   managers are assumed to have permission to create a branch.
-  All source code in SVN should be in Unix text format as opposed to
   DOS text mode.
-  When committing new features or significant changes to existing
   source code, the committer should take reasonable measures to insure
   that the source code continues to build and work on the most commonly
   supported platforms (currently Linux and Windows), either by testing
   on those platforms directly, running [wiki:Buildbot] tests, or by
   getting help from other developers working on those platforms. If new
   files or library dependencies are added, then the configure.in,
   Makefile.in, Makefile.vc and related documentations should be kept up
   to date.

Relationship with other upstream projects imported in GDAL/OGR code base
------------------------------------------------------------------------

Some parts of the GDAL/OGR code base are regularly refreshed from other
upstream projects. So changes in those areas should go first into those
upstream projects, otherwise they may be lost during a later refresh.
Note that those directories may contain a mix of GDAL specific files and
upstream files. This has to be checked on a case-by-case basis (any file
with CVS changelog at its beginning is a good candidate for belonging to
the upstream project)

Currently the list of those areas is :

-  frmts/gtiff/libtiff : from libtiff CVS
   (`http://www.remotesensing.org/libtiff/ <http://www.remotesensing.org/libtiff/>`__)
-  frmts/gtiff/libgeotiff : from libgeotiff SVN
   (`http://trac.osgeo.org/geotiff/ <http://trac.osgeo.org/geotiff/>`__)
-  frmts/jpeg/libjpeg : from libjpeg project
   (`http://sourceforge.net/projects/libjpeg/ <http://sourceforge.net/projects/libjpeg/>`__)
-  frmts/png/libpng : from libpng project
   (`http://www.libpng.org/pub/png/libpng.html <http://www.libpng.org/pub/png/libpng.html>`__)
-  frmts/gif/giflib : from giflib project
   (`http://sourceforge.net/projects/giflib <http://sourceforge.net/projects/giflib>`__)
-  frmts/zlib : from zlib project
   (`http://www.zlib.net/ <http://www.zlib.net/>`__)
-  ogr/ogrsf_frmts/mitab : from MITAB CVS
   (`http://mitab.maptools.org/ <http://mitab.maptools.org/>`__)
-  ogr/ogrsf_frmts/avc : from AVCE00 CVS
   (`http://avce00.maptools.org/ <http://avce00.maptools.org/>`__)
-  ogr/ogrsf_frmts/shape/[dbfopen.c, shpopen.c, shptree.c, shapefil.h] :
   from shapelib project
   (`http://shapelib.maptools.org/ <http://shapelib.maptools.org/>`__)
-  data/ : some .csv files related to CRS come from libgeotiff

Legal
-----

Committers are the front line gatekeepers to keep the code base clear of
improperly contributed code. It is important to the GDAL/OGR users,
developers and the OSGeo foundation to avoid contributing any code to
the project without it being clearly licensed under the project license.

Generally speaking the key issues are that those providing code to be
included in the repository understand that the code will be released
under the MIT/X license, and that the person providing the code has the
right to contribute the code. For the committer themselves understanding
about the license is hopefully clear. For other contributors, the
committer should verify the understanding unless the committer is very
comfortable that the contributor understands the license (for instance
frequent contributors).

If the contribution was developed on behalf of an employer (on work
time, as part of a work project, etc) then it is important that an
appropriate representative of the employer understand that the code will
be contributed under the MIT/X license. The arrangement should be
cleared with an authorized supervisor/manager, etc.

The code should be developed by the contributor, or the code should be
from a source which can be rightfully contributed such as from the
public domain, or from an open source project under a compatible
license.

All unusual situations need to be discussed and/or documented.

Committers should adhere to the following guidelines, and may be
personally legally liable for improperly contributing code to the source
repository:

-  Make sure the contributor (and possibly employer) is aware of the
   contribution terms.
-  Code coming from a source other than the contributor (such as adapted
   from another project) should be clearly marked as to the original
   source, copyright holders, license terms and so forth. This
   information can be in the file headers, but should also be added to
   the project licensing file if not exactly matching normal project
   licensing (gdal/LICENSE.txt).
-  Existing copyright headers and license text should never be stripped
   from a file. If a copyright holder wishes to give up copyright they
   must do so in writing to the foundation before copyright messages are
   removed. If license terms are changed it has to be by agreement
   (written in email is ok) of the copyright holders.
-  Code with licenses requiring credit, or disclosure to users should be
   added to /trunk/gdal/LICENSE.TXT.
-  When substantial contributions are added to a file (such as
   substantial patches) the author/contributor should be added to the
   list of copyright holders for the file.
-  If there is uncertainty about whether a change it proper to
   contribute to the code base, please seek more information from the
   project steering committee, or the foundation legal counsel.

Bootstraping
------------

The following existing committers will be considered authorized GDAL/OGR
committers as long as they each review the committer guidelines, and
agree to adhere to them. The SVN administrator will be responsible for
checking with each person.

-  Daniel Morissette
-  Frank Warmerdam
-  Gillian Walter
-  Andrey Kiselev
-  Alessandro Amici
-  Kor de Jong
-  Howard Butler
-  Lichun Wang
-  Norman Vine
-  Ken Melero
-  Kevin Ruland
-  Marek Brudka
-  Pirmin Kalberer
-  Steve Soule
-  Frans van der Bergh
-  Denis Nadeau
-  Oleg Semykin
-  Julien-Samuel Lacroix
-  Daniel Wallner
-  Charles F. I. Savage
-  Mateusz Loskot
-  Peter Nagy
-  Simon Perkins
-  Radim Blazek
-  Steve Halasz
-  Nacho Brodin
-  Benjamin Collins
-  Ivan Lucena
-  Ari Jolma
-  Tamas Szekeres

--------------

-  `COMMITTERS <http://trac.osgeo.org/gdal/browser/trunk/gdal/COMMITTERS>`__
   file
-  `Edit GDAL Subversion
   Group <https://www.osgeo.org/cgi-bin/auth/ldap_group.py?group=gdal>`__
