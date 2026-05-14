.. _ai_tool_policy:

================================================================================
LLM tool policy
================================================================================

Rationale
--------------------------------------------------------------------------------

LLMs are changing software development in many ways. What was once scarce, the
time and expertise to *write* software, is now plentiful. This scarcity was a
choke point that limited consumption of related resources in open source
projects. With that scarcity effectively gone in 2026, the dynamics and economics
of a project like GDAL are being disrupted.

A project like GDAL is not simply code. It is also design, review, coordinated
refactoring, deprecation scheduling, coordinated communication, distribution,
and historical outlook. The plentiful convenience of code writing LLMs do not
effectively replace the human maintainers, architects, and documenters doing
the jobs that provide much of the value GDAL users actually derive. If they
did, LLM contributors would not need to bother to upstream activity to the
project at all.

With the explosion of LLM usage in software development, the constrained
resource is now "maintenance". It is the time to review your contribution, the
time to make it concise, the time to refactor it in to a larger system, and the
time to protect the larger software system from disruption, breakage, and
performance degradation. Indiscriminate usage of LLMs in open source projects
*consume* maintenance, and the GDAL LLM tool policy attempts to conserve that
resource.

Additionally, legal systems across the world (including US and EU) have not
definitely determined whether LLM outputs are derived works of training data or
if LLM-written code can even be copyrighted by a human. This is despite it
being latently extracted and originated from open source software in the first
place.

Policy
--------------------------------------------------------------------------------

Contributors can make **limited use** of LLMs for contributions in GDAL,
subject to details mentioned below:

    * **Human contributors must be the primary author(s) of GDAL contributions**

    * All contributions including code, ticket comments, and commit messages
      should be fully understood by the author(s) submitting them to the
      project.

    * Submission of `vibe-coded <https://en.wikipedia.org/wiki/Vibe_coding>`__ contributions is *banned*.

    * LLMs may only be used as an improved auto-completion mechanism, or
      for repeated tasks (mechanical refactoring) that could potentially be
      completed with a deterministic algorithm.

    * Human-coordinated or uncoordinated (OpenClaw, etc) use of agents for
      submission of contributions to the GDAL repository is *banned*.

    * *Any* LLM usage must be indicated by ticket label, comment, or commit
      message indication and account for what was written by whom/what.

    * The contributing human author is ultimately responsible for every line of
      code, comment, or mailing list interaction they initiate, and all of it
      is subject to the project's :ref:`code_of_conduct`.

    * The typical high verbosity of LLM code and text is actively discouraged.
      More code is more code to maintain. High verbosity contribution (tickets,
      code, messages, etc) will be seen as indication of LLM-generated content
      when not labeled otherwise and may be ignored, closed, left unmerged, or
      removed at maintainers' discretion.


Violations
--------------------------------------------------------------------------------

If a maintainer judges that a contribution does not comply with this policy,
they should paste the following response to request changes:

.. code-block:: text

    This PR does not appear to comply with our policy on tool-generated content,
    and requires additional justification for why it is valuable enough to the
    project for us to review it. Please see our developer policy on
    AI-generated contributions:
    https://gdal.org/community/ai_tool_policy.html

If a contributor fails to rectify their contribution to comply with the policy,
maintainers may lock the conversation and/or close the pull request/issue/RFC.
In case of repeated violations of our policy, the GDAL project reserves itself
the right to temporarily or permanently ban the infringing person/account.

Mitigation
--------------------------------------------------------------------------------

The GDAL :ref:`sponsorship_program` is one way your organization can help
buffer the cost and disruption of LLMs in keystone projects such as GDAL. The
constrained resource is maintenance, not adding more code/capability. The
Sponsorship Program financially supports operation of GDAL as an ongoing open source
software project, and without it, much of the activity GDAL users take for granted
would simply not happen.

.. below is an allow-list for spelling checker.

.. spelling:word-list::
    LLM
    documenters
    unmerged
    latently
