
#include <stdio.h>
#include <string.h>
#include <assert.h>

/*
 * Defines the longest recognizable sequence.
 * Must be larger than '\includegraphics'.
 */
#define REQ_SIZE (42)
#define B_SIZE (REQ_SIZE*2)

static const char *target;

static void out_graphics(int len, const char *s)
{
	int i;
	for (i = len - 1; i >= 0; i--) {
		if (s[i] != '_')
			continue;
		if (!(i + 4 == len && !memcmp("_xcf", s + i, 4)))
			return;
		break;
	}
	if (i < 0)
		return;
	assert(s[i] == '_');
	printf("%s: .aux/%.*s.pdf\n", target, len, s);
}

static void out_input(int len, const char *s)
{
	enum { SVG, GP } inp;
	int i;
	int dot;
	for (i = len - 1; i >= 0; i--) {
		if (s[i] != '.')
			continue;
		if (i + 8 == len && !memcmp(".pdf_tex", s + i, 8))
			inp = SVG;
		else if (i + 7 == len && !memcmp(".gp_tex", s + i, 7))
			inp = GP;
		else
			return;
		break;
	}
	if (i < 0)
		return;
	assert(s[i] == '.');
	dot = i;
	if (inp == SVG) {
		for (i--; i > 0; i--) {
			if (s[i] != '-')
				continue;
			if (s[i - 1] != '-')
				break;

			printf("%s: .aux/%.*s\n", target, len, s);
			printf(".aux/%.*s: .aux/%.*s.sum\n\t@$(SVGLAYER_TO_PDF)\n", len, s, dot, s);
			printf(".aux/%.*s.sum: %.*s.svg\n\t@$(SVGLAYER_SUM)\n\n", dot, s, i - 1, s);
			return;
		}
		printf("%s: .aux/%.*s\n", target, len, s);
	} else if (inp == GP) {
		printf("%s: .aux/%.*s\n", target, len, s);
	}
}

struct incr_arg {
	int *i;		// Offset Iterator
	int *l;
	int *end;	// Boolean, is end of file reached?
	char *b;	// Buffer
	int *esc;	// Offset where escape character '\' was spotted.
			// -1 if none has been spotted in reasonable range
};

static inline void incr(struct incr_arg *a)
{
	a->i[0]++;
	if (a->i[0] >= B_SIZE) {
		static int eof = 0;
		if (eof) {
			a->end[0] = 1;
			return;
		}
		int strt = 0;
		if (a->esc[0] != -1 && a->esc[0] <= REQ_SIZE) {/* too long a keyword for anything */
			a->esc[0] = -1;
		} else if (a->esc[0] != -1) {
			strt = B_SIZE - a->esc[0];
			memmove(a->b, a->b + a->esc[0], B_SIZE - a->esc[0]);
			a->esc[0] = 0;
		}
		int r = fread(a->b + strt, 1, B_SIZE - strt, stdin);
		a->i[0] = 0 + strt;
		a->l[0] = r + strt;
		if (r >= B_SIZE - strt)
			return;
		if (feof(stdin)) {
			//fprintf(stderr, "[31meof[39m\n");
			int shortof = (B_SIZE - strt - r);
			memmove(a->b + shortof, a->b, B_SIZE - shortof);
			a->i[0] = shortof + strt;
			if (a->esc[0] != -1)
				a->esc[0] = shortof;
			eof = 1;
			return;
		}

		int z = ferror(stdin);

		if (!z)
			fprintf(stderr, "No eof, no err!\n");

		fprintf(stderr, "texdeps: Hit an error %i!\n", z);
		a->end[0] = 1;
	}
}

int main(int argc, const char *argv[])
{
	char b[B_SIZE]; /*  */
	char cmdname[REQ_SIZE];

	if (argc != 2) {
		fprintf(stderr, "Target argument missing!\n");
		return 1;
	}
	target = argv[1];

	assert(!ferror(stdin));
	int end = 0, i = B_SIZE, l, esc = -1;
	struct incr_arg argz = {
		.i = &i,
		.l = &l,
		.end = &end,
		.b = b,
		.esc = &esc,
	};
	struct incr_arg *a = &argz;
	incr(a);

	while (!end) {
		if (b[i] == '\\') {
			esc = i;
			incr(a);
			if (end)
				break;
		} else if (esc != -1 && !((b[i] >= 'a' && b[i] <= 'z')
				|| (b[i] >= 'A' && b[i] <= 'Z'))) {
			memcpy(cmdname, b + esc, i - esc);
			int cmdtmp = esc;
			esc = -1;

			if (i - cmdtmp == 6 && !memcmp("\\input", cmdname, 6)) {
				if (b[i] != '{')
					continue;
				incr(a);
				esc = i;
				while (b[i] != '%' && b[i] != '}' && !end && esc != -1)
					incr(a);
				if (esc == -1)
					continue;
				cmdtmp = esc;
				esc = -1;
				if (b[i] == '%')
					continue;
				else if (end)
					break;
				out_input(i - cmdtmp, b + cmdtmp);
				//fprintf(stdout, "[33minput: %.*s[39m\n", i - cmdtmp, b + cmdtmp);
			} else if (i - cmdtmp == 16 && !memcmp("\\includegraphics", cmdname, 16)) {
				if (b[i] == '[') {
					while (b[i] != '%' && b[i] != ']' && !end)
						incr(a);
					if (end)
						break;
					else if (b[i] == '%')
						continue; /* skips to comment handler */
					incr(a);
					if (end)
						break;
				}
				if (b[i] != '{')
					continue;
				incr(a);
				esc = i;
				while (b[i] != '%' && b[i] != '}' && !end && esc != -1)
					incr(a);
				if (esc == -1)
					continue;
				cmdtmp = esc;
				esc = -1;
				if (b[i] == '%')
					continue;
				else if (end)
					break;
				out_graphics(i - cmdtmp, b + cmdtmp);
				//fprintf(stderr, "[32mincludegraphics: %.*s[39m\n",
				//		i - cmdtmp, b + cmdtmp);
			} else {
				cmdname[i - cmdtmp] = '\0';
				//fprintf(stdout, "Cmd %02i -- %s\n", i - cmdtmp, cmdname);
			}
		} else if (b[i] == '%') {
			esc = -1;
			while (b[i] != '\n' && !end)
				incr(a);

			if (end)
				break;
		} else if (b[i] == '\0') {
			fprintf(stderr, "Did not expect '\\0' in a text file :/\n");
			return 0;
		}

		incr(a);
	}

	return 0;

}

