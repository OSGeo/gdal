.. _rfc-79:

=============================================================
RFC 79: Listing of Service Providers on GDAL website
=============================================================

============== ============================
Author:        Daniel Morissette, Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2021-Feb-22
Last updated:  2021-Mar-01
Status:        Adopted
============== ============================

1. Motivation
=============

GDAL is developed and supported by a rich ecosystem of businesses and
individuals around the world but we do not do a good job of promoting those
service providers and making them easy to find by our users at the moment.

In this RFC, we propose a plan to address this by adding a Service Providers
page to the GDAL website.

2. Proposed plan
================

A new Service Providers page will be added to the Sphinx documentation.

The service providers will be grouped in three categories. The descriptions
below should make it relatively easy to determine in which group a given
organization belongs.

Each qualified organization who wishes to be listed is responsible for
adding themselves to the correct category in the page through a pull
request (or a direct commit in case of core committers). We are intentionally
not providing instructions here for preparing the pull request as
it is expected that a qualified service provider should have people on
staff who can figure it out. Yes, we intentionally
try to keep the bar at a minimum level.

A PSC vote is NOT required in order to add a new service provider,
i.e. any core committer can validate and commit the pull
request (or their own entry) but in case of doubts or disagreements on the
interpretation of the rules, the GDAL PSC will be the ultimate authority
to resolve the question.

Organizations in each category are displayed in randomized order (except
in the PDF docs which are static). However entries should be added to the
source file in alphabetical order (for PDF docs purpose).
The output will be similar to
https://mapserver.org/community/service_providers.html

2.1 Core Contributors
---------------------

Core Contributors are the organizations that have one or more
Core Committer (a person with merge rights in the GDAL GitHub repository)
and/or PSC members as part of their staff and who provide
GDAL related services to customers.

To qualify, the Core Committers and/or PSC members must be directly employed
by the organization and not part time contractors.

They get to add a 200px max high or wide logo with a descriptive text outlining their qualifications and services.

2.2 Contributors
----------------

Contributors are organizations who provide GDAL related services and
have one or more people on their staff who are well known in the GDAL
community for their long term contributions to the project. Contributions are
not limited to source code and can also be in the form of documentation,
binary builds, active user support via the public forums, etc.

They get to add a 150px max high or wide logo with a descriptive text outlining their qualifications and services.

2.3 Other Service Providers
---------------------------

Organization with GDAL expertise and providing services around GDAL
can fit in this category. While proprietary solutions providers are welcome,
we expect organizations listed here to be providing legitimate services to
users of the Open Source software as well.
This is not an advertisement board for proprietary products.

They get to add a 100px max high or wide logo with a descriptive text outlining their qualifications and services.

2.4 Refreshing the service providers list
-----------------------------------------

If at some point (every 2 years?) we feel that the list is or might be no
longer current, we could bring it up to date as follows:

* create a GitHub ticket dedicated for the refresh
* send an email to the list with this message "in 4 weeks, we are going to remove
  all currently listed service providers to refresh the site. If you are still
  interested, add a note to ticket XXXX".

Instead of the ticket we could just ask people to reply to the email but the advantage of
the ticket is to generate less email traffic for those not interested. It
leaves a trace of the list of organizations who responded in a single place inside the ticket.

3. FAQ
======

* Should we also recognize sponsors or organizations that are funding significant
  features in this page?

  * No. This is a service provider page and not a sponsorship page. However
    if the organization in question is also offering GDAL related services they
    can be added to the relevant category. They just won't get special status due
    to their financial contributions.

4. Credits
==========

This RFC is a straightforward adaptation of the equivalent MapServer one:
https://mapserver.org/development/rfc/ms-rfc-116.html

5. Voting History
=================

+1 from PSC members MateuszL, KurtS, JukkaR, DanielM and EvenR.
