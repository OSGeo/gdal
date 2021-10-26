.. _sponsoring-faq:

================================================================================
Sponsoring FAQ
================================================================================

Why does GDAL need sponsorship?
-------------------------------

GDAL is the most central piece of `Critical Digital Infrastructure`_ 
in the geospatial world, serving as the primary building block for data management and processing 
in open source, commercial, and government geospatial software. But most of its resources  
have gone to the development of new features, while the maintenance burden has only increased with 
more functionality.

.. _Critical Digital Infrastructure: https://www.fordfoundation.org/campaigns/critical-digital-infrastructure-research/

The purpose of sponsor funding is to provide substantial long-acting resources
that give the project the ability to address software, testing, and project
challenges that do not attract individual sponsorship attention. With
a pool of resources that are not earmarked for features, it can
attack usability, performance, and modernization challenges that benefit everyone.
Sustained funding enables multi-year efforts that do not
disrupt the existing GDAL user base, and it will provide the baseline
resources to allow day-to-day maintenance operations to continue uninterrupted.

The larger funding levels enabled by sponsorship aim to help GDAL grow to a team of maintainers,
as is typical in other leading open source projects. The past few years have seen only one maintainer, 
who has been stretched thin. The resources provided by this effort allow maintenance activities to 
continue *and* allow the project to support on-boarding additional developers to tackle various needs 
of the project that have been ignored due to lack of direct funding interest.

My organization wants to sponsor. How can we do that?
-----------------------------------------------------

To learn about the benefits of becoming a sponsor at
various levels start with the `Sustainable GDAL Sponsorship Prospectus`_.
If you are interested, need help convincing your key decision-makers, or have
any questions, don't hesitate to contact gdal-sponsors@osgeo.org.

.. _Sustainable GDAL Sponsorship Prospectus: https://gdal.org/sponsors/Sustainable%20GDAL%20Sponsorship%20Prospectus.pdf

What is NumFOCUS and why is the project using that foundation rather than using OSGeo for this effort?
------------------------------------------------------------------------------------------------------

`NumFOCUS <https://numfocus.org>`__ is a US-based 501(c)(3) tax-exempt non-profit that is already managing
funding for many individual software projects using this model such as Numpy,
Jupyter, pandas, Julia, and SciPy. They have staff, policies, procedures, and
infrastructure for managing the financial support of open source software
projects with this funding model. Many organizations in the initial list of
sponsors are already funding projects through NumFOCUS, and adding GDAL to the
roster improves efficiency on their side.

`OSGeo <https://www.osgeo.org>`__  does not have staff and procedures to manage tracking workloads and
payments. OSGeo also does not provide tax-exempt status for contributions.
NumFOCUS has established relationships with a large number of the initial
sponsors. These properties are why the GDAL PSC has chosen the NumFOCUS path
for management of this effort.

So GDAL is a NumFOCUS project now?
----------------------------------

Not exactly. GDAL will be a project within NumFOCUS under the "Grantor-Grantee Model".
OSGeo is still the primary foundational "home" of GDAL such as it is, but NumFOCUS
is providing this financial vehicle and service to the project under the purview of
its charter.

What is the project going to do with the money?
-----------------------------------------------

* The GDAL PSC will control the purse strings. Contributors seeking resources
  will submit a proposal to the PSC (RFC-style, but not public) describing the
  tasks and efforts they will seek to achieve. Any substantial efforts with
  external impacts will continue to be required to use the GDAL RFC process as
  described in :ref:`rfc-1`.

* Developers will be able submit requests to the GDAL PSC for 'maintenance
  work units', which could encompass ticket i/o and related code improvements,
  CI grooming, mailing list gardening, and fuzzing response activities.

* A significant portion (25% per year if possible) of the resources will be targeted toward
  *growing* new active developers into the project. Examples of this include
  soliciting ticket and code contributors with funding if they show interest
  and aptitude, and providing resources to mentor and support junior developers who are
  working into roles in the project.  These activities are
  extremely hard to do without financial support.

* Spot resources will be available to attack needs that have difficulty finding
  funded attention, such as API improvements in support of specific application
  niches and subsystem refreshes like build, tests, and CI. The GDAL PSC could
  fund significant RFCs that demonstrate need and agreement on an ad hoc basis.

* The GDAL PSC will delegate some resources to GDAL-related projects and
  dependencies – libtiff, libgeotiff, PROJ, shapelib, and the various language
  bindings and libraries. If more resources than the GDAL project itself can
  use are available for a particular year, the project will open them up to the
  wider community of related libraries and users to repurpose them.

How can the resources be used?
------------------------------

The funds cannot be used to benefit an individual contributor or be directed by a
contributor through the non-profit organization. For example, a fictional
Imagery Corp cannot fund NumFOCUS with explicit intent to have those resources
used to fix bugs or add features that benefit Imagery Corp. Imagery Corp can
still continue to solicit GDAL active contributors, or actively contribute the
fixes themselves, to achieve their goals with the software.

How is this going to impact GDAL software releases?
---------------------------------------------------

A primary goal of this effort is to provide resources needed to allow the current
GDAL maintainer to
keep up the existing release schedule and cadence. Without these resources, the
schedule was likely to have significantly stretched out to one or two
maintenance releases per year.

Can I use some of the funding to support fixing something?
----------------------------------------------------------

Quite possibly. The GDAL PSC will provide a proposal template where you will
need to describe the issue(s), propose the approach and impacts of it, and
state your cost to complete the effort.
