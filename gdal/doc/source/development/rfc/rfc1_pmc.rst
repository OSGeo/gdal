.. _rfc-1:

==============================================
RFC 1: Project Management Committee Guidelines
==============================================

Author: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Adopted

Summary
-------

This document describes how the GDAL/OGR Project Management Committee
determines membership, and makes decisions on GDAL/OGR project issues.

In brief the committee votes on proposals on gdal-dev. Proposals are
available for review for at least two days, and a single veto is
sufficient to delay progress though ultimately a majority of members can
pass a proposal.

Detailed Process
----------------

1.  Proposals are written up and submitted on the gdal-dev mailing list
    for discussion and voting, by any interested party, not just
    committee members.
2.  Proposals need to be available for review for at least two business
    days before a final decision can be made.
3.  Respondents may vote "+1" to indicate support for the proposal and a
    willingness to support implementation.
4.  Respondents may vote "-1" to veto a proposal, but must provide clear
    reasoning and alternate approaches to resolving the problem within
    the two days.
5.  A vote of -0 indicates mild disagreement, but has no effect. A 0
    indicates no opinion. A +0 indicate mild support, but has no effect.
6.  Anyone may comment on proposals on the list, but only members of the
    Project Management Committee's votes will be counted.
7.  A proposal will be accepted if it receives +2 (including the
    proposer) and no vetos (-1).
8.  If a proposal is vetoed, and it cannot be revised to satisfy all
    parties, then it can be resubmitted for an override vote in which a
    majority of all eligible voters indicating +1 is sufficient to pass
    it. Note that this is a majority of all committee members, not just
    those who actively vote.
9.  Upon completion of discussion and voting the proposer should
    announce whether they are proceeding (proposal accepted) or are
    withdrawing their proposal (vetoed).
10. The Chair gets a vote.
11. The Chair is responsible for keeping track of who is a member of the
    Project Management Committee.
12. Addition and removal of members from the committee, as well as
    selection of a Chair should be handled as a proposal to the
    committee. The selection of a new Chair also requires approval of
    the OSGeo board.
13. The Chair adjudicates in cases of disputes about voting.

When is Vote Required?
----------------------

-  Anything that could cause backward compatibility issues.
-  Adding substantial amounts of new code.
-  Changing inter-subsystem APIs, or objects.
-  Issues of procedure.
-  When releases should take place.
-  Anything that might be controversial.

Observations
------------

-  The Chair is the ultimate adjudicator if things break down.
-  The absolute majority rule can be used to override an obstructionist
   veto, but it is intended that in normal circumstances vetoers need to
   be convinced to withdraw their veto. We are trying to reach
   consensus.

Bootstrapping
-------------

Frank Warmerdam is declared initial Chair of the Project Management
Committee.

Daniel Morissette, Frank Warmerdam, Andrey Kiselev and Howard Butler are
declared to be the founding Project Management Committee. The current
membership list can be found on the :ref:`psc` page.
