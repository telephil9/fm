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
	Tickw = 3,
	Padding = 4,
	Scrollwidth = 12,
};

Mousectl *mctl;
Keyboardctl *kctl;
Image *selbg;
Image *mfg;
Image *tick;
Rectangle ir;
Rectangle sr;
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
	return n.score - m.score;
}

void
match(char *pat)
{
	int i, s;

	nmatches = 0;
	for(i = 0; i < nlines; i++){
		s = fuzzymatch(pat, lines[i]);
		if(s >= 0){
			matches[nmatches].name  = lines[i];
			matches[nmatches].score = s;
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
		matches[nmatches++].name = s;
		if(nlines >= Maxlines)
			break;
	}
	Bterm(bp);
}

void
activate(void)
{
	Match m;

	m = matches[lsel + loff];
	if(pmode){
		print("%s", m.name);
		threadexitsall(nil);
	}
	plumbsendtext(plumbfd, argv0, nil, pwd, m.name);
}

int
lineat(Point p)
{
	if(ptinrect(p, lr) == 0)
		return -1;
	return (p.y - lr.min.y) / lh;	
}

Rectangle
linerect(int i)
{
	Rectangle r;

	r.min.x = 0;
	r.min.y = i * (font->height + Padding);
	r.max.x = Dx(lr) - 2*Padding;
	r.max.y = (i + 1) * (font->height + Padding);
	r = rectaddpt(r, lr.min);
	return r;
}

void
drawline(int i, int sel)
{
	if(loff + i >= nmatches)
		return;
	draw(screen, linerect(i), sel ? selbg : display->white, nil, ZP);
	string(screen, addpt(lr.min, Pt(0, i * lh)), display->black, ZP, font, matches[loff + i].name);
}

void
redraw(void)
{
	char b[10] = {0};
	Point p;
	Rectangle r, scrposr;
	int i, h, y, ye;

	draw(screen, screen->r, display->white, nil, ZP);
	p = string(screen, addpt(screen->r.min, Pt(Padding, Padding)), display->black, ZP, font, "> ");
	p = stringn(screen, p, display->black, ZP, font, input, ninput);
	r = Rect(p.x, p.y, p.x + Dx(tick->r), p.y + Dy(tick->r));
	draw(screen, r, tick, nil, ZP);
	draw(screen, sr, mfg, nil, ZP);
	border(screen, sr, 0, display->black, ZP);
	if(nmatches > 0){
		h = ((double)lcount / nmatches) * Dy(sr);
		y = ((double)loff / nmatches) * Dy(sr);
		ye = sr.min.y + y + h - 1;
		if(ye >= sr.max.y)
			ye = sr.max.y - 1;
		scrposr = Rect(sr.min.x + 1, sr.min.y + y + 1, sr.max.x - 1, ye);
	}else
		scrposr = insetrect(sr, -1);
	draw(screen, scrposr, display->white, nil, ZP);
	for(i = 0; i < lcount; i++)
		drawline(i, i == lsel);
	i = snprint(b, sizeof b, "%d/%d", nmatches, nlines);
	b[i] = 0;
	p = Pt(screen->r.max.x - Padding - stringwidth(font, b), screen->r.min.y + Padding);
	string(screen, p, mfg, ZP, font, b);
	flushimage(display, 1);
}

int
scroll(int lines, int setsel)
{
	if(nmatches <= lcount)
		return 0;
	if(lines < 0 && loff == 0)
		return 0;
	if(lines > 0 && loff + lcount >= nmatches)
		return 0;
	loff += lines;
	if(loff < 0)
		loff = 0;
	if(loff + lcount >= nmatches)
		loff = nmatches - nmatches%lcount;
	if(setsel){
		if(lines > 0)
			lsel = 0;
		else
			lsel = lcount - 1;
	}
	redraw();
	return 1;
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
			matches[nmatches++].name = lines[i];
	}
	if(lsel >= (nmatches - 1))
		lsel = 0;
	if(nmatches == 0)
		lsel = -1;
	loff = 0;
	redraw();
}

void
changesel(int from, int to)
{
	drawline(from, 0);
	drawline(to, 1);
	flushimage(display, 1);
}

void
eresize(void)
{
	ir = Rect(Padding, Padding, screen->r.max.x - Padding, Padding + font->height + Padding);
	ir = rectaddpt(ir, screen->r.min);
	sr = Rect(screen->r.min.x + Padding, ir.max.y, screen->r.min.x + Padding + Scrollwidth, screen->r.max.y - Padding - 1);
	lr = Rect(sr.max.x + 2*Padding, ir.max.y, ir.max.x, screen->r.max.y - Padding);
	lh = font->height + Padding;
	lcount = Dy(lr) / lh;
	redraw();
}

void
emouse(Mouse *m)
{
	int n;

	if(m->buttons == 1){
		if((n = lineat(m->xy)) != -1){
			changesel(lsel, n);
			lsel = n;
		}
	}else if(m->buttons == 4){
		activate();
	}
}

void
ekeyboard(Rune k)
{
	int osel;

	switch(k){
	case Kdel:
		threadexitsall(nil);
		break;
	case Kup:
		if(lsel == 0 && loff > 0)
			scroll(-lcount, 1);
		else if(lsel > 0)
			changesel(lsel, --lsel);
		break;
	case Kdown:
		if(lsel < (nmatches - 1)){
			if(lsel == lcount - 1)
				scroll(lcount, 1);
			else if(loff + lsel < nmatches - 1)
				changesel(lsel, ++lsel);
		}
		break;
	case Kpgup:
		scroll(-lcount, 1);
		break;
	case Kpgdown:
		scroll(lcount, 1);
		break;
	case Khome:
		osel = lsel;
		lsel = 0;
		if(scroll(-nmatches, 0) == 0)
			changesel(osel, lsel);
		break;
	case Kend:
		osel = lsel;
		lsel = nmatches%lcount - 1;
		if(scroll(nmatches, 0) == 0){
			changesel(osel, lsel);
		}
		break;
	case '\n':
		if(lsel >= 0)
			activate();
		break;
	case Kesc:
		if(ninput > 0){
			ninput = 0;
			inputchanged();
		}else
			threadexitsall(nil);
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
		if(isprint(k) && ninput < (sizeof input - 1)){
			input[ninput++] = (char)k; /* XXX */
			inputchanged();
		}
	}
}

Image*
createtick(void)
{
	Image *t;

	t = allocimage(display, Rect(0, 0, Tickw, font->height), screen->chan, 0, DWhite);
	if(t == nil)
		return 0;
	/* background color */
	draw(t, t->r, display->white, nil, ZP);
	/* vertical line */
	draw(t, Rect(Tickw/2, 0, Tickw/2+1, font->height), display->black, nil, ZP);
	/* box on each end */
	draw(t, Rect(0, 0, Tickw, Tickw), display->black, nil, ZP);
	draw(t, Rect(0, font->height-Tickw, Tickw, font->height), display->black, nil, ZP);
	return t;
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
	tick = createtick();
	selbg = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xEFEFEFFF);
	mfg = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x999999FF);
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
			break;
		case Ekeyboard:
			ekeyboard(k);
			break;
		}
	}
}
