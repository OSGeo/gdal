.. _rfc-83:

=============================================================
RFC 83: guidelines for the use of GDAL project sponsorship
=============================================================

============== =============================================
Author:        Even Rouault (with content from RFC 9)
Contact:       even.rouault @ spatialys.com
Started:       2021-May-19
Status:        Adopted
============== =============================================

Summary
-------

Per :ref:`rfc-80`, the GDAL project benefits from a multi-year budget coming
from sponsorship. This document formalizes guidelines for how that budget will
be spent by the project and supersedes :ref:`rfc-9`

There are two different type of tasks that will be funded:

- Day-to-day maintenance tasks. They will be filled by several co-maintainers.
- Tasks to achieve a specific development

Day-to-day maintenance tasks
----------------------------

Scope
+++++

- Bug tracker maintenance:

    * Validating completeness of bug reports to ensure reproducibility
    * Closing reports that don't respect the guidelines, or are invalid.
    * Tagging the issue with appropriate labels and milestones.
    * When a fix is relevant, addressing the issue with a pull request.
    * Adding / enhancing tests in the regression test suite should be done as
      much as practical
    * Tasks that are expected, or are found, to take more than a given time
      should be confirmed with the supervisor.

- Continuous Integration maintenance and improvements: the project has several
  Continuous Integration configurations. Some of them break regularly due to
  external causes (network resources no longer available, change in
  dependencies, etc.). The co-maintainer will take the necessary actions to
  keep them in working state.
- Review of code and documentation contributions. Such contributions are made
  currently through GitHub pull requests. The co-maintainer will review the
  contribution and guide the proponent through the steps to make it accepted:
  identifying issues reported by Continuous Integration, asking for new tests
  to be developed whenever possible, etc. When the contributor is not in capacity
  to polish his/her contribution, the co-maintainer may do it themself.
- Ensuring that fixes that are appropriate for backporting in stable branche(s)
  are backported.
- Monitoring of project communication channels (mailing list, IRC, Slack
  channels, etc.). The co-maintainer will in particular identify issues that are
  reported through those means and ensures that they are captured by a
  corresponding ticket.
  The co-maintainer may also occasionally answer questions on usage of the
  library and tools, but in-depth support of users is not in the scope of the
  covered tasks   (users may use service providers for that purpose).
- Proactive tracking of new versions of libraries that GDAL depends on, and
  address needed changes for compilation and proper runtime behavior
- Monitor and address reports of static and dynamic analyzers. Currently:
  OSS-Fuzz,   cppcheck, Clang Static Analyzer, Coverity Scan, gcc&clang
  memory/undefined behavior/etc. analyzers.
- Documentation improvements
- Activities related to software releases: writing the release notes,
  issuing release candidates, addressing feedback, finalizing releases
- For experienced contributors, help less experienced contributors get on board
  and along with GDAL.

Projects that are concerned by such activities: GDAL itself of course, but also
other, generally smaller, free and open source projects that have been strongly
tied to it as being required or very common dependencies for GDAL, and don't
benefit from similar maintenance funds (on the contrary, projects that use GDAL
don't qualify for being funded by the GDAL sponsorship funds). The current list
includes PROJ, libtiff, libgeotiff, shapelib, GEOS, OpenJPEG. If during the
investigation of an issue, a co-maintainer finds an issue in another open source
component and can come up with a patch, they are also welcome to do it.

A typical share of time could be 75% for GDAL and 25% for the other projects.

GDAL is a vast and complex project. It is not expected that each co-maintainer
has expertise in all its aspects. Applicants will mention their domains of
interest, which can evolve over time, and coordinate with other stakeholders in
how tasks are assigned.

Direction
+++++++++

.. Mostly taken from RFC9, but with a key difference on the priority given to
   issues coming from sponsors

The co-maintainer is generally subject to the project PSC. However, for day to
day decisions one PSC member might be designated as the supervisor for the
co-maintainer (PSC members that are funded through this program will use existing
decision making processes to prioritize their work). This supervisor will
coordinate with the co-maintainer via email or IRC discussions.

.. I was unsure how to handle that situation, but the supervisor role might be
   somewhat time consuming, and if no PSC member is funded to supervise a funded
   PSC member that might not work well in practice.

The supervisor will try to keep the following in mind when prioritizing tasks:

- Tickets that are regressions compared to a previous release are of higher
  priority than other tasks
- Bug reports should be given higher priority than other tasks. (bug reports
  from sponsors will *not* qualify as such for higher priority treatment)
- Areas of focus identified by the PSC should be given higher priority than
  other tasks.
- Bugs or needs that affect many users should have higher priority.
- The co-maintainer should be used to take care of work that no one else is
  willing and able to do (i.e. fill the holes, rather than displacing volunteers)
- Try to avoid tying up the co-maintainer on one big task for many weeks unless
  directed by the PSC.
- The co-maintainer should not be directed to do work for which someone else is
  getting paid.

Substantial new development projects will only be taken on by the co-maintainer
with the direction of a PSC motion (or possibly an RFC designating the co-maintainer
to work on a change).

Note that the co-maintainer and the co-maintainer supervisor are subject to the
normal RFC process for any substantial change to GDAL.

Reporting
+++++++++

The co-maintainer will produce a monthly report (typically in a shared spreadsheet)
with the tasks they have tackled, typically pointing at tickets and pull requests,
and the amount of hours spent during the reported period.

Who can apply ?
+++++++++++++++

Applicants should be individuals, either self-employed or employed by a company
agreeing to allocate some of their time to such activities, that have been a
proven public track record of meeting the following qualities:

- Knowledge of the programming languages required for the task: C/C++ for GDAL
  library and some knowledge of Python for the test suite, appropriate language
  for any of the SWIG bindings.
- Knowledge of the geospatial field, ideally with one or several open source
  geospatial projects, and/or experience with file formats and low-level
  considerations.
- Ability to interact with members of an open source community in accordance to
  the Code of Conduct.
- Good knowledge of written English is necessary.

We cannot put formal criteria on that, but applicants should ideally aim for a
multi-year involvement with the project, so that their onboarding time is amortized.

Each co-maintainer will be allocated a maximum number of hours per quarter (they
will indicate their planned availability and the PSC will decide on the effective
allocation), and will invoice on the time effectively spent within that allocation.

Applicants will provide their hourly rate, in US dollars, to the PSC (privately)

Tasks to achieve a specific development
---------------------------------------

.. any better naming ? should we call that a grant program like the QGIS one ?
   The "grant" term may imply that some cost-sharing is required, whereas I think
   we could intend to cover full cost of proposals

The GDAL PSC will call for proposals (frequency to be adjusted, but could be per
quarter year) of proponents that want to achieve a specific development.
The PSC will indicate the total budget available and other conditions.
The PSC may suggest a few ideas that it would want to receive proposals for.

Generally speaking, priority will be given to proposals that address housekeeping
tasks, non-directly user oriented aspects  of the project(s), rather than
user-oriented features (new drivers, new utilities, etc. are more prone to be
funded by interested parties), but the later ones can be proposed if they are
deemed of sufficiently large interest.

A non-exhaustive list of topics that are meant to be addressed per this vehicle are:

- Improving/rewriting a part of the code base
- Changes that affect a large part of the code base
- Speed optimizations
- Adding / improving support for some platforms
- Improvements in test suite / Continuous Integration
- Improvements in build system
- Improvements in documentation
- Packaging efforts (provided that they use fully reproducible open source build recipes)

Applicants will provide the amount to be funded.
Proposals may be put together by one or several individuals (in the later case,
to be determined if we can let the team have a "invoicing point of contact" and
let them arrange how to dispatch it amongst members, or if each team member
should ask for its part of funding).
An applicant may submit proposals for several subjects.

Applicants will submit the technical details of their proposal as an issue in
the bug tracker where it can be collaboratively discussed with interested members
of the community (or as an RFC for changes that would usually qualify for a RFC).

.. Above is inspired from QGIS Enhancement proposal mechanisms. See
   https://github.com/qgis/QGIS-Enhancement-Proposals/issues?q=is%3Aissue+is%3Aopen+label%3AGrant-2021

Criteria for applicants are the same as in the above section.

Decision process
----------------

The allocation of funds, through the selection of co-maintainers and grantees of
specific developments, will be decided by the PSC.

.. note:: The input provided by the Advisory Board regarding maintenance
          priorities will be taken into account by the PSC, on an equal footing
          as other input provided by the community, and PSC's own analysis of
          priorities.

PSC members that apply for funds, or that have a conflict of interest (e.g. working
in the same company as an applicant), or any other situation of conflict or
interest, may take part in discussions, but should abstain from voting on
decisions related to fund allocation.

Note
----

As this is a new way of operating for the project, it is expected that this RFC
will evolve over time with the gained experience in the management of the
sponsorship program.

Voting History
--------------

https://lists.osgeo.org/pipermail/gdal-dev/2021-June/thread.html#54249

+1 from PSC members MateuszL, HowardB, FrankW, KurtS, SeanG, JukkaR, DanielM and EvenR
