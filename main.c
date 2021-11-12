#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <bio.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <plumb.h>
#include "a.h"

enum
{
	Emouse,
	Eresize,
	Ekeyboard,
};

enum
{
	Maxlines = 4096,
	Padding = 4,
};

Mousectl *mctl;
Keyboardctl *kctl;
Image *selbg;
Rectangle ir;
Rectangle lr;
int lh;
int lcount;
int loff;
int lsel;
int pmode;
int plumbfd;
char* lines[Maxlines];
int nlines;
Match matches[Maxlines];
int nmatches;
char input[255] = {0};
int ninput = 0;
char pwd[255] = {0};

int
matchcmp(void *a, void *b)
{
	Match m, n;

	m = *(Match*)a;
	n = *(Match*)b;
	return n.s - m.s;
}

void
match(char *pat)
{
	int i, s;

	nmatches = 0;
	for(i = 0; i < nlines; i++){
		s = fuzzymatch(pat, lines[i]);
		if(s >= 0){
			matches[nmatches].n = lines[i];
			matches[nmatches].s = s;
			nmatches++;
		}
	}
	if(nmatches > 0)
		qsort(matches, nmatches, sizeof(Match), matchcmp);
}

void
loadlines(void)
{
	Biobuf *bp;
	char *s;

	nlines = 0;
	bp = Bfdopen(0, OREAD);
	for(;;){
		s = Brdstr(bp, '\n', 1);
		if(s == nil)
			break;
		lines[nlines++] = s;
		matches[nmatches++].n = s;
		if(nlines >= Maxlines)
			break;
	}
	Bterm(bp);
}

Rectangle
linerect(int i)
{
	Rectangle r;

	r.min.x = 0;
	r.min.y = i * (font->height + Padding);
	r.max.x = lr.max.x;
	r.max.y = (i + 1) * (font->height + Padding);
	r = rectaddpt(r, lr.min);
	return r;
}

void
redraw(void)
{
	Point p;
	int i, n;
	Match m;

	draw(screen, screen->r, display->white, nil, ZP);
	p = string(screen, addpt(screen->r.min, Pt(Padding, Padding)), display->black, ZP, font, "> ");
	stringn(screen, p, display->black, ZP, font, input, ninput);

	for(i = 0; i < lcount; i++){
		n = loff + i;
		if(n >= nmatches)
			break;
		m = matches[n];
		p = addpt(lr.min, Pt(0, i * (font->height + Padding)));
		if(lsel == i)
			draw(screen, linerect(i), selbg, nil, ZP);
		string(screen, p, display->black, ZP, font, matches[i].n);
	}
	flushimage(display, 1);
}

void
eresize(void)
{
	ir = Rect(Padding, Padding, screen->r.max.x - Padding, Padding + font->height + Padding);
	ir = rectaddpt(ir, screen->r.min);
	lr = Rect(screen->r.min.x + Padding, ir.max.y, ir.max.x, screen->r.max.y - Padding);
	lh = font->height + Padding;
	lcount = Dy(lr) / lh;
	redraw();
}

void
emouse(Mouse *m)
{
	USED(m);
}

void
inputchanged(void)
{
	int i;

	input[ninput] = 0;
	if(ninput > 0){
		match(input);
	}else{
		nmatches = 0;
		for(i = 0; i < nlines; i++)
			matches[nmatches++].n = lines[i];
	}
	if(lsel >= (nmatches - 1))
		lsel = 0;
	if(nmatches == 0)
		lsel = -1;
	redraw();
}

void
ekeyboard(Rune k)
{
	Match m;

	switch(k){
	case Kdel:
		threadexitsall(nil);
		break;
	case Kup:
		if(lsel > 0)
			--lsel;
		redraw();
		break;
	case Kdown:
		if(lsel < (nmatches - 1))
			++lsel;
		redraw();
		break;
	case '\n':
		if(lsel >= 0){
			m = matches[lsel + loff];
			if(pmode){
				print("%s", m.n);
				threadexitsall(nil);
			}else
				plumbsendtext(plumbfd, argv0, nil, pwd, m.n);
		}
		break;
	case Kbs:
		if(ninput > 0){
			ninput--;
			inputchanged();
		}
		break;
	case Knack: /* ^U */
		if(ninput > 0){
			ninput = 0;
			inputchanged();
		}
	default:
		if(isalnum(k)){
			input[ninput++] = (char)k; /* XXX */
			inputchanged();
		}
	}
}

void
usage(void)
{
	fprint(2, "usage: %s [-p]\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char **argv)
{
	Mouse m;
	Rune k;
	Alt a[] = {
		{ nil, &m, CHANRCV },
		{ nil, nil, CHANRCV },
		{ nil, &k, CHANRCV },
		{ nil, nil, CHANEND },
	};

	pmode = 0;
	ARGBEGIN{
	case 'p':
		pmode = 1;
		break;
	default:
		usage();
	}ARGEND;
	getwd(pwd, sizeof pwd);
	if(pmode == 0){
		plumbfd = plumbopen("send", OWRITE|OCEXEC);
		if(plumbfd < 0)
			sysfatal("plumbopen: %r");
	}
	loadlines();
	if(initdraw(nil, nil, "fm") < 0)
		sysfatal("initdraw: %r");
	display->locking = 0;
	if((mctl = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kctl = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");
	a[Emouse].c = mctl->c;
	a[Eresize].c = mctl->resizec;
	a[Ekeyboard].c = kctl->c;
	selbg = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xEFEFEFFF);
	loff = 0;
	lsel = 0;
	eresize();
	for(;;){
		switch(alt(a)){
		case Emouse:
			emouse(&m);
			break;
		case Eresize:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			eresize();
		case Ekeyboard:
			ekeyboard(k);
			break;
		}
	}
}
