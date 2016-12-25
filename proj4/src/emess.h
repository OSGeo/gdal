/* Error message processing header file */
#ifndef EMESS_H
#define EMESS_H

struct EMESS {
	char	*File_name,	/* input file name */
			*Prog_name;	/* name of program */
	int		File_line;	/* approximate line read
							where error occured */
};

#ifdef EMESS_ROUTINE	/* use type */
/* for emess procedure */
struct EMESS emess_dat = { (char *)0, (char *)0, 0 };

#ifdef sun /* Archaic SunOs 4.1.1, etc. */
extern char *sys_errlist[];
#define strerror(n) (sys_errlist[n])
#endif

#else	/* for for calling procedures */

extern struct EMESS emess_dat;
void emess(int, char *, ...);

#endif /* use type */

#endif /* end EMESS_H */
