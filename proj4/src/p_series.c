/* print row coefficients of Tseries structure */
#include "projects.h"
#include <stdio.h>
#include <string.h>
#define NF 20 /* length of final format string */
#define CUT 60 /* check length of line */
	void
p_series(Tseries *T, FILE *file, char *fmt) {
	int i, j, n, L;
	char format[NF+1];

	*format = ' ';
	strncpy(format + 1, fmt, NF - 3);
	strcat(format, "%n");
	fprintf(file, "u: %d\n", T->mu+1);
	for (i = 0; i <= T->mu; ++i)
		if (T->cu[i].m) {
			fprintf(file, "%d %d%n", i, T->cu[i].m, &L);
			n = 0;
			for (j = 0; j < T->cu[i].m; ++j) {
				if ((L += n) > CUT)
					fprintf(file, "\n %n", &L);
				fprintf(file, format, T->cu[i].c[j], &n);
			}
			fputc('\n', file);
		}
	fprintf(file, "v: %d\n", T->mv+1);
	for (i = 0; i <= T->mv; ++i)
		if (T->cv[i].m) {
			fprintf(file, "%d %d%n", i, T->cv[i].m, &L);
			n = 0;
			for (j = 0; j < T->cv[i].m; ++j) {
				if ((L += n) > 60)
					fprintf(file, "\n %n", &L);
				fprintf(file, format, T->cv[i].c[j], &n);
			}
			fputc('\n', file);
		}
}
