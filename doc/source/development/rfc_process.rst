.. _rfc_process:

================================================================================
Request For Comments (RFC) process for code changes
================================================================================

While most bug fixes and small enhancements following the existing logic of code
do not require a RFC and can be done using pull requests, we require contributors
to write a RFC and go through a formal adoption process in the following cases:

- Anything that could cause backward compatibility issues.
- Adding substantial amounts of new code (see :ref:`rfc-85`)
- Changing inter-subsystem APIs, or objects.
- Anything that might be controversial.

A good rule of thumb is that if you wonder "does this require a RFC?", the
answer is generally "yes".

The RFC process is the following one:

- add a new .rst file in :file:`doc/source/development/rfc` with the RFC text,
  and references it in :file:`doc/source/development/rfc/index.rst`, and issue
  a pull request.
  You can look at https://github.com/OSGeo/gdal/pull/10913 as a potential example
  for the expected content. Typical sections are:

  - Title: RFC {XYZ}: {Description of the RFC in a short sentence}
  - Author: your name.
  - Contact: your email.
  - Started: date at which this is started.
  - Status: "Draft", when the RFC is started.
  - Summary: what the RFC accomplishes in a few sentences.
  - Motivation: Paragraph describing why this RFC is needed.
  - Technical solution: organized in several sub-paragraph depending on the
    complexity of the RFC. You may need to mention new API calls, impacted
    files, etc., and other needed details.
  - Backward compatibility: API, ABI, and other compatibility concerns.
  - Performance impacts.
  - Documentation: how this will be documented.
  - Testing: how this will be tested.
  - Related issues and PRs: points at potential existing tickets about the topic,
    and pull request with a candidate implementation
  - Voting history: "TBD" (to be defined), when the RFC is started

- Send an email to the :ref:`gdal-dev mailing list <mailing_list>` with a title like:
  "Call for discussion on RFC {XYZ}: {Description of the RFC in a short sentence}"
  where you point to the pull request. You may get feedback both in the pull
  request or in the mailing list.

- Wait for feedback (give readers at least one week, or more if holiday season).
  If it seems positive and reasonable consensus can be reached, you can start
  working on a candidate implementation, and issue a pull request for it,
  separate for the pull request with the RFC text.
  Reference this candidate implementation pull request in the "Related issues
  and PRs" section of the RFC text.

- Once your candidate implementation is representative enough, and you and other
  contributors can have a good confidence that the RFC can be implemented in the
  way it has been described, send an email to the gdal-dev mailing list with
  a title like "Motion: adopt RFC {XYZ}: {Description of the RFC in a short sentence}"
  where you point to the pull request.
  All subscribers can vote, but only votes from members of the GDAL Project
  Steering Committee are binding. The voting rules are covered by :ref:`rfc-1`.

- Once the RFC is approved, finish your candidate implementation if not already
  finished, and wait for the review process and for it to be merged.

- Update the Status to "Adopted, implemented" and Voting history sections of the
  RFC text.
