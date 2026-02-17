.. _ai_tool_policy:

================================================================================
AI/LLM tool policy
================================================================================

Summary
-------

GDAL's policy is that contributors can use whatever tools they would like to
craft their contributions, but **there must be a human in the loop**.
Contributors must read and review all AI (Artificial Intelligence) /
Large Language Model (LLM)-generated code or text before they ask other project
members to review it. The contributor is always the author and is fully
accountable for their contributions.

Warning
-------

It is not currently a settled question within the GDAL community whether code
created by LLMs is acceptable in the GDAL codebase at all. This document only
addresses the interaction aspects of LLM content, and should not be viewed as the
project taking a position on whether or not LLM code contributions are acceptable.

Policy
------

GDAL's policy is that contributors can use whatever tools they would like to
craft their contributions, but there must be a **human in the loop**.
Contributors must read and review all AI (Artificial Intelligence) / Large Language Model (LLM)-generated code
or text before they ask other project members to review it.
The contributor is always the author and is fully accountable for their contributions.
Contributors should be sufficiently confident that the contribution is high enough
quality that asking for a review is a good use of scarce maintainer time, and they
should be **able to answer questions about their work** during review.

We expect that new contributors will be less confident in their contributions,
and our guidance to them is to **start with small contributions** that they can
fully understand to build confidence. We aspire to be a welcoming community
that helps new contributors grow their expertise, but learning involves taking
small steps, getting feedback, and iterating. Passing maintainer feedback to an
LLM doesn't help anyone grow, and does not sustain our community.

Contributors **must be transparent and label contributions that
contain substantial amounts of tool-generated content**, and always mention it.
The pull request and issue templates contain a checkbox for that purpose.
Failure to do so, or lies when asked by a reviewer, will be considered as a violation.
Our policy on labeling is intended to facilitate reviews, and not to track which parts of
GDAL are generated. Contributors should note tool usage in their pull request
description, commit message, or wherever authorship is normally indicated for
the work. For instance, use a commit message trailer like Assisted-by: <name of
code assistant>. This transparency helps the community develop best practices
and understand the role of these new tools.

This policy includes, but is not limited to, the following kinds of
contributions:

- Code, usually in the form of a pull request
- RFCs or design proposals
- Issue or security vulnerability reporting
- Comments and feedback on pull requests

Details
-------

To ensure sufficient self review and understanding of the work, it is strongly
recommended that contributors write PR descriptions themselves (if needed,
using tools for translation or copy-editing), in particular to avoid over-verbose
descriptions that LLMs are prone to generate. The description should explain
the motivation, implementation approach, expected impact, and any open
questions or uncertainties to the same extent as a contribution made without
tool assistance.

An important implication of this policy is that it bans agents that take action
in our digital spaces without human approval, such as the GitHub `@claude`
agent. Similarly, automated review tools that
publish comments without human review are not allowed. However, an opt-in
review tool that **keeps a human in the loop** is acceptable under this policy.
As another example, using an LLM to generate documentation, which a contributor
manually reviews for correctness and relevance, edits, and then posts as a PR,
is an approved use of tools under this policy.

Extractive Contributions
------------------------

The reason for our "human-in-the-loop" contribution policy is that processing
patches, PRs, RFCs, comments, issues, security alerts to GDAL is not free --
it takes a lot of maintainer time and energy to review those contributions! Sending the
unreviewed output of an LLM to open source project maintainers *extracts* work
from them in the form of design and code review, so we call this kind of
contribution an "extractive contribution".

Our **golden rule** is that a contribution should be worth more to the project
than the time it takes to review it. These ideas are captured by this quote
from the book `Working in Public <https://press.stripe.com/working-in-public>`__ by Nadia Eghbal:

    When attention is being appropriated, producers need to weigh the costs and
    benefits of the transaction. To assess whether the appropriation of attention
    is net-positive, it's useful to distinguish between *extractive* and
    *non-extractive* contributions. Extractive contributions are those where the
    marginal cost of reviewing and merging that contribution is greater than the
    marginal benefit to the project's producers. In the case of a code
    contribution, it might be a pull request that's too complex or unwieldy to
    review, given the potential upside.

    -- Nadia Eghbal


Prior to the advent of LLMs, open source project maintainers would often review
any and all changes sent to the project simply because posting a change for
review was a sign of interest from a potential long-term contributor. While new
tools enable more development, it shifts effort from the implementor to the
reviewer, and our policy exists to ensure that we value and do not squander
maintainer time.

Handling Violations
-------------------

If a maintainer judges that a contribution doesn't comply with this policy,
they should paste the following response to request changes:

.. code-block:: text

    This PR does not appear to comply with our policy on tool-generated content,
    and requires additional justification for why it is valuable enough to the
    project for us to review it. Please see our developer policy on
    AI-generated contributions:
    https://gdal.org/community/_ai_tool_policy.html

The best ways to make a change less extractive and more valuable are to reduce
its size or complexity or to increase its usefulness to the community. These
factors are impossible to weigh objectively, and our project policy leaves this
determination up to the maintainers of the project, i.e. those who are doing
the work of sustaining the project.

If/or when it becomes clear that a GitHub issue or PR is off-track and not
moving in the right direction, maintainers should apply the `extractive` label
to help other reviewers prioritize their review time.

If a contributor fails to make their change meaningfully less extractive,
maintainers may lock the conversation and/or close the pull request/issue/RFC.
In case of repeated violations of our policy, the GDAL project reserves itself
the right to ban temporarily or definitely the infringing person/account.

Copyright
---------

Artificial intelligence systems raise many questions around copyright that have
yet to be answered. Our policy on AI tools is similar to our copyright policy:
Contributors are responsible for ensuring that they have the right to
contribute code under the terms of our license, typically meaning that either
they, their employer, or their collaborators hold the copyright. Using AI tools
to regenerate copyrighted material does not remove the copyright, and
contributors are responsible for ensuring that such material does not appear in
their contributions. Contributions found to violate this policy will be removed
just like any other offending contribution. If a reviewer has doubts about the
legal aspects of a contribution, they may ask the contributor to provide more
details on the origins of a particular piece of code.

Credits for this document
-------------------------

This document is a quasi direct adaptation from the
`LLVM software "AI Tool Use Policy" <https://github.com/llvm/llvm-project/blob/main/llvm/docs/AIToolPolicy.md>`__,
and due credits go to its original authors: Reid Kleckner, Hubert Tong and
"maflcko"

.. below is an allow-list for spelling checker.

.. spelling:word-list::
    Reid
    Kleckner
    Hubert
    Tong
    maflcko
    LLM
    unreviewed
    Eghbal
    implementor
