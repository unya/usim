#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <errno.h>
#include <pwd.h>

#include "Files.h"
#include "glob.h"

#include <dirent.h>
#include <stdlib.h>

#define QUOTE 0200
#define TRIM 0177
#define eq(a,b) (strcmp(a, b)==0)
#define isdir(d) ((d.st_mode & S_IFMT) == S_IFDIR)

char **gargv;			// Pointer to the (stack) arglist.
int gargc;			// Number args in gargv.
int gargmax;
short gflag;
int tglob(char c);
int globerr;
char *home;
extern int errno;
char *strend(char *cp);
void ginit(void);
void collect(char *as);
void acollect(char *as);
void sort(void);
void expand(char *as);
void matchdir(char *pattern);
int execbrc(char *p, char *s);
int match(char *s, char *p);
int amatch(char *s, char *p);
void Gcat(char *s1, char *s2);
void addpath(char);
void rscan(char **t, int (*f) (char));
char *strspl(char *cp, char *dp);

int letter(char c);
int digit(char c);
int any(int c, char *s);

int gethdir(char *home);

int blklen(char **av);
char **blkcpy(char **oav, char **bv);
char **copyblk(char **v);
void blkfree(char **av0);

void fatal(char *fmt, ...);

int globcnt;

char *globchars = "`{[*?";

char *gpath;
char *gpathp;
char *lastgpathp;
int globbed;
char *entp;
char **sortbas;

#define STARTVEC 100
#define INCVEC 100

char **
glob(char *v)
{
	char agpath[BUFSIZ];
	char *vv[2];

	vv[0] = v;
	vv[1] = 0;
	gflag = 0;
	rscan(vv, tglob);
	if (gflag == 0) {
		return copyblk(vv);
	}

	globerr = 0;
	gpath = agpath;
	gpathp = gpath;
	*gpathp = 0;
	lastgpathp = &gpath[sizeof agpath - 2];
	ginit();
	globcnt = 0;
	collect(vv[0]);
	if (globcnt == 0 && (gflag & 1)) {
		blkfree(gargv), gargv = 0;
		lastgpathp = 0;
		gpathp = 0;
		gpath = 0;
		return (0);
	}
	lastgpathp = 0;
	gpathp = 0;
	gpath = 0;
	return gargv;
}

void
gfree(char **glob)
{
	blkfree(glob);
}

void
ginit(void)
{
	char **agargv;

	agargv = (char **) malloc((STARTVEC + 1) * sizeof(char *));
	if (agargv) {
		gargmax = STARTVEC;
		agargv[0] = 0;
		gargv = agargv;
		sortbas = agargv;
		gargc = 0;
	} else
		fatal(NOMEM);
}

void
collect(char *as)
{
	if (eq(as, "{") || eq(as, "{}")) {
		Gcat(as, "");
		sort();
	} else
		acollect(as);
}

void
acollect(char *as)
{
	int ogargc = gargc;

	gpathp = gpath;
	*gpathp = 0;
	globbed = 0;
	expand(as);
	if (gargc != ogargc)
		sort();
}

void
sort(void)
{
	char **p1;
	char **p2;
	char *c;
	char **Gvp = &gargv[gargc];

	p1 = sortbas;
	while (p1 < Gvp - 1) {
		p2 = p1;
		while (++p2 < Gvp)
			if (strcmp(*p1, *p2) > 0)
				c = *p1, *p1 = *p2, *p2 = c;
		p1++;
	}
	sortbas = Gvp;
}

void
expand(char *as)
{
	char *cs;
	char *sgpathp;
	char *oldcs;
	struct stat stb;

	sgpathp = gpathp;
	cs = as;
	if (*cs == '~' && gpathp == gpath) {
		addpath('~');
		for (cs++; letter(*cs) || digit(*cs) || *cs == '-';)
			addpath(*cs++);
		if (!*cs || *cs == '/') {
			if (gpathp != gpath + 1) {
				*gpathp = 0;
				if (gethdir(gpath + 1)) {
					sprintf(errstring = errbuf, "Unknown user name: %s after '~'", gpath + 1);
					globerr = IPS;
				}
				strcpy(gpath, gpath + 1);
			} else
				strcpy(gpath, home);
			gpathp = strend(gpath);
		}
	}
	while (!any(*cs, globchars) && globerr == 0) {
		if (*cs == 0) {
			if (!globbed)
				Gcat(gpath, "");
			else if (stat(gpath, &stb) >= 0) {
				Gcat(gpath, "");
				globcnt++;
			}
			goto endit;
		}
		addpath(*cs++);
	}
	oldcs = cs;
	while (cs > as && *cs != '/')
		cs--, gpathp--;
	if (*cs == '/')
		cs++, gpathp++;
	*gpathp = 0;
	if (*oldcs == '{') {
		execbrc(cs, ((char *) 0));
		return;
	}
	matchdir(cs);
endit:
	gpathp = sgpathp;
	*gpathp = 0;
}

void
matchdir(char *pattern)
{
	struct stat stb;
	int dirf;
	struct direct *dp;
	DIR *dirp;

	dirp = opendir(gpath);
	if (dirp != NULL)
		dirf = dirfd(dirp);
	else {
		switch (errno) {
		case ENOENT:
			globerr = DNF;
			break;
		case EACCES:
			globerr = ATD;
			errstring = READDIR;
			break;
		case ENOTDIR:
			globerr = DNF;
			errstring = PATHNOTDIR;
			break;
		default:
			globerr = MSC;
			errstring = (char *) strerror(errno);
		}
		return;
	}
	if (fstat(dirf, &stb) < 0)
		fatal(FSTAT);
	if (!isdir(stb)) {
		globerr = DNF;
		errstring = PATHNOTDIR;
		return;
	}
	while ((dp = readdir(dirp)) != NULL)
		if (dp->d_ino == 0 || (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' || (dp->d_name[1] == '.' && dp->d_name[2] == '\0'))))
			continue;
		else {
			if (match(dp->d_name, pattern)) {
				Gcat(gpath, dp->d_name);
				globcnt++;
			}
			if (globerr != 0)
				goto out;
		}
out:
	closedir(dirp);
}

int
execbrc(char *p, char *s)
{
	char restbuf[BUFSIZ + 2];
	char *pe;
	char *pm;
	char *pl;
	int brclev = 0;
	char *lm;
	char savec;
	char *sgpathp;

	for (lm = restbuf; *p != '{'; *lm++ = *p++)
		continue;
	for (pe = ++p; *pe; pe++)
		switch (*pe) {
		case '{':
			brclev++;
			continue;
		case '}':
			if (brclev == 0)
				goto pend;
			brclev--;
			continue;
		case '[':
			for (pe++; *pe && *pe != ']'; pe++)
				continue;
			if (!*pe) {
				globerr = IWC;
				errstring = MISSBRACK;
				return (0);
			}
			continue;
		}
pend:
	if (brclev || !*pe) {
		globerr = IWC;
		errstring = "Missing } in wild card syntax";
		return (0);
	}
	for (pl = pm = p; pm <= pe; pm++)
		switch (*pm & (QUOTE | TRIM)) {
		case '{':
			brclev++;
			continue;
		case '}':
			if (brclev) {
				brclev--;
				continue;
			}
			goto doit;
		case ',' | QUOTE:
		case ',':
			if (brclev)
				continue;
		doit:
			savec = *pm;
			*pm = 0;
			strcpy(lm, pl);
			strcat(restbuf, pe + 1);
			*pm = savec;
			if (s == 0) {
				sgpathp = gpathp;
				expand(restbuf);
				gpathp = sgpathp;
				*gpathp = 0;
			} else if (amatch(s, restbuf))
				return (1);
			sort();
			pl = pm + 1;
			if (brclev)
				return (0);
			continue;
		case '[':
			for (pm++; *pm && *pm != ']'; pm++)
				continue;
			if (!*pm) {
				globerr = IWC;
				errstring = MISSBRACK;
				return (0);
			}
			continue;
		}
	if (brclev && globerr == 0)
		goto doit;
	return (0);
}

int
match(char *s, char *p)
{
	int c;
	char *sentp;
	int sglobbed = globbed;

#if 0				// We don't want this "feature".
	if (*s == '.' && *p != '.')
		return (0);
#endif

	sentp = entp;
	entp = s;
	c = amatch(s, p);
	entp = sentp;
	globbed = sglobbed;
	return (c);
}

int
amatch(char *s, char *p)
{
	int scc;
	int ok;
	int lc;
	char *sgpathp;
	struct stat stb;
	int c;
	int cc;

	globbed = 1;
	for (;;) {
		scc = *s++ & TRIM;
		switch (c = *p++) {
		case '{':
			return (execbrc(p - 1, s - 1));
		case '[':
			ok = 0;
			lc = 077777;
			while ((cc = *p++)) {
				if (cc == ']') {
					if (ok)
						break;
					return (0);
				}
				if (cc == '-') {
					if (lc <= scc && scc <= *p++)
						ok++;
				} else if (scc == (lc = cc))
					ok++;
			}
			if (cc == 0) {
				globerr = IWC;
				errstring = MISSBRACK;
				return (0);
			}
			continue;
		case '*':
			if (!*p)
				return (1);
			if (*p == '/') {
				p++;
				goto slash;
			}
			s--;
			do {
				if (amatch(s, p))
					return (1);
			}
			while (*s++);
			return (0);
		case 0:
			return (scc == 0);
		default:
			if (c != scc)
				return (0);
			continue;
		case '?':
			if (scc == 0)
				return (0);
			continue;
		case '/':
			if (scc)
				return (0);
		slash:
			s = entp;
			sgpathp = gpathp;
			while (*s)
				addpath(*s++);
			addpath('/');
			if (stat(gpath, &stb) == 0 && isdir(stb)) {
				if (*p == 0) {
					Gcat(gpath, "");
					globcnt++;
				} else
					expand(p);
			}
			gpathp = sgpathp;
			*gpathp = 0;
			return (0);
		}
	}
}

void
Gcat(char *s1, char *s2)
{
	if (gargc == gargmax) {
		char **newvec;

		if ((newvec = (char **) malloc((size_t) (gargc + INCVEC + 1) * sizeof(char *))) == (char **) 0)
			fatal(NOMEM);
		sortbas = newvec + (sortbas - gargv);
		blkcpy(newvec, gargv);
		free(gargv);
		gargv = newvec;
		gargmax += INCVEC;
	}
	gargc++;
	if (gargv) {
		gargv[gargc] = 0;
		gargv[gargc - 1] = strspl(s1, s2);
	}
}

void
addpath(char c)
{
	if (gpathp >= lastgpathp) {
		globerr = MSC;
		errstring = "Pathname too long";
	} else {
		*gpathp++ = c;
		*gpathp = 0;
	}
}

void
rscan(char **t, int (*f) (char))
{
	char *p;
	char c;

	while ((p = *t++)) {
		if (f == tglob) {
			if (*p == '~')
				gflag |= 2;
			else if (eq(p, "{") || eq(p, "{}"))
				continue;
		}
		while ((c = *p++))
			(*f) (c);
	}
}

int
tglob(char c)
{
	if (any(c, globchars))
		gflag |= c == '{' ? 2 : 1;
	return (c);
}

int
letter(char c)
{
	return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_');
}

int
digit(char c)
{
	return (c >= '0' && c <= '9');
}

int
any(int c, char *s)
{
	while (*s)
		if (*s++ == c)
			return (1);
	return (0);
}

int
blklen(char **av)
{
	int i = 0;

	while (*av++)
		i++;
	return (i);
}

char **
blkcpy(char **oav, char **bv)
{
	char **av = oav;
	if (av) {
		while ((*av++ = *bv++))
			continue;
	}
	return (oav);
}

void
blkfree(char **av0)
{
	char **av = av0;

	while (*av)
		free(*av++);
	free((char *) av0);
}

char *
strspl(char *cp, char *dp)
{
	char *ep = malloc((unsigned) (strlen(cp) + strlen(dp) + 1));

	if (ep) {
		strcpy(ep, cp);
		strcat(ep, dp);
	} else
		fatal(NOMEM);
	return (ep);
}

char **
copyblk(char **v)
{
	if (v == 0)
		return 0;

	char **nv = (char **) malloc((size_t) (((size_t) blklen(v) + 1) * sizeof(char **)));
	if (nv == (char **) 0)
		return 0;

	char **av = v;
	if (av) {
		char **bv = nv;
		while (*av) {
			*bv = malloc(strlen(*av) + 1);
			if (*bv == 0) {
				free(nv);
				return 0;
			}
			strcpy(*bv, *av);
			av++;
			bv++;
		}
		*bv = 0;
	}
	return nv;
}

char *
strend(char *cp)
{
	while (*cp)
		cp++;
	return (cp);
}

// Extract a home directory from the password file. The argument
// points to a buffer where the name of the user whose home directory
// is sought is currently. We write the home directory of the user
// back there.
int
gethdir(char *homepath)
{
	struct passwd *pp = getpwnam(homepath);

	if (pp == 0)
		return (1);
	strcpy(home, pp->pw_dir);
	return (0);
}
