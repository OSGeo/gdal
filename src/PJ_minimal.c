/***********************************************************************

   A minimal example of a new proj.4 projection implementation

   ...and a verbose justification for some highly intrusive code
   surgery

************************************************************************

**The brief version:**

In an attempt to make proj.4 code slightly more secure and much easier
to read and maintain, I'm trying to eliminate a few unfortunate design
decisions from the early days of proj.4

The work will be *very* intrusive, especially in the PJ_xxx segment of
the code tree, but great care has been taken to design a process that
can be implemented stepwise and localized, one projection at a time,
then finalized with a relatively small and concentrated work package.

**The (very) long version:**

Gerald I. Evenden's original design for the proj.4 projection system
is a beautiful example of software architecture, where a very limited
set of policy rules leads to a well defined hierarchical structure and
a high degree of both encapsulation and internal interoperability.

In the proj.4 code, the policy rules are *enforced* by a system of
preprocessor macros for building the scaffolding for implementation
of a new projection.

While this system of macros undeniably possesses the property of both
reducing repetitive code and enforcing policy, unfortunately it also
possesses two much less desirable properties:

First, while enforcing policy, it also *hides* policy: The "beauty in
simplicity" of Gerald's design is hidden behind layers of macros,
whose architectural clarity do not match that of proj.4 in general.

Second (and related), the macros make the source code look like
something only vaguely related to C, making it hard to read (an effect
that gets amplified to the tune of syntax highlighters getting confused
by the macros).

While the policy rule enforcement macros can be eliminated in relatively
non-intrusive ways, a more fundamental flaw in the proj.4 use of macros
is found in the PJ_xxx.c files implementing the individual projections:
The use of internal redefinition of PJ, the fundamental proj data object,
through the use of the PROJ_PARMS__ macro, makes the sizeof (PJ)
fundamentally unknown to the calling pj_init function.

This leads to code that is probably not in full conformance with the
C standard.

It is also a memory management catastrophe waiting to happen.

But first and foremost, it leads to some very clumsy initialization code,
where pj_init (the constructor function), needs to start the constsruction
process by asking the PJ_xxx function to do the memory allocation (because
pj_init does not know the size of the PROJ_PARMS-mangled PJ object being
instantiated).

Then, after doing some initialization work, pj_init returns control to
PJ_xxx, asking it to finalize the initialization with the projection
specific parameters specified by the PROJ_PARMS__ macro.

Behind the scenes, hidden by two layers of macros, what happens is even
worse, as a lot of the initialization code is duplicated in every PJ_xxx
file, rather than being centralized in the pj_init function.

**Solution procedure:**

Evidently, the way to eliminate this clumsyness will be to introduce an
opaque object, that is managed by tne individual PJ_xxx projection code,
and represented as a simple void-pointer in the PJ object.

This can be done one projection code file at a time, working through the
code base as time permits (it will take at least a month).

When a PJ_xxx file is on the surgical bench, it will also have its
ENTRYA/ENTRY0/ENTRY1/ENTRY2/ENDENTRY/etc. etc. macro-guts torn out and
replaced by the PROJECTION macro (introduced in projects.h).

This leads to code that looks a lot more like real C, and hence is much
less confusing to both syntax higlighters and humans. It also leads
to code that, after all projections have been processed, with a final
sweep over the code base can be brought into the style of the code in
PJ_minimal.c

In my humble opinion the result wil be a code base that is not only easier
to maintain, but also more welcoming to new contributors.

And if proj is to expand its strong basis in projections into the fields
of geodetic transformations and general geometric geodesy, we will need
to be able to attract quite a few expert geodesist contributors.

And since expert geodesists are not necessarily expert coders, a welcoming
code base is a real asset (to put the icing on the cake of the already
welcoming user- and developer community).

Note that the entire process does not touch the algorithmic/mathematical
parts of the code at all - it is actuallly an attempt to make this part
stand out more clearly.

---

The attached material is an attempt to show what happens if we remove
the layers of macros, and introduce a more centralized approach to
memory allocation and initialization.

Please note, however, that the level of cantralization achieved here
is not yet fully supported by the proj.4 infrastructure: It is an
example, intended to show what can be achieved through a smooth,
gradual and safe refactoring of the existing layered macro system.

In my humble opinion, this version makes the beauty of Gerald's design
much more evident than the current layered-macro-version.

Thomas Knudsen, thokn@sdfe.dk, 2016-03-31

***********************************************************************/

#define PJ_LIB__
#include	<projects.h>
#include <assert.h>
PROJ_HEAD(minimal, "Minimal example (brief description goes here)");


/* Projection specific elements for the PJ object */
struct pj_opaque {
	double a;
	int b;
};


static XY e_forward (LP lp, PJ *P) {          /* Ellipsoidal, forward */
    XY xy = {0.0,0.0};
    /* Actual ellipsoidal forward code goes here */
    xy.y = lp.lam + P->es;
    xy.x = lp.phi + 42;
	return xy;
}


static XY s_forward (LP lp, PJ *P) {           /* Spheroidal, forward */
    XY xy = {0.0,0.0};
    /* Actual spheroidal forward code goes here */
    xy.y = lp.lam + P->es;
    xy.x = lp.phi + 42;
	return xy;
}


static LP e_inverse (XY xy, PJ *P) {          /* Ellipsoidal, inverse */
    LP lp = {0.0,0.0};
    /* Actual ellipsoidal forward code goes here */
    lp.lam = xy.x - P->es;
    lp.phi = xy.y - P->opaque->b;
	return lp;
}


static LP s_inverse (XY xy, PJ *P) {           /* Spheroidal, inverse */
    LP lp = {0.0,0.0};
    /* Actual spheroidal forward code goes here */
    lp.lam = xy.x - P->es;
    lp.phi = xy.y - P->opaque->b;
	return lp;
}


static void freeup(PJ *P) {                                    /* Destructor */
    if (P==0)
        return;
    /* Projection specific deallocation goes here */
    pj_dealloc (P->opaque);
    pj_dealloc (P);
    return;
}


PJ *pj_projection_specific_setup_minimal (PJ *P) {
    pj_prepare (P, des_minimal, freeup, sizeof (struct pj_opaque));
    if (0==P->opaque) {
        freeup (P);
        return 0;
    }

    P->opaque->a = 42.42;
    P->opaque->b = 42;

    /* Spheroidal? */
	if (0==P->es) {
		P->fwd = s_forward;
		P->inv = s_inverse;
        return P;
    }

    /* Otherwise it's ellipsoidal */
    P->fwd = e_forward;
	P->inv = e_inverse;

    return P;
}
