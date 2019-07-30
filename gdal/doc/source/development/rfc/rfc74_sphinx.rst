.. _rfc-74:

===========================================================
RFC 74: Migrate gdal.org to RTD-style Sphinx infrastructure
===========================================================

======== ==============
Author:  Howard Butler
======== ==============
Contact: howard@hobu.co
Started: 2019-May-19
Status:  *Adopted*
======== ==============

Summary
-------

The document proposes migrating the GDAL documentation from a Doxygen to
`Sphinx <http://www.sphinx-doc.org/en/master/>`__ in
`ReadTheDocs <https://readthedocs.org>`__ format.

Motivation
----------

Casual contribution to the GDAL documentation is challenging. Wiki-style
contribution is possible through the Trac instance, but that requires
getting an OSGeo login, which is a high bar, and the Trac information is
disconnected from the primary documentation. Other projects such as PROJ
and `MapServer <https://mapserver.org>`__ have seen significant uptick
in contributed documentation by adopting Sphinx-based systems, and we
hope the adoption of this approach for GDAL will ignite a renaissance of
documentation contribution as it did for those projects.

The current approach has some significant deficiencies that have not
been overcome by waiting for new versions of Doxygen to arrive. These
include:

-  The Doxygen build buries the source of the documentation deep into
   the source tree, which makes it hard to find where to properly add
   information.
-  The structure of the website is indirect from the source code that
   creates it.
-  New features such as convenient mobile-friendly styling, alternative
   serializations such as PDF, and tighter API and user-level
   documentation integration are not within easy reach.
-  Editing of raw HTML means that convenient output of other
   serialization types, such as PDF, Windows Compiled Help, or manpage
   output is challenging.

Proposal
--------

The GDAL team will refactor the GDAL.org website to be based on Sphinx
with the following properties:

-  Convert the bulk of the existing documentation to reStructuredText
-  Adapt the `ReadTheDocs
   theme <https://sphinx-rtd-theme.readthedocs.io/en/stable/>`__
-  Apply an "Edit this Page on GitHub" link to every page on the site
   for convenient contribution
-  Utilize `GitHub Pages <https://pages.github.com/>`__ to host
   gdal.org, with updates being regenerated and committed to a
   repository by `Azure
   Pipelines <https://dev.azure.com/osgeo/gdal/_build>`__ continuous
   integration.
-  Output a PDF serialization of the website for documentation version
   posterity.

Considerations
~~~~~~~~~~~~~~

-  Numerous hard links to driver pages exist in source code. Care must
   be taken to attempt to preserve these links as well as possible with
   redirects to adapt to any new organization.
-  Porting of existing Trac content to the new data structure will allow
   decommission of that piece of infrastructure. Significant content
   porting investment may be required to achieve this.
-  Doxygen API-style documents are still valuable, and we propose to
   keep a rendering of them at ``/doxygen`` for users who wish to
   continue with that approach. Internal API documentation will continue
   to use Doxygen, and it will be reflected into the Sphinx website
   using the Breathe capability.
-  Initial content organization will attempt to mimic the existing
   website as well as can be achieved, but no requirement to maintain
   adherence to the previous structure is required if other organization
   approaches are more convenient given the features and capabilities of
   Sphinx.
-  Existing translations will not be ported. Adaptation and continuation
   of porting of translations is beyond the scope of this RFC, but there
   are capabilities for managing translations in Sphinx (MapServer.org
   provides an excellent example), and follow-on contributors can keep
   moving forward with the architecture once the initial effort is
   complete.
-  Content may be missed during the transition. Please file tickets in
   GitHub for any items that became more difficult to find or are gone
   after the transition.

Logistics
~~~~~~~~~

A current example of the site lives at
`https://gdal.dev <https://gdal.dev>`__ This example is set to noindex.
Once the RFC is passed, adaptation of the infrastructure that builds it
will be migrated to `https://gdal.org <https://gdal.org>`__ and the
example website will be completely decommissioned. Currently,
`https://github.com/hobu/gdal <https://github.com/hobu/gdal>`__
``doc-sprint`` branch is the fork that drives this content. It will be
squash-merged to the main repository at the passing of the RFC.

References:

-  `issue #1204 <https://github.com/OSGeo/gdal/issues/1204>`__.
-  `2019 OSGeo Community Code
   Sprint <https://wiki.osgeo.org/wiki/OSGeo_Community_Sprint_2019>`__

Voting history
--------------

+1 from KurtS, HowardB, DanielM, NormanB, JukkaR and EvenR.
