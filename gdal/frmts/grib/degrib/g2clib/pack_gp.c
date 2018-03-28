/* pack_gp.f -- translated by f2c (version 20031025).
   You must link the resulting object file with libf2c:
	on Microsoft Windows system, link with libf2c.lib;
	on Linux or Unix systems, link with .../path/to/libf2c.a -lm
	or, if you install libf2c.a in a standard place, with -lf2c -lm
	-- in that order, at the end of the command line, as in
		cc *.o -lf2c -lm
	Source for libf2c is in /netlib/f2c/libf2c.zip, e.g.,

		http://www.netlib.org/f2c/libf2c.zip
*/

/*#include "f2c.h"*/
#include "cpl_port.h"
#include <limits.h>
#include <stdlib.h>
#include "grib2.h"
typedef g2int logical;
#define TRUE_ (1)
#define FALSE_ (0)

/* Subroutine */ int pack_gp(integer *kfildo, integer *ic, integer *nxy,
	integer *is523, integer *minpk, integer *inc, integer *missp, integer
	*misss, integer *jmin, integer *jmax, integer *lbit, integer *nov,
	integer *ndg, integer *lx, integer *ibit, integer *jbit, integer *
	kbit, integer *novref, integer *lbitref, integer *ier)
{
    /* Initialized data */

    const  integer mallow = 1073741825;   /*  MALLOW=2**30+1  */
    integer ifeed = 12;
    integer ifirst = 0;

    /* System generated locals */
    integer i__1, i__2, i__3;

    /* Local variables */
    integer j, k, l;
    logical adda;
    integer ired, kinc, mina, maxa, minb = 0, maxb = 0, minc = 0, maxc = 0, ibxx2[31];
    char cfeed[1];
    integer nenda, nendb = 0, ibita, ibitb = 0, minak, minbk = 0, maxak, maxbk = 0,
	    minck = 0, maxck, nouta = 0, lmiss, itest = 0, nount = 0;
    extern /* Subroutine */ int reduce(integer *, integer *, integer *,
	    integer *, integer *, integer *, integer *, integer *, integer *,
	    integer *, integer *, integer *, integer *);
    integer ibitbs = 0, mislla, misllb = 0, misllc = 0, iersav = 0, lminpk, ktotal,
	    kounta, kountb = 0, kstart, mstart = 0, mintst = 0, maxtst = 0,
	    kounts = 0, mintstk = 0, maxtstk = 0;
    integer *misslx;


/*        FEBRUARY 1994   GLAHN   TDL   MOS-2000 */
/*        JUNE     1995   GLAHN   MODIFIED FOR LMISS ERROR. */
/*        JULY     1996   GLAHN   ADDED MISSS */
/*        FEBRUARY 1997   GLAHN   REMOVED 4 REDUNDANT TESTS FOR */
/*                                MISSP.EQ.0; INSERTED A TEST TO BETTER */
/*                                HANDLE A STRING OF 9999'S */
/*        FEBRUARY 1997   GLAHN   ADDED LOOPS TO ELIMINATE TEST FOR */
/*                                MISSS WHEN MISSS = 0 */
/*        MARCH    1997   GLAHN   CORRECTED FOR SECONDARY MISSING VALUE */
/*        MARCH    1997   GLAHN   CORRECTED FOR USE OF LOCAL VALUE */
/*                                OF MINPK */
/*        MARCH    1997   GLAHN   CORRECTED FOR SECONDARY MISSING VALUE */
/*        MARCH    1997   GLAHN   CHANGED CALCULATING NUMBER OF BITS */
/*                                THROUGH EXPONENTS TO AN ARRAY (IMPROVED */
/*                                OVERALL PACKING PERFORMANCE BY ABOUT */
/*                                35 PERCENT!).  ALLOWED 0 BITS FOR */
/*                                PACKING JMIN( ), LBIT( ), AND NOV( ). */
/*        MAY      1997   GLAHN   A NUMBER OF CHANGES FOR EFFICIENCY. */
/*                                MOD FUNCTIONS ELIMINATED AND ONE */
/*                                IFTHEN ADDED.  JOUNT REMOVED. */
/*                                RECOMPUTATION OF BITS NOT MADE UNLESS */
/*                                NECESSARY AFTER MOVING POINTS FROM */
/*                                ONE GROUP TO ANOTHER.  NENDB ADJUSTED */
/*                                TO ELIMINATE POSSIBILITY OF VERY */
/*                                SMALL GROUP AT THE END. */
/*                                ABOUT 8 PERCENT IMPROVEMENT IN */
/*                                OVERALL PACKING.  ISKIPA REMOVED; */
/*                                THERE IS ALWAYS A GROUP B THAT CAN */
/*                                BECOME GROUP A.  CONTROL ON SIZE */
/*                                OF GROUP B (STATEMENT BELOW 150) */
/*                                ADDED.  ADDED ADDA, AND USE */
/*                                OF GE AND LE INSTEAD OF GT AND LT */
/*                                IN LOOPS BETWEEN 150 AND 160. */
/*                                IBITBS ADDED TO SHORTEN TRIPS */
/*                                THROUGH LOOP. */
/*        MARCH    2000   GLAHN   MODIFIED FOR GRIB2; CHANGED NAME FROM */
/*                                PACKGP */
/*        JANUARY  2001   GLAHN   COMMENTS; IER = 706 SUBSTITUTED FOR */
/*                                STOPS; ADDED RETURN1; REMOVED STATEMENT */
/*                                NUMBER 110; ADDED IER AND * RETURN */
/*        NOVEMBER 2001   GLAHN   CHANGED SOME DIAGNOSTIC FORMATS TO */
/*                                ALLOW PRINTING LARGER NUMBERS */
/*        NOVEMBER 2001   GLAHN   ADDED MISSLX( ) TO PUT MAXIMUM VALUE */
/*                                INTO JMIN( ) WHEN ALL VALUES MISSING */
/*                                TO AGREE WITH GRIB STANDARD. */
/*        NOVEMBER 2001   GLAHN   CHANGED TWO TESTS ON MISSP AND MISSS */
/*                                EQ 0 TO TESTS ON IS523.  HOWEVER, */
/*                                MISSP AND MISSS CANNOT IN GENERAL BE */
/*                                = 0. */
/*        NOVEMBER 2001   GLAHN   ADDED CALL TO REDUCE; DEFINED ITEST */
/*                                BEFORE LOOPS TO REDUCE COMPUTATION; */
/*                                STARTED LARGE GROUP WHEN ALL SAME */
/*                                VALUE */
/*        DECEMBER 2001   GLAHN   MODIFIED AND ADDED A FEW COMMENTS */
/*        JANUARY  2002   GLAHN   REMOVED LOOP BEFORE 150 TO DETERMINE */
/*                                A GROUP OF ALL SAME VALUE */
/*        JANUARY  2002   GLAHN   CHANGED MALLOW FROM 9999999 TO 2**30+1, */
/*                                AND MADE IT A PARAMETER */
/*        MARCH    2002   GLAHN   ADDED NON FATAL IER = 716, 717; */
/*                                REMOVED NENDB=NXY ABOVE 150; */
/*                                ADDED IERSAV=0; COMMENTS */

/*        PURPOSE */
/*            DETERMINES GROUPS OF VARIABLE SIZE, BUT AT LEAST OF */
/*            SIZE MINPK, THE ASSOCIATED MAX (JMAX( )) AND MIN (JMIN( )), */
/*            THE NUMBER OF BITS NECESSARY TO HOLD THE VALUES IN EACH */
/*            GROUP (LBIT( )), THE NUMBER OF VALUES IN EACH GROUP */
/*            (NOV( )), THE NUMBER OF BITS NECESSARY TO PACK THE JMIN( ) */
/*            VALUES (IBIT), THE NUMBER OF BITS NECESSARY TO PACK THE */
/*            LBIT( ) VALUES (JBIT), AND THE NUMBER OF BITS NECESSARY */
/*            TO PACK THE NOV( ) VALUES (KBIT).  THE ROUTINE IS DESIGNED */
/*            TO DETERMINE THE GROUPS SUCH THAT A SMALL NUMBER OF BITS */
/*            IS NECESSARY TO PACK THE DATA WITHOUT EXCESSIVE */
/*            COMPUTATIONS.  IF ALL VALUES IN THE GROUP ARE ZERO, THE */
/*            NUMBER OF BITS TO USE IN PACKING IS DEFINED AS ZERO WHEN */
/*            THERE CAN BE NO MISSING VALUES; WHEN THERE CAN BE MISSING */
/*            VALUES, THE NUMBER OF BITS MUST BE AT LEAST 1 TO HAVE */
/*            THE CAPABILITY TO RECOGNIZE THE MISSING VALUE.  HOWEVER, */
/*            IF ALL VALUES IN A GROUP ARE MISSING, THE NUMBER OF BITS */
/*            NEEDED IS 0, AND THE UNPACKER RECOGNIZES THIS. */
/*            ALL VARIABLES ARE INTEGER.  EVEN THOUGH THE GROUPS ARE */
/*            INITIALLY OF SIZE MINPK OR LARGER, AN ADJUSTMENT BETWEEN */
/*            TWO GROUPS (THE LOOKBACK PROCEDURE) MAY MAKE A GROUP */
/*            SMALLER THAN MINPK.  THE CONTROL ON GROUP SIZE IS THAT */
/*            THE SUM OF THE SIZES OF THE TWO CONSECUTIVE GROUPS, EACH OF */
/*            SIZE MINPK OR LARGER, IS NOT DECREASED.  WHEN DETERMINING */
/*            THE NUMBER OF BITS NECESSARY FOR PACKING, THE LARGEST */
/*            VALUE THAT CAN BE ACCOMMODATED IN, SAY, MBITS, IS */
/*            2**MBITS-1; THIS LARGEST VALUE (AND THE NEXT SMALLEST */
/*            VALUE) IS RESERVED FOR THE MISSING VALUE INDICATOR (ONLY) */
/*            WHEN IS523 NE 0.  IF THE DIMENSION NDG */
/*            IS NOT LARGE ENOUGH TO HOLD ALL THE GROUPS, THE LOCAL VALUE */
/*            OF MINPK IS INCREASED BY 50 PERCENT.  THIS IS REPEATED */
/*            UNTIL NDG WILL SUFFICE.  A DIAGNOSTIC IS PRINTED WHENEVER */
/*            THIS HAPPENS, WHICH SHOULD BE VERY RARELY.  IF IT HAPPENS */
/*            OFTEN, NDG IN SUBROUTINE PACK SHOULD BE INCREASED AND */
/*            A CORRESPONDING INCREASE IN SUBROUTINE UNPACK MADE. */
/*            CONSIDERABLE CODE IS PROVIDED SO THAT NO MORE CHECKING */
/*            FOR MISSING VALUES WITHIN LOOPS IS DONE THAN NECESSARY; */
/*            THE ADDED EFFICIENCY OF THIS IS RELATIVELY MINOR, */
/*            BUT DOES NO HARM.  FOR GRIB2, THE REFERENCE VALUE FOR */
/*            THE LENGTH OF GROUPS IN NOV( ) AND FOR THE NUMBER OF */
/*            BITS NECESSARY TO PACK GROUP VALUES ARE DETERMINED, */
/*            AND SUBTRACTED BEFORE JBIT AND KBIT ARE DETERMINED. */

/*            WHEN 1 OR MORE GROUPS ARE LARGE COMPARED TO THE OTHERS, */
/*            THE WIDTH OF ALL GROUPS MUST BE AS LARGE AS THE LARGEST. */
/*            A SUBROUTINE REDUCE BREAKS UP LARGE GROUPS INTO 2 OR */
/*            MORE TO REDUCE TOTAL BITS REQUIRED.  IF REDUCE SHOULD */
/*            ABORT, PACK_GP WILL BE EXECUTED AGAIN WITHOUT THE CALL */
/*            TO REDUCE. */

/*        DATA SET USE */
/*           KFILDO - UNIT NUMBER FOR OUTPUT (PRINT) FILE. (OUTPUT) */

/*        VARIABLES IN CALL SEQUENCE */
/*              KFILDO = UNIT NUMBER FOR OUTPUT (PRINT) FILE.  (INPUT) */
/*               IC( ) = ARRAY TO HOLD DATA FOR PACKING.  THE VALUES */
/*                       DO NOT HAVE TO BE POSITIVE AT THIS POINT, BUT */
/*                       MUST BE IN THE RANGE -2**30 TO +2**30 (THE */
/*                       THE VALUE OF MALLOW).  THESE INTEGER VALUES */
/*                       WILL BE RETAINED EXACTLY THROUGH PACKING AND */
/*                       UNPACKING.  (INPUT) */
/*                 NXY = NUMBER OF VALUES IN IC( ).  ALSO TREATED */
/*                       AS ITS DIMENSION.  (INPUT) */
/*              IS523  = missing value management */
/*                       0=data contains no missing values */
/*                       1=data contains Primary missing values */
/*                       2=data contains Primary and secondary missing values */
/*                       (INPUT) */
/*               MINPK = THE MINIMUM SIZE OF EACH GROUP, EXCEPT POSSIBLY */
/*                       THE LAST ONE.  (INPUT) */
/*                 INC = THE NUMBER OF VALUES TO ADD TO AN ALREADY */
/*                       EXISTING GROUP IN DETERMINING WHETHER OR NOT */
/*                       TO START A NEW GROUP.  IDEALLY, THIS WOULD BE */
/*                       1, BUT EACH TIME INC VALUES ARE ATTEMPTED, THE */
/*                       MAX AND MIN OF THE NEXT MINPK VALUES MUST BE */
/*                       FOUND.  THIS IS "A LOOP WITHIN A LOOP," AND */
/*                       A SLIGHTLY LARGER VALUE MAY GIVE ABOUT AS GOOD */
/*                       RESULTS WITH SLIGHTLY LESS COMPUTATIONAL TIME. */
/*                       IF INC IS LE 0, 1 IS USED, AND A DIAGNOSTIC IS */
/*                       OUTPUT.  NOTE:  IT IS EXPECTED THAT INC WILL */
/*                       EQUAL 1.  THE CODE USES INC PRIMARILY IN THE */
/*                       LOOPS STARTING AT STATEMENT 180.  IF INC */
/*                       WERE 1, THERE WOULD NOT NEED TO BE LOOPS */
/*                       AS SUCH.  HOWEVER, KINC (THE LOCAL VALUE OF */
/*                       INC) IS SET GE 1 WHEN NEAR THE END OF THE DATA */
/*                       TO FORESTALL A VERY SMALL GROUP AT THE END. */
/*                       (INPUT) */
/*               MISSP = WHEN MISSING POINTS CAN BE PRESENT IN THE DATA, */
/*                       THEY WILL HAVE THE VALUE MISSP OR MISSS. */
/*                       MISSP IS THE PRIMARY MISSING VALUE AND  MISSS */
/*                       IS THE SECONDARY MISSING VALUE .  THESE MUST */
/*                       NOT BE VALUES THAT WOULD OCCUR WITH SUBTRACTING */
/*                       THE MINIMUM (REFERENCE) VALUE OR SCALING. */
/*                       FOR EXAMPLE, MISSP = 0 WOULD NOT BE ADVISABLE. */
/*                       (INPUT) */
/*               MISSS = SECONDARY MISSING VALUE INDICATOR (SEE MISSP). */
/*                       (INPUT) */
/*             JMIN(J) = THE MINIMUM OF EACH GROUP (J=1,LX).  (OUTPUT) */
/*             JMAX(J) = THE MAXIMUM OF EACH GROUP (J=1,LX).  THIS IS */
/*                       NOT REALLY NEEDED, BUT SINCE THE MAX OF EACH */
/*                       GROUP MUST BE FOUND, SAVING IT HERE IS CHEAP */
/*                       IN CASE THE USER WANTS IT.  (OUTPUT) */
/*             LBIT(J) = THE NUMBER OF BITS NECESSARY TO PACK EACH GROUP */
/*                       (J=1,LX).  IT IS ASSUMED THE MINIMUM OF EACH */
/*                       GROUP WILL BE REMOVED BEFORE PACKING, AND THE */
/*                       VALUES TO PACK WILL, THEREFORE, ALL BE POSITIVE. */
/*                       HOWEVER, IC( ) DOES NOT NECESSARILY CONTAIN */
/*                       ALL POSITIVE VALUES.  IF THE OVERALL MINIMUM */
/*                       HAS BEEN REMOVED (THE USUAL CASE), THEN IC( ) */
/*                       WILL CONTAIN ONLY POSITIVE VALUES.  (OUTPUT) */
/*              NOV(J) = THE NUMBER OF VALUES IN EACH GROUP (J=1,LX). */
/*                       (OUTPUT) */
/*                 NDG = THE DIMENSION OF JMIN( ), JMAX( ), LBIT( ), AND */
/*                       NOV( ).  (INPUT) */
/*                  LX = THE NUMBER OF GROUPS DETERMINED.  (OUTPUT) */
/*                IBIT = THE NUMBER OF BITS NECESSARY TO PACK THE JMIN(J) */
/*                       VALUES, J=1,LX.  (OUTPUT) */
/*                JBIT = THE NUMBER OF BITS NECESSARY TO PACK THE LBIT(J) */
/*                       VALUES, J=1,LX.  (OUTPUT) */
/*                KBIT = THE NUMBER OF BITS NECESSARY TO PACK THE NOV(J) */
/*                       VALUES, J=1,LX.  (OUTPUT) */
/*              NOVREF = REFERENCE VALUE FOR NOV( ).  (OUTPUT) */
/*             LBITREF = REFERENCE VALUE FOR LBIT( ).  (OUTPUT) */
/*                 IER = ERROR RETURN. */
/*                       706 = VALUE WILL NOT PACK IN 30 BITS--FATAL */
/*                       714 = ERROR IN REDUCE--NON-FATAL */
/*                       715 = NGP NOT LARGE ENOUGH IN REDUCE--NON-FATAL */
/*                       716 = MINPK INCEASED--NON-FATAL */
/*                       717 = INC SET = 1--NON-FATAL */
/*                       (OUTPUT) */
/*                   * = ALTERNATE RETURN WHEN IER NE 0 AND FATAL ERROR. */

/*        INTERNAL VARIABLES */
/*               CFEED = CONTAINS THE CHARACTER REPRESENTATION */
/*                       OF A PRINTER FORM FEED. */
/*               IFEED = CONTAINS THE INTEGER VALUE OF A PRINTER */
/*                       FORM FEED. */
/*                KINC = WORKING COPY OF INC.  MAY BE MODIFIED. */
/*                MINA = MINIMUM VALUE IN GROUP A. */
/*                MAXA = MAXIMUM VALUE IN GROUP A. */
/*               NENDA = THE PLACE IN IC( ) WHERE GROUP A ENDS. */
/*              KSTART = THE PLACE IN IC( ) WHERE GROUP A STARTS. */
/*               IBITA = NUMBER OF BITS NEEDED TO HOLD VALUES IN GROUP A. */
/*                MINB = MINIMUM VALUE IN GROUP B. */
/*                MAXB = MAXIMUM VALUE IN GROUP B. */
/*               NENDB = THE PLACE IN IC( ) WHERE GROUP B ENDS. */
/*               IBITB = NUMBER OF BITS NEEDED TO HOLD VALUES IN GROUP B. */
/*                MINC = MINIMUM VALUE IN GROUP C. */
/*                MAXC = MAXIMUM VALUE IN GROUP C. */
/*              KTOTAL = COUNT OF NUMBER OF VALUES IN IC( ) PROCESSED. */
/*               NOUNT = NUMBER OF VALUES ADDED TO GROUP A. */
/*               LMISS = 0 WHEN IS523 = 0.  WHEN PACKING INTO A */
/*                       SPECIFIC NUMBER OF BITS, SAY MBITS, */
/*                       THE MAXIMUM VALUE THAT CAN BE HANDLED IS */
/*                       2**MBITS-1.  WHEN IS523 = 1, INDICATING */
/*                       PRIMARY MISSING VALUES, THIS MAXIMUM VALUE */
/*                       IS RESERVED TO HOLD THE PRIMARY MISSING VALUE */
/*                       INDICATOR AND LMISS = 1.  WHEN IS523 = 2, */
/*                       THE VALUE JUST BELOW THE MAXIMUM (I.E., */
/*                       2**MBITS-2) IS RESERVED TO HOLD THE SECONDARY */
/*                       MISSING VALUE INDICATOR AND LMISS = 2. */
/*              LMINPK = LOCAL VALUE OF MINPK.  THIS WILL BE ADJUSTED */
/*                       UPWARD WHENEVER NDG IS NOT LARGE ENOUGH TO HOLD */
/*                       ALL THE GROUPS. */
/*              MALLOW = THE LARGEST ALLOWABLE VALUE FOR PACKING. */
/*              MISLLA = SET TO 1 WHEN ALL VALUES IN GROUP A ARE MISSING. */
/*                       THIS IS USED TO DISTINGUISH BETWEEN A REAL */
/*                       MINIMUM WHEN ALL VALUES ARE NOT MISSING */
/*                       AND A MINIMUM THAT HAS BEEN SET TO ZERO WHEN */
/*                       ALL VALUES ARE MISSING.  0 OTHERWISE. */
/*                       NOTE THAT THIS DOES NOT DISTINGUISH BETWEEN */
/*                       PRIMARY AND SECONDARY MISSING WHEN SECONDARY */
/*                       MISSING ARE PRESENT.  THIS MEANS THAT */
/*                       LBIT( ) WILL NOT BE ZERO WITH THE RESULTING */
/*                       COMPRESSION EFFICIENCY WHEN SECONDARY MISSING */
/*                       ARE PRESENT.  ALSO NOTE THAT A CHECK HAS BEEN */
/*                       MADE EARLIER TO DETERMINE THAT SECONDARY */
/*                       MISSING ARE REALLY THERE. */
/*              MISLLB = SET TO 1 WHEN ALL VALUES IN GROUP B ARE MISSING. */
/*                       THIS IS USED TO DISTINGUISH BETWEEN A REAL */
/*                       MINIMUM WHEN ALL VALUES ARE NOT MISSING */
/*                       AND A MINIMUM THAT HAS BEEN SET TO ZERO WHEN */
/*                       ALL VALUES ARE MISSING.  0 OTHERWISE. */
/*              MISLLC = PERFORMS THE SAME FUNCTION FOR GROUP C THAT */
/*                       MISLLA AND MISLLB DO FOR GROUPS B AND C, */
/*                       RESPECTIVELY. */
/*            IBXX2(J) = AN ARRAY THAT WHEN THIS ROUTINE IS FIRST ENTERED */
/*                       IS SET TO 2**J, J=0,30. IBXX2(30) = 2**30, WHICH */
/*                       IS THE LARGEST VALUE PACKABLE, BECAUSE 2**31 */
/*                       IS LARGER THAN THE INTEGER WORD SIZE. */
/*              IFIRST = SET BY DATA STATEMENT TO 0.  CHANGED TO 1 ON */
/*                       FIRST */
/*                       ENTRY WHEN IBXX2( ) IS FILLED. */
/*               MINAK = KEEPS TRACK OF THE LOCATION IN IC( ) WHERE THE */
/*                       MINIMUM VALUE IN GROUP A IS LOCATED. */
/*               MAXAK = DOES THE SAME AS MINAK, EXCEPT FOR THE MAXIMUM. */
/*               MINBK = THE SAME AS MINAK FOR GROUP B. */
/*               MAXBK = THE SAME AS MAXAK FOR GROUP B. */
/*               MINCK = THE SAME AS MINAK FOR GROUP C. */
/*               MAXCK = THE SAME AS MAXAK FOR GROUP C. */
/*                ADDA = KEEPS TRACK WHETHER OR NOT AN ATTEMPT TO ADD */
/*                       POINTS TO GROUP A WAS MADE.  IF SO, THEN ADDA */
/*                       KEEPS FROM TRYING TO PUT ONE BACK INTO B. */
/*                       (LOGICAL) */
/*              IBITBS = KEEPS CURRENT VALUE IF IBITB SO THAT LOOP */
/*                       ENDING AT 166 DOESN'T HAVE TO START AT */
/*                       IBITB = 0 EVERY TIME. */
/*           MISSLX(J) = MALLOW EXCEPT WHEN A GROUP IS ALL ONE VALUE (AND */
/*                       LBIT(J) = 0) AND THAT VALUE IS MISSING.  IN */
/*                       THAT CASE, MISSLX(J) IS MISSP OR MISSS.  THIS */
/*                       GETS INSERTED INTO JMIN(J) LATER AS THE */
/*                       MISSING INDICATOR; IT CAN'T BE PUT IN UNTIL */
/*                       THE END, BECAUSE JMIN( ) IS USED TO CALCULATE */
/*                       THE MAXIMUM NUMBER OF BITS (IBITS) NEEDED TO */
/*                       PACK JMIN( ). */
/*        1         2         3         4         5         6         7 X */

/*        NON SYSTEM SUBROUTINES CALLED */
/*           NONE */



/*        MISSLX( ) was AN AUTOMATIC ARRAY. */
    misslx = (integer *)calloc(*ndg,sizeof(integer));
    if( misslx == NULL )
    {
        *ier = -1;
        return 0;
    }

    /* Parameter adjustments */
    --ic;
    --nov;
    --lbit;
    --jmax;
    --jmin;

    /* Function Body */

    *ier = 0;
    iersav = 0;
/*     CALL TIMPR(KFILDO,KFILDO,'START PACK_GP        ') */
    *(unsigned char *)cfeed = (char) ifeed;

    ired = 0;
/*        IRED IS A FLAG.  WHEN ZERO, REDUCE WILL BE CALLED. */
/*        IF REDUCE ABORTS, IRED = 1 AND IS NOT CALLED.  IN */
/*        THIS CASE PACK_GP EXECUTES AGAIN EXCEPT FOR REDUCE. */

    if (*inc <= 0) {
	iersav = 717;
/*        WRITE(KFILDO,101)INC */
/* 101     FORMAT(/' ****INC ='I8,' NOT CORRECT IN PACK_GP.  1 IS USED.') */
    }

/*        THERE WILL BE A RESTART OF PACK_GP IF SUBROUTINE REDUCE */
/*        ABORTS.  THIS SHOULD NOT HAPPEN, BUT IF IT DOES, PACK_GP */
/*        WILL COMPLETE WITHOUT SUBROUTINE REDUCE.  A NON FATAL */
/*        DIAGNOSTIC RETURN IS PROVIDED. */

L102:
    /*kinc = max(*inc,1);*/
    kinc = (*inc > 1) ? *inc : 1;
    lminpk = *minpk;

/*         CALCULATE THE POWERS OF 2 THE FIRST TIME ENTERED. */

    if (ifirst == 0) {
	ifirst = 1;
	ibxx2[0] = 1;

	for (j = 1; j <= 30; ++j) {
	    ibxx2[j] = ibxx2[j - 1] << 1;
/* L104: */
	}

    }

/*        THERE WILL BE A RESTART AT 105 IS NDG IS NOT LARGE ENOUGH. */
/*        A NON FATAL DIAGNOSTIC RETURN IS PROVIDED. */

L105:
    kstart = 1;
    ktotal = 0;
    *lx = 0;
    adda = FALSE_;
    lmiss = 0;
    if (*is523 == 1) {
	lmiss = 1;
    }
    if (*is523 == 2) {
	lmiss = 2;
    }

/*        ************************************* */

/*        THIS SECTION COMPUTES STATISTICS FOR GROUP A.  GROUP A IS */
/*        A GROUP OF SIZE LMINPK. */

/*        ************************************* */

    ibita = 0;
    mina = mallow;
    maxa = -mallow;
    minak = mallow;
    maxak = -mallow;

/*        FIND THE MIN AND MAX OF GROUP A.  THIS WILL INITIALLY BE OF */
/*        SIZE LMINPK (IF THERE ARE STILL LMINPK VALUES IN IC( )), BUT */
/*        WILL INCREASE IN SIZE IN INCREMENTS OF INC UNTIL A NEW */
/*        GROUP IS STARTED.  THE DEFINITION OF GROUP A IS DONE HERE */
/*        ONLY ONCE (UPON INITIAL ENTRY), BECAUSE A GROUP B CAN ALWAYS */
/*        BECOME A NEW GROUP A AFTER A IS PACKED, EXCEPT IF LMINPK */
/*        HAS TO BE INCREASED BECAUSE NDG IS TOO SMALL.  THEREFORE, */
/*        THE SEPARATE LOOPS FOR MISSING AND NON-MISSING HERE BUYS */
/*        ALMOST NOTHING. */

/* Computing MIN */
    i__1 = kstart + lminpk - 1;
    /*nenda = min(i__1,*nxy);*/
    nenda = (i__1 < *nxy) ? i__1 : *nxy;
    if (*nxy - nenda <= lminpk / 2) {
	nenda = *nxy;
    }
/*        ABOVE STATEMENT GUARANTEES THE LAST GROUP IS GT LMINPK/2 BY */
/*        MAKING THE ACTUAL GROUP LARGER.  IF A PROVISION LIKE THIS IS */
/*        NOT INCLUDED, THERE WILL MANY TIMES BE A VERY SMALL GROUP */
/*        AT THE END.  USE SEPARATE LOOPS FOR MISSING AND NO MISSING */
/*        VALUES FOR EFFICIENCY. */

/*        DETERMINE WHETHER THERE IS A LONG STRING OF THE SAME VALUE */
/*        UNLESS NENDA = NXY.  THIS MAY ALLOW A LARGE GROUP A TO */
/*        START WITH, AS WITH MISSING VALUES.   SEPARATE LOOPS FOR */
/*        MISSING OPTIONS.  THIS SECTION IS ONLY EXECUTED ONCE, */
/*        IN DETERMINING THE FIRST GROUP.  IT HELPS FOR AN ARRAY */
/*        OF MOSTLY MISSING VALUES OR OF ONE VALUE, SUCH AS */
/*        RADAR OR PRECIP DATA. */

    if (nenda != *nxy && ic[kstart] == ic[kstart + 1]) {
/*           NO NEED TO EXECUTE IF FIRST TWO VALUES ARE NOT EQUAL. */

	if (*is523 == 0) {
/*              THIS LOOP IS FOR NO MISSING VALUES. */

	    i__1 = *nxy;
	    for (k = kstart + 1; k <= i__1; ++k) {

		if (ic[k] != ic[kstart]) {
/* Computing MAX */
		    i__2 = nenda;
            i__3 = k - 1;
		    /*nenda = max(i__2,i__3);*/
		    nenda = (i__2 > i__3) ? i__2 : i__3;
		    goto L114;
		}

/* L111: */
	    }

	    nenda = *nxy;
/*              FALL THROUGH THE LOOP MEANS ALL VALUES ARE THE SAME. */

	} else if (*is523 == 1) {
/*              THIS LOOP IS FOR PRIMARY MISSING VALUES ONLY. */

	    i__1 = *nxy;
	    for (k = kstart + 1; k <= i__1; ++k) {

		if (ic[k] != *missp) {

		    if (ic[k] != ic[kstart]) {
/* Computing MAX */
			i__2 = nenda;
            i__3 = k - 1;
			/*nenda = max(i__2,i__3);*/
			nenda = (i__2 > i__3) ? i__2 : i__3;
			goto L114;
		    }

		}

/* L112: */
	    }

	    nenda = *nxy;
/*              FALL THROUGH THE LOOP MEANS ALL VALUES ARE THE SAME. */

	} else {
/*              THIS LOOP IS FOR PRIMARY AND SECONDARY MISSING VALUES. */

	    i__1 = *nxy;
	    for (k = kstart + 1; k <= i__1; ++k) {

		if (ic[k] != *missp && ic[k] != *misss) {

		    if (ic[k] != ic[kstart]) {
/* Computing MAX */
			i__2 = nenda;
            i__3 = k - 1;
			/*nenda = max(i__2,i__3);*/
			nenda = (i__2 > i__3) ? i__2 : i__3;
			goto L114;
		    }

		}

/* L113: */
	    }

	    nenda = *nxy;
/*              FALL THROUGH THE LOOP MEANS ALL VALUES ARE THE SAME. */
	}

    }

L114:
    if (*is523 == 0) {

	i__1 = nenda;
	for (k = kstart; k <= i__1; ++k) {
	    if (ic[k] < mina) {
		mina = ic[k];
		minak = k;
	    }
	    if (ic[k] > maxa) {
		maxa = ic[k];
		maxak = k;
	    }
/* L115: */
	}

    } else if (*is523 == 1) {

	i__1 = nenda;
	for (k = kstart; k <= i__1; ++k) {
	    if (ic[k] == *missp) {
		goto L117;
	    }
	    if (ic[k] < mina) {
		mina = ic[k];
		minak = k;
	    }
	    if (ic[k] > maxa) {
		maxa = ic[k];
		maxak = k;
	    }
L117:
	    ;
	}

    } else {

	i__1 = nenda;
	for (k = kstart; k <= i__1; ++k) {
	    if (ic[k] == *missp || ic[k] == *misss) {
		goto L120;
	    }
	    if (ic[k] < mina) {
		mina = ic[k];
		minak = k;
	    }
	    if (ic[k] > maxa) {
		maxa = ic[k];
		maxak = k;
	    }
L120:
	    ;
	}

    }

    kounta = nenda - kstart + 1;

/*        INCREMENT KTOTAL AND FIND THE BITS NEEDED TO PACK THE A GROUP. */

    ktotal += kounta;
    mislla = 0;
    if (mina != mallow) {
	goto L125;
    }
/*        ALL MISSING VALUES MUST BE ACCOMMODATED. */
    mina = 0;
    maxa = 0;
    mislla = 1;
    ibitb = 0;
    if (*is523 != 2) {
	goto L130;
    }
/*        WHEN ALL VALUES ARE MISSING AND THERE ARE NO */
/*        SECONDARY MISSING VALUES, IBITA = 0. */
/*        OTHERWISE, IBITA MUST BE CALCULATED. */

L125:
    itest = maxa - mina + lmiss;

    for (ibita = 0; ibita <= 30; ++ibita) {
	if (itest < ibxx2[ibita]) {
	    goto L130;
	}
/* ***        THIS TEST IS THE SAME AS: */
/* ***     IF(MAXA-MINA.LT.IBXX2(IBITA)-LMISS)GO TO 130 */
/* L126: */
    }

/*     WRITE(KFILDO,127)MAXA,MINA */
/* 127  FORMAT(' ****ERROR IN PACK_GP.  VALUE WILL NOT PACK IN 30 BITS.', */
/*    1       '  MAXA ='I13,'  MINA ='I13,'.  ERROR AT 127.') */
    *ier = 706;
    goto L900;

L130:

/* ***D     WRITE(KFILDO,131)KOUNTA,KTOTAL,MINA,MAXA,IBITA,MISLLA */
/* ***D131  FORMAT(' AT 130, KOUNTA ='I8,'  KTOTAL ='I8,'  MINA ='I8, */
/* ***D    1       '  MAXA ='I8,'  IBITA ='I3,'  MISLLA ='I3) */

L133:
    if (ktotal >= *nxy) {
	goto L200;
    }

/*        ************************************* */

/*        THIS SECTION COMPUTES STATISTICS FOR GROUP B.  GROUP B IS A */
/*        GROUP OF SIZE LMINPK IMMEDIATELY FOLLOWING GROUP A. */

/*        ************************************* */

L140:
    minb = mallow;
    maxb = -mallow;
    minbk = mallow;
    maxbk = -mallow;
    ibitbs = 0;
    mstart = ktotal + 1;

/*        DETERMINE WHETHER THERE IS A LONG STRING OF THE SAME VALUE. */
/*        THIS WORKS WHEN THERE ARE NO MISSING VALUES. */

    nendb = 1;

    if (mstart < *nxy) {

	if (*is523 == 0) {
/*              THIS LOOP IS FOR NO MISSING VALUES. */

	    i__1 = *nxy;
	    for (k = mstart + 1; k <= i__1; ++k) {

		if (ic[k] != ic[mstart]) {
		    nendb = k - 1;
		    goto L150;
		}

/* L145: */
	    }

	    nendb = *nxy;
/*              FALL THROUGH THE LOOP MEANS ALL REMAINING VALUES */
/*              ARE THE SAME. */
	}

    }

L150:
/* Computing MAX */
/* Computing MIN */
    i__3 = ktotal + lminpk;
    /*i__1 = nendb, i__2 = min(i__3,*nxy);*/
    i__1 = nendb;
    i__2 = (i__3 < *nxy) ? i__3 : *nxy;
    /*nendb = max(i__1,i__2);*/
    nendb = (i__1 > i__2) ? i__1 : i__2;
/* **** 150  NENDB=MIN(KTOTAL+LMINPK,NXY) */

    if (*nxy - nendb <= lminpk / 2) {
	nendb = *nxy;
    }
/*        ABOVE STATEMENT GUARANTEES THE LAST GROUP IS GT LMINPK/2 BY */
/*        MAKING THE ACTUAL GROUP LARGER.  IF A PROVISION LIKE THIS IS */
/*        NOT INCLUDED, THERE WILL MANY TIMES BE A VERY SMALL GROUP */
/*        AT THE END.  USE SEPARATE LOOPS FOR MISSING AND NO MISSING */

/*        USE SEPARATE LOOPS FOR MISSING AND NO MISSING VALUES */
/*        FOR EFFICIENCY. */

    if (*is523 == 0) {

	i__1 = nendb;
	for (k = mstart; k <= i__1; ++k) {
	    if (ic[k] <= minb) {
		minb = ic[k];
/*              NOTE LE, NOT LT.  LT COULD BE USED BUT THEN A */
/*              RECOMPUTE OVER THE WHOLE GROUP WOULD BE NEEDED */
/*              MORE OFTEN.  SAME REASONING FOR GE AND OTHER */
/*              LOOPS BELOW. */
		minbk = k;
	    }
	    if (ic[k] >= maxb) {
		maxb = ic[k];
		maxbk = k;
	    }
/* L155: */
	}

    } else if (*is523 == 1) {

	i__1 = nendb;
	for (k = mstart; k <= i__1; ++k) {
	    if (ic[k] == *missp) {
		goto L157;
	    }
	    if (ic[k] <= minb) {
		minb = ic[k];
		minbk = k;
	    }
	    if (ic[k] >= maxb) {
		maxb = ic[k];
		maxbk = k;
	    }
L157:
	    ;
	}

    } else {

	i__1 = nendb;
	for (k = mstart; k <= i__1; ++k) {
	    if (ic[k] == *missp || ic[k] == *misss) {
		goto L160;
	    }
	    if (ic[k] <= minb) {
		minb = ic[k];
		minbk = k;
	    }
	    if (ic[k] >= maxb) {
		maxb = ic[k];
		maxbk = k;
	    }
L160:
	    ;
	}

    }

    kountb = nendb - ktotal;
    misllb = 0;
    if (minb != mallow) {
	goto L165;
    }
/*        ALL MISSING VALUES MUST BE ACCOMMODATED. */
    minb = 0;
    maxb = 0;
    misllb = 1;
    ibitb = 0;

    if (*is523 != 2) {
	goto L170;
    }
/*        WHEN ALL VALUES ARE MISSING AND THERE ARE NO SECONDARY */
/*        MISSING VALUES, IBITB = 0.  OTHERWISE, IBITB MUST BE */
/*        CALCULATED. */

L165:
    if( (GIntBig)maxb - minb < INT_MIN ||
        (GIntBig)maxb - minb > INT_MAX )
    {
        *ier = -1;
        free(misslx);
        return 0;
    }

    for (ibitb = ibitbs; ibitb <= 30; ++ibitb) {
	if (maxb - minb < ibxx2[ibitb] - lmiss) {
	    goto L170;
	}
/* L166: */
    }

/*     WRITE(KFILDO,167)MAXB,MINB */
/* 167  FORMAT(' ****ERROR IN PACK_GP.  VALUE WILL NOT PACK IN 30 BITS.', */
/*    1       '  MAXB ='I13,'  MINB ='I13,'.  ERROR AT 167.') */
    *ier = 706;
    goto L900;

/*        COMPARE THE BITS NEEDED TO PACK GROUP B WITH THOSE NEEDED */
/*        TO PACK GROUP A.  IF IBITB GE IBITA, TRY TO ADD TO GROUP A. */
/*        IF NOT, TRY TO ADD A'S POINTS TO B, UNLESS ADDITION TO A */
/*        HAS BEEN DONE.  THIS LATTER IS CONTROLLED WITH ADDA. */

L170:

/* ***D     WRITE(KFILDO,171)KOUNTA,KTOTAL,MINA,MAXA,IBITA,MISLLA, */
/* ***D    1                               MINB,MAXB,IBITB,MISLLB */
/* ***D171  FORMAT(' AT 171, KOUNTA ='I8,'  KTOTAL ='I8,'  MINA ='I8, */
/* ***D    1       '  MAXA ='I8,'  IBITA ='I3,'  MISLLA ='I3, */
/* ***D    2       '  MINB ='I8,'  MAXB ='I8,'  IBITB ='I3,'  MISLLB ='I3) */

    if (ibitb >= ibita) {
	goto L180;
    }
    if (adda) {
	goto L200;
    }

/*        ************************************* */

/*        GROUP B REQUIRES LESS BITS THAN GROUP A.  PUT AS MANY OF A'S */
/*        POINTS INTO B AS POSSIBLE WITHOUT EXCEEDING THE NUMBER OF */
/*        BITS NECESSARY TO PACK GROUP B. */

/*        ************************************* */

    kounts = kounta;
/*        KOUNTA REFERS TO THE PRESENT GROUP A. */
    mintst = minb;
    maxtst = maxb;
    mintstk = minbk;
    maxtstk = maxbk;

/*        USE SEPARATE LOOPS FOR MISSING AND NO MISSING VALUES */
/*        FOR EFFICIENCY. */

    if (*is523 == 0) {

	i__1 = kstart;
	for (k = ktotal; k >= i__1; --k) {
/*           START WITH THE END OF THE GROUP AND WORK BACKWARDS. */
	    if (ic[k] < minb) {
		mintst = ic[k];
		mintstk = k;
	    } else if (ic[k] > maxb) {
		maxtst = ic[k];
		maxtstk = k;
	    }
	    if (maxtst - mintst >= ibxx2[ibitb]) {
		goto L174;
	    }
/*           NOTE THAT FOR THIS LOOP, LMISS = 0. */
	    minb = mintst;
	    maxb = maxtst;
	    minbk = mintstk;
	    maxbk = maxtstk;
	    --kounta;
/*           THERE IS ONE LESS POINT NOW IN A. */
/* L1715: */
	}

    } else if (*is523 == 1) {

	i__1 = kstart;
	for (k = ktotal; k >= i__1; --k) {
/*           START WITH THE END OF THE GROUP AND WORK BACKWARDS. */
	    if (ic[k] == *missp) {
		goto L1718;
	    }
	    if (ic[k] < minb) {
		mintst = ic[k];
		mintstk = k;
	    } else if (ic[k] > maxb) {
		maxtst = ic[k];
		maxtstk = k;
	    }
	    if (maxtst - mintst >= ibxx2[ibitb] - lmiss) {
		goto L174;
	    }
/*           FOR THIS LOOP, LMISS = 1. */
	    minb = mintst;
	    maxb = maxtst;
	    minbk = mintstk;
	    maxbk = maxtstk;
	    misllb = 0;
/*           WHEN THE POINT IS NON MISSING, MISLLB SET = 0. */
L1718:
	    --kounta;
/*           THERE IS ONE LESS POINT NOW IN A. */
/* L1719: */
	}

    } else {

	i__1 = kstart;
	for (k = ktotal; k >= i__1; --k) {
/*           START WITH THE END OF THE GROUP AND WORK BACKWARDS. */
	    if (ic[k] == *missp || ic[k] == *misss) {
		goto L1729;
	    }
	    if (ic[k] < minb) {
		mintst = ic[k];
		mintstk = k;
	    } else if (ic[k] > maxb) {
		maxtst = ic[k];
		maxtstk = k;
	    }
	    if (maxtst - mintst >= ibxx2[ibitb] - lmiss) {
		goto L174;
	    }
/*           FOR THIS LOOP, LMISS = 2. */
	    minb = mintst;
	    maxb = maxtst;
	    minbk = mintstk;
	    maxbk = maxtstk;
	    misllb = 0;
/*           WHEN THE POINT IS NON MISSING, MISLLB SET = 0. */
L1729:
	    --kounta;
/*           THERE IS ONE LESS POINT NOW IN A. */
/* L173: */
	}

    }

/*        AT THIS POINT, KOUNTA CONTAINS THE NUMBER OF POINTS TO CLOSE */
/*        OUT GROUP A WITH.  GROUP B NOW STARTS WITH KSTART+KOUNTA AND */
/*        ENDS WITH NENDB.  MINB AND MAXB HAVE BEEN ADJUSTED AS */
/*        NECESSARY TO REFLECT GROUP B (EVEN THOUGH THE NUMBER OF BITS */
/*        NEEDED TO PACK GROUP B HAVE NOT INCREASED, THE END POINTS */
/*        OF THE RANGE MAY HAVE). */

L174:
    if (kounta == kounts) {
	goto L200;
    }
/*        ON TRANSFER, GROUP A WAS NOT CHANGED.  CLOSE IT OUT. */

/*        ONE OR MORE POINTS WERE TAKEN OUT OF A.  RANGE AND IBITA */
/*        MAY HAVE TO BE RECOMPUTED; IBITA COULD BE LESS THAN */
/*        ORIGINALLY COMPUTED.  IN FACT, GROUP A CAN NOW CONTAIN */
/*        ONLY ONE POINT AND BE PACKED WITH ZERO BITS */
/*        (UNLESS MISSS NE 0). */

    nouta = kounts - kounta;
    ktotal -= nouta;
    kountb += nouta;
    if (nenda - nouta > minak && nenda - nouta > maxak) {
	goto L200;
    }
/*        WHEN THE ABOVE TEST IS MET, THE MIN AND MAX OF THE */
/*        CURRENT GROUP A WERE WITHIN THE OLD GROUP A, SO THE */
/*        RANGE AND IBITA DO NOT NEED TO BE RECOMPUTED. */
/*        NOTE THAT MINAK AND MAXAK ARE NO LONGER NEEDED. */
    ibita = 0;
    mina = mallow;
    maxa = -mallow;

/*        USE SEPARATE LOOPS FOR MISSING AND NO MISSING VALUES */
/*        FOR EFFICIENCY. */

    if (*is523 == 0) {

	i__1 = nenda - nouta;
	for (k = kstart; k <= i__1; ++k) {
	    if (ic[k] < mina) {
		mina = ic[k];
	    }
	    if (ic[k] > maxa) {
		maxa = ic[k];
	    }
/* L1742: */
	}

    } else if (*is523 == 1) {

	i__1 = nenda - nouta;
	for (k = kstart; k <= i__1; ++k) {
	    if (ic[k] == *missp) {
		goto L1744;
	    }
	    if (ic[k] < mina) {
		mina = ic[k];
	    }
	    if (ic[k] > maxa) {
		maxa = ic[k];
	    }
L1744:
	    ;
	}

    } else {

	i__1 = nenda - nouta;
	for (k = kstart; k <= i__1; ++k) {
	    if (ic[k] == *missp || ic[k] == *misss) {
		goto L175;
	    }
	    if (ic[k] < mina) {
		mina = ic[k];
	    }
	    if (ic[k] > maxa) {
		maxa = ic[k];
	    }
L175:
	    ;
	}

    }

    mislla = 0;
    if (mina != mallow) {
	goto L1750;
    }
/*        ALL MISSING VALUES MUST BE ACCOMMODATED. */
    mina = 0;
    maxa = 0;
    mislla = 1;
    if (*is523 != 2) {
	goto L177;
    }
/*        WHEN ALL VALUES ARE MISSING AND THERE ARE NO SECONDARY */
/*        MISSING VALUES IBITA = 0 AS ORIGINALLY SET.  OTHERWISE, */
/*        IBITA MUST BE CALCULATED. */

L1750:
    itest = maxa - mina + lmiss;

    for (ibita = 0; ibita <= 30; ++ibita) {
	if (itest < ibxx2[ibita]) {
	    goto L177;
	}
/* ***        THIS TEST IS THE SAME AS: */
/* ***         IF(MAXA-MINA.LT.IBXX2(IBITA)-LMISS)GO TO 177 */
/* L176: */
    }

/*     WRITE(KFILDO,1760)MAXA,MINA */
/* 1760 FORMAT(' ****ERROR IN PACK_GP.  VALUE WILL NOT PACK IN 30 BITS.', */
/*    1       '  MAXA ='I13,'  MINA ='I13,'.  ERROR AT 1760.') */
    *ier = 706;
    goto L900;

L177:
    goto L200;

/*        ************************************* */

/*        AT THIS POINT, GROUP B REQUIRES AS MANY BITS TO PACK AS GROUPA. */
/*        THEREFORE, TRY TO ADD INC POINTS TO GROUP A WITHOUT INCREASING */
/*        IBITA.  THIS AUGMENTED GROUP IS CALLED GROUP C. */

/*        ************************************* */

L180:
    if (mislla == 1) {
	minc = mallow;
	minck = mallow;
	maxc = -mallow;
	maxck = -mallow;
    } else {
	minc = mina;
	maxc = maxa;
	minck = minak;
	maxck = minak;
    }

    nount = 0;
    if (*nxy - (ktotal + kinc) <= lminpk / 2) {
	kinc = *nxy - ktotal;
    }
/*        ABOVE STATEMENT CONSTRAINS THE LAST GROUP TO BE NOT LESS THAN */
/*        LMINPK/2 IN SIZE.  IF A PROVISION LIKE THIS IS NOT INCLUDED, */
/*        THERE WILL MANY TIMES BE A VERY SMALL GROUP AT THE END. */

/*        USE SEPARATE LOOPS FOR MISSING AND NO MISSING VALUES */
/*        FOR EFFICIENCY.  SINCE KINC IS USUALLY 1, USING SEPARATE */
/*        LOOPS HERE DOESN'T BUY MUCH.  A MISSING VALUE WILL ALWAYS */
/*        TRANSFER BACK TO GROUP A. */

    if (*is523 == 0) {

/* Computing MIN */
	i__2 = ktotal + kinc;
	/*i__1 = min(i__2,*nxy);*/
	i__1 = (i__2 < *nxy) ? i__2 : *nxy;
	for (k = ktotal + 1; k <= i__1; ++k) {
	    if (ic[k] < minc) {
		minc = ic[k];
		minck = k;
	    }
	    if (ic[k] > maxc) {
		maxc = ic[k];
		maxck = k;
	    }
	    ++nount;
/* L185: */
	}

    } else if (*is523 == 1) {

/* Computing MIN */
	i__2 = ktotal + kinc;
	/*i__1 = min(i__2,*nxy);*/
	i__1 = (i__2 < *nxy) ? i__2 : *nxy;
	for (k = ktotal + 1; k <= i__1; ++k) {
	    if (ic[k] == *missp) {
		goto L186;
	    }
	    if (ic[k] < minc) {
		minc = ic[k];
		minck = k;
	    }
	    if (ic[k] > maxc) {
		maxc = ic[k];
		maxck = k;
	    }
L186:
	    ++nount;
/* L187: */
	}

    } else {

/* Computing MIN */
	i__2 = ktotal + kinc;
	/*i__1 = min(i__2,*nxy);*/
	i__1 = (i__2 < *nxy) ? i__2 : *nxy;
	for (k = ktotal + 1; k <= i__1; ++k) {
	    if (ic[k] == *missp || ic[k] == *misss) {
		goto L189;
	    }
	    if (ic[k] < minc) {
		minc = ic[k];
		minck = k;
	    }
	    if (ic[k] > maxc) {
		maxc = ic[k];
		maxck = k;
	    }
L189:
	    ++nount;
/* L190: */
	}

    }

/* ***D     WRITE(KFILDO,191)KOUNTA,KTOTAL,MINA,MAXA,IBITA,MISLLA, */
/* ***D    1   MINC,MAXC,NOUNT,IC(KTOTAL),IC(KTOTAL+1) */
/* ***D191  FORMAT(' AT 191, KOUNTA ='I8,'  KTOTAL ='I8,'  MINA ='I8, */
/* ***D    1       '  MAXA ='I8,'  IBITA ='I3,'  MISLLA ='I3, */
/* ***D    2       '  MINC ='I8,'  MAXC ='I8, */
/* ***D    3       '  NOUNT ='I5,'  IC(KTOTAL) ='I9,'  IC(KTOTAL+1) =',I9) */

/*        IF THE NUMBER OF BITS NEEDED FOR GROUP C IS GT IBITA, */
/*        THEN THIS GROUP A IS A GROUP TO PACK. */

    if (minc == mallow) {
	minc = mina;
	maxc = maxa;
	minck = minak;
	maxck = maxak;
	misllc = 1;
	goto L195;
/*           WHEN THE NEW VALUE(S) ARE MISSING, THEY CAN ALWAYS */
/*           BE ADDED. */

    } else {
	misllc = 0;
    }

    if (maxc - minc >= ibxx2[ibita] - lmiss) {
	goto L200;
    }

/*        THE BITS NECESSARY FOR GROUP C HAS NOT INCREASED FROM THE */
/*        BITS NECESSARY FOR GROUP A.  ADD THIS POINT(S) TO GROUP A. */
/*        COMPUTE THE NEXT GROUP B, ETC., UNLESS ALL POINTS HAVE BEEN */
/*        USED. */

L195:
    ktotal += nount;
    kounta += nount;
    mina = minc;
    maxa = maxc;
    minak = minck;
    maxak = maxck;
    mislla = misllc;
    adda = TRUE_;
    if (ktotal >= *nxy) {
	goto L200;
    }

    if (minbk > ktotal && maxbk > ktotal) {
	mstart = nendb + 1;
/*           THE MAX AND MIN OF GROUP B WERE NOT FROM THE POINTS */
/*           REMOVED, SO THE WHOLE GROUP DOES NOT HAVE TO BE LOOKED */
/*           AT TO DETERMINE THE NEW MAX AND MIN.  RATHER START */
/*           JUST BEYOND THE OLD NENDB. */
	ibitbs = ibitb;
	nendb = 1;
	goto L150;
    } else {
	goto L140;
    }

/*        ************************************* */

/*        GROUP A IS TO BE PACKED.  STORE VALUES IN JMIN( ), JMAX( ), */
/*        LBIT( ), AND NOV( ). */

/*        ************************************* */

L200:
    ++(*lx);
    if (*lx <= *ndg) {
	goto L205;
    }
    lminpk += lminpk / 2;
/*     WRITE(KFILDO,201)NDG,LMINPK,LX */
/* 201  FORMAT(' ****NDG ='I5,' NOT LARGE ENOUGH.', */
/*    1       '  LMINPK IS INCREASED TO 'I3,' FOR THIS FIELD.'/ */
/*    2       '  LX = 'I10) */
    iersav = 716;
    goto L105;

L205:
    jmin[*lx] = mina;
    jmax[*lx] = maxa;
    lbit[*lx] = ibita;
    nov[*lx] = kounta;
    kstart = ktotal + 1;

    if (mislla == 0) {
	misslx[*lx - 1] = mallow;
    } else {
	misslx[*lx - 1] = ic[ktotal];
/*           IC(KTOTAL) WAS THE LAST VALUE PROCESSED.  IF MISLLA NE 0, */
/*           THIS MUST BE THE MISSING VALUE FOR THIS GROUP. */
    }

/* ***D     WRITE(KFILDO,206)MISLLA,IC(KTOTAL),KTOTAL,LX,JMIN(LX),JMAX(LX), */
/* ***D    1                 LBIT(LX),NOV(LX),MISSLX(LX) */
/* ***D206  FORMAT(' AT 206,  MISLLA ='I2,'  IC(KTOTAL) ='I5,'  KTOTAL ='I8, */
/* ***D    1       '  LX ='I6,'  JMIN(LX) ='I8,'  JMAX(LX) ='I8, */
/* ***D    2       '  LBIT(LX) ='I5,'  NOV(LX) ='I8,'  MISSLX(LX) =',I7) */

    if (ktotal >= *nxy) {
	goto L209;
    }

/*        THE NEW GROUP A WILL BE THE PREVIOUS GROUP B.  SET LIMITS, ETC. */

    ibita = ibitb;
    mina = minb;
    maxa = maxb;
    minak = minbk;
    maxak = maxbk;
    mislla = misllb;
    nenda = nendb;
    kounta = kountb;
    ktotal += kounta;
    adda = FALSE_;
    goto L133;

/*        ************************************* */

/*        CALCULATE IBIT, THE NUMBER OF BITS NEEDED TO HOLD THE GROUP */
/*        MINIMUM VALUES. */

/*        ************************************* */

L209:
    *ibit = 0;

    i__1 = *lx;
    for (l = 1; l <= i__1; ++l) {
L210:
        if( *ibit == 31 )
        {
            *ier = -1;
            goto L900;
        }
	if (jmin[l] < ibxx2[*ibit]) {
	    goto L220;
	}
	++(*ibit);
	goto L210;
L220:
	;
    }

/*        INSERT THE VALUE IN JMIN( ) TO BE USED FOR ALL MISSING */
/*        VALUES WHEN LBIT( ) = 0.  WHEN SECONDARY MISSING */
/*        VALUES CAN BE PRESENT, LBIT(L) WILL NOT = 0. */

    if (*is523 == 1) {

	i__1 = *lx;
	for (l = 1; l <= i__1; ++l) {

	    if (lbit[l] == 0) {

		if (misslx[l - 1] == *missp) {
		    jmin[l] = ibxx2[*ibit] - 1;
		}

	    }

/* L226: */
	}

    }

/*        ************************************* */

/*        CALCULATE JBIT, THE NUMBER OF BITS NEEDED TO HOLD THE BITS */
/*        NEEDED TO PACK THE VALUES IN THE GROUPS.  BUT FIND AND */
/*        REMOVE THE REFERENCE VALUE FIRST. */

/*        ************************************* */

/*     WRITE(KFILDO,228)CFEED,LX */
/* 228  FORMAT(A1,/' *****************************************' */
/*    1          /' THE GROUP WIDTHS LBIT( ) FOR ',I8,' GROUPS' */
/*    2          /' *****************************************') */
/*     WRITE(KFILDO,229) (LBIT(J),J=1,MIN(LX,100)) */
/* 229  FORMAT(/' '20I6) */

    *lbitref = lbit[1];

    i__1 = *lx;
    for (k = 1; k <= i__1; ++k) {
	if (lbit[k] < *lbitref) {
	    *lbitref = lbit[k];
	}
/* L230: */
    }

    if (*lbitref != 0) {

	i__1 = *lx;
	for (k = 1; k <= i__1; ++k) {
	    lbit[k] -= *lbitref;
/* L240: */
	}

    }

/*     WRITE(KFILDO,241)CFEED,LBITREF */
/* 241  FORMAT(A1,/' *****************************************' */
/*    1          /' THE GROUP WIDTHS LBIT( ) AFTER REMOVING REFERENCE ', */
/*    2             I8, */
/*    3          /' *****************************************') */
/*     WRITE(KFILDO,242) (LBIT(J),J=1,MIN(LX,100)) */
/* 242  FORMAT(/' '20I6) */

    *jbit = 0;

    i__1 = *lx;
    for (k = 1; k <= i__1; ++k) {
L310:
	if (lbit[k] < ibxx2[*jbit]) {
	    goto L320;
	}
	++(*jbit);
	goto L310;
L320:
	;
    }

/*        ************************************* */

/*        CALCULATE KBIT, THE NUMBER OF BITS NEEDED TO HOLD THE NUMBER */
/*        OF VALUES IN THE GROUPS.  BUT FIND AND REMOVE THE */
/*        REFERENCE FIRST. */

/*        ************************************* */

/*     WRITE(KFILDO,321)CFEED,LX */
/* 321  FORMAT(A1,/' *****************************************' */
/*    1          /' THE GROUP SIZES NOV( ) FOR ',I8,' GROUPS' */
/*    2          /' *****************************************') */
/*     WRITE(KFILDO,322) (NOV(J),J=1,MIN(LX,100)) */
/* 322  FORMAT(/' '20I6) */

    *novref = nov[1];

    i__1 = *lx;
    for (k = 1; k <= i__1; ++k) {
	if (nov[k] < *novref) {
	    *novref = nov[k];
	}
/* L400: */
    }

    if (*novref > 0) {

	i__1 = *lx;
	for (k = 1; k <= i__1; ++k) {
	    nov[k] -= *novref;
/* L405: */
	}

    }

/*     WRITE(KFILDO,406)CFEED,NOVREF */
/* 406  FORMAT(A1,/' *****************************************' */
/*    1          /' THE GROUP SIZES NOV( ) AFTER REMOVING REFERENCE ',I8, */
/*    2          /' *****************************************') */
/*     WRITE(KFILDO,407) (NOV(J),J=1,MIN(LX,100)) */
/* 407  FORMAT(/' '20I6) */
/*     WRITE(KFILDO,408)CFEED */
/* 408  FORMAT(A1,/' *****************************************' */
/*    1          /' THE GROUP REFERENCES JMIN( )' */
/*    2          /' *****************************************') */
/*     WRITE(KFILDO,409) (JMIN(J),J=1,MIN(LX,100)) */
/* 409  FORMAT(/' '20I6) */

    *kbit = 0;

    i__1 = *lx;
    for (k = 1; k <= i__1; ++k) {
L410:
	if (nov[k] < ibxx2[*kbit]) {
	    goto L420;
	}
	++(*kbit);
	goto L410;
L420:
	;
    }

/*        DETERMINE WHETHER THE GROUP SIZES SHOULD BE REDUCED */
/*        FOR SPACE EFFICIENCY. */

    if (ired == 0) {
	reduce(kfildo, &jmin[1], &jmax[1], &lbit[1], &nov[1], lx, ndg, ibit,
		jbit, kbit, novref, ibxx2, ier);

	if (*ier == 714 || *ier == 715) {
/*              REDUCE HAS ABORTED.  REEXECUTE PACK_GP WITHOUT REDUCE. */
/*              PROVIDE FOR A NON FATAL RETURN FROM REDUCE. */
	    iersav = *ier;
	    ired = 1;
	    *ier = 0;
	    goto L102;
	}

    }

    free(misslx);
    misslx=0;

/*     CALL TIMPR(KFILDO,KFILDO,'END   PACK_GP        ') */
    if (iersav != 0) {
	*ier = iersav;
	return 0;
    }

/* 900  IF(IER.NE.0)RETURN1 */

L900:
    free(misslx);
    return 0;
} /* pack_gp__ */

