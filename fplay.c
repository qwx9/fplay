#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include "dat.h"
#include "fns.h"

/* FIXME: labeled axes */
/* FIXME: midi y scale */
/* FIXME: optimization: stereo s16int fft (real integral 16-bit fft) */

enum{
	Rate = 44100,
	Ndelay = Rate / 25,
	Nchunk = Ndelay * 4,

	Csamp = 0,
	Cft,
	Cline,
	Cloop,
	Ncol
};

int cat;
int T, stereo, scale = 1;
Rectangle liner;
Point pan, statp;
double freq[128];
int nfft = 1<<10;
int *fftb;
ulong nbuf;
uchar *buf, *bufp, *bufe, *viewp, *viewe, *viewmax, *loops, *loope;
Image *viewbg, *view, *col[Ncol];
Keyboardctl *kc;
Mousectl *mc;
QLock lck;

u32int cmapa[] = {
	0x000000ff, 0x000003ff, 0x000005ff, 0x000008ff, 0x00000bff, 0x00000dff,
	0x000010ff, 0x000013ff, 0x000015ff, 0x000018ff, 0x00001bff, 0x00001dff,
	0x000020ff, 0x000022ff, 0x000025ff, 0x000027ff, 0x00002aff, 0x00002cff,
	0x00002fff, 0x000031ff, 0x000034ff, 0x000036ff, 0x000039ff, 0x00003bff,
	0x00003dff, 0x000040ff, 0x000042ff, 0x000044ff, 0x000047ff, 0x000049ff,
	0x00004bff, 0x00004dff, 0x00004fff, 0x010051ff, 0x040053ff, 0x070055ff,
	0x090057ff, 0x0c0059ff, 0x0f005bff, 0x11005dff, 0x14005fff, 0x170061ff,
	0x190062ff, 0x1c0064ff, 0x1f0066ff, 0x210067ff, 0x240069ff, 0x27006aff,
	0x29006cff, 0x2c006dff, 0x2e006eff, 0x310070ff, 0x340071ff, 0x360072ff,
	0x390073ff, 0x3c0074ff, 0x3e0076ff, 0x410077ff, 0x430078ff, 0x460078ff,
	0x480079ff, 0x4b007aff, 0x4e007bff, 0x50007bff, 0x53007cff, 0x55007dff,
	0x58007dff, 0x5a007eff, 0x5d007eff, 0x5f007eff, 0x62007fff, 0x64007fff,
	0x66007fff, 0x69007fff, 0x6b007fff, 0x6e0080ff, 0x70007fff, 0x73007fff,
	0x75007fff, 0x77007fff, 0x7a007fff, 0x7c007eff, 0x7e007eff, 0x81007eff,
	0x83007dff, 0x85007dff, 0x88007cff, 0x8a007bff, 0x8c007bff, 0x8e007aff,
	0x900079ff, 0x930078ff, 0x950078ff, 0x970077ff, 0x990076ff, 0x9b0074ff,
	0x9d0073ff, 0x9f0072ff, 0xa20071ff, 0xa40070ff, 0xa6006eff, 0xa8006dff,
	0xaa006cff, 0xac006aff, 0xae0069ff, 0xb00067ff, 0xb10066ff, 0xb30064ff,
	0xb50062ff, 0xb70061ff, 0xb9005fff, 0xbb005dff, 0xbd005bff, 0xbe0059ff,
	0xc00057ff, 0xc20055ff, 0xc40053ff, 0xc50051ff, 0xc7004fff, 0xc9004dff,
	0xca004bff, 0xcc0049ff, 0xce0047ff, 0xcf0044ff, 0xd10042ff, 0xd20040ff,
	0xd4003dff, 0xd5003bff, 0xd70039ff, 0xd80036ff, 0xd90034ff, 0xdb0031ff,
	0xdc002fff, 0xde002cff, 0xdf002aff, 0xe00027ff, 0xe10025ff, 0xe30022ff,
	0xe40020ff, 0xe5001dff, 0xe6001bff, 0xe70018ff, 0xe80015ff, 0xe90013ff,
	0xeb0010ff, 0xec000dff, 0xed000bff, 0xee0008ff, 0xef0003ff, 0xef0005ff,
	0xf00000ff, 0xf10500ff, 0xf20a00ff, 0xf30f00ff, 0xf41500ff, 0xf41a00ff,
	0xf51f00ff, 0xf62400ff, 0xf72900ff, 0xf72e00ff, 0xf83300ff, 0xf93800ff,
	0xf93d00ff, 0xfa4200ff, 0xfa4700ff, 0xfb4c00ff, 0xfb5100ff, 0xfc5600ff,
	0xfc5b00ff, 0xfc6000ff, 0xfd6500ff, 0xfd6900ff, 0xfd6e00ff, 0xfe7300ff,
	0xfe7700ff, 0xfe7c00ff, 0xfe8000ff, 0xff8500ff, 0xff8900ff, 0xff8d00ff,
	0xff9200ff, 0xff9600ff, 0xff9a00ff, 0xff9e00ff, 0xffa200ff, 0xffa600ff,
	0xffaa00ff, 0xffae00ff, 0xffb200ff, 0xffb500ff, 0xffb900ff, 0xffbc00ff,
	0xffc000ff, 0xffc300ff, 0xffc600ff, 0xffca00ff, 0xffcd05ff, 0xffd009ff,
	0xffd30eff, 0xffd613ff, 0xffd817ff, 0xffdb1cff, 0xffde20ff, 0xffe025ff,
	0xffe32aff, 0xffe52eff, 0xffe733ff, 0xffe938ff, 0xffeb3cff, 0xffed41ff,
	0xffef46ff, 0xfff14aff, 0xfff34fff, 0xfff453ff, 0xfff658ff, 0xfff75dff,
	0xfff861ff, 0xfff966ff, 0xfffa6bff, 0xfffb6fff, 0xfffc74ff, 0xfffd79ff,
	0xfffd7dff, 0xfffe82ff, 0xfffe86ff, 0xffff8bff, 0xffff90ff, 0xffff94ff,
	0xffff99ff, 0xffff9eff, 0xffffa2ff, 0xffffa7ff, 0xffffacff, 0xffffb0ff,
	0xffffb5ff, 0xffffb9ff, 0xffffbeff, 0xffffc3ff, 0xffffc7ff, 0xffffccff,
	0xffffd1ff, 0xffffd5ff, 0xffffdaff, 0xffffdfff, 0xffffe3ff, 0xffffe8ff,
	0xffffecff, 0xfffff1ff, 0xfffff6ff, 0xfffffaff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};
Image *cmap[nelem(cmapa)];

Image *
eallocimage(Rectangle r, int repl, ulong col)
{
	Image *i;

	if((i = allocimage(display, r, screen->chan, repl, col)) == nil)
		sysfatal("allocimage: %r");
	return i;
}

void
drawstat(void)
{
	char s[64];

	snprint(s, sizeof s, "T %d p %zd", T, bufp-buf);
	string(screen, statp, col[Cline], ZP, font, s);
}

void
update(void)
{
	int x;

	x = screen->r.min.x + (bufp - viewp) / T;
	if(liner.min.x == x || bufp < viewp && x > liner.min.x)
		return;
	draw(screen, screen->r, view, nil, ZP);
	liner.min.x = x;
	liner.max.x = x + 1;
	if(bufp >= viewp)
		draw(screen, liner, col[Cline], nil, ZP);
	drawstat();
	flushimage(display, 1);
}

void
athread(void *)
{
	int n, fd;

	if((fd = cat ? 1 : open("/dev/audio", OWRITE)) < 0)
		sysfatal("open: %r");
	for(;;){
		qlock(&lck);
		n = bufp + Nchunk >= loope ? loope - bufp : Nchunk;
		if(write(fd, bufp, n) != n)
			break;
		bufp += n;
		if(bufp >= loope)
			bufp = loops;
		update();
		qunlock(&lck);
		yield();
	}
	close(fd);
}

void
dofft(void)
{
	int n, *cp;
	double *fp, *u, f;
	uchar *p;

	free(fftb);
	if((fftb = mallocz(T * nfft/2 * sizeof *fftb, 1)) == nil)
		sysfatal("mallocz: %r");
	if((u = mallocz(nfft * sizeof *u, 1)) == nil)
		sysfatal("mallocz: %r");
	cp = fftb;
	p = buf;
	n = nfft;
	while(p < bufe){
		if(n > (bufe - p) / 4)
			n = (bufe - p) / 4;
		memset(u, 0, nfft * sizeof *u);
		for(fp=u; fp<u+n; fp++, p+=4)
			*fp = (s64int)(s16int)(p[1] << 8 | p[0]);
		realft(u, nfft, 1);
		for(fp=u; fp<u+nfft; fp+=2){
			if(fp == u)
				f = u[0];
			else if(fp == u+nfft-2)
				f = u[1];
			else
				f = sqrt(fp[0] * fp[0] + fp[1] * fp[1]);
			if(f < 0.0)
				f = -f;
			f /= nfft;
			f *= 2;
			if(f != 0.0)
				f = 20 * log10(f / 32768.);
			else
				f = -100;
			if(f < -100.)
				f = -100.;
			else if(f > -20)
				f = -20;
			f += 100;
			f *= 255. / 80;
			*cp++ = f;
		}
	}
	free(u);
}

void
drawfft(void)
{
	int i, *cp;
	Rectangle r, r2;

	r = rectaddpt(viewbg->r, pan);
	if(rectclip(&r, rectsubpt(screen->r, screen->r.min)) == 0)
		return;
	r2 = r;
	if(r2.max.x > T)
		r2.max.x = T;
	if(r2.max.y > nfft>>1)
		r2.max.y = nfft>>1;
	while(r.min.x < r2.max.x){
		r.max.x = r.min.x + scale;
		r.min.y = r2.min.y;
		cp = fftb + nfft/2*r.min.x + r.min.y;
		while(r.min.y < r2.max.y){
			r.max.y = r.min.y + scale;
			assert(cp >= fftb);
			assert(cp < fftb + T * nfft/2);
			i = *cp++;
			assert(i >= 0);
			assert(i < 256);
			draw(viewbg, r, cmap[i], nil, ZP);
			r.min.y = r.max.y;
		}
		r.min.x = r.max.x;
	}
}

void
drawview(void)
{
	int x;
	Rectangle r;

	draw(view, view->r, display->black, nil, ZP);
	draw(view, view->r, viewbg, nil, pan);
	//draw(view, view->r, viewbg, nil, addpt(ZP, Δview));
	if(loops != buf && loops >= viewp){
		x = (loops - viewp) / T;
		r = view->r;
		r.min.x += x;
		r.max.x = r.min.x + 1;
		draw(view, r, col[Cloop], nil, ZP);
	}
	if(loope != bufe && loope >= viewp){
		x = (loope - viewp) / T;
		r = view->r;
		r.min.x += x;
		r.max.x = r.min.x + 1;
		draw(view, r, col[Cloop], nil, ZP);
	}
	draw(screen, screen->r, view, nil, ZP);
	draw(screen, liner, col[Cline], nil, ZP);
	drawstat();
	flushimage(display, 1);
}

enum{
	Rall,
	Rzoom,
	Rpan,
};

void
redrawbg(int type)
{
	int w, x;
	Rectangle r, midr;

	T = 1 + nbuf / scale / nfft;
	w = Dx(screen->r) * T * 4;
	viewmax = bufe - w;
	if(viewp < buf)
		viewp = buf;
	else if(viewp > viewmax)
		viewp = viewmax;
	viewe = viewp + w;
	x = screen->r.min.x + (bufp - viewp) / T;
	liner = screen->r;
	liner.min.x = x;
	liner.max.x = x + 1;
	if(type == Rpan)
		goto end;
	if(type == Rall){
		dofft();
		freeimage(view);
		r = rectsubpt(screen->r, screen->r.min);
		view = eallocimage(r, 0, DBlack);
	}
	freeimage(viewbg);
	viewbg = eallocimage(rectsubpt(screen->r, screen->r.min), 0, DBlack);
	if(type > Rall)
		goto end;
	if(stereo){
		midr = r;
		midr.min.y = midr.max.y / 2;
		midr.max.y = midr.min.y + 1;
		draw(viewbg, midr, col[Csamp], nil, ZP);
		statp = Pt(screen->r.min.x,
			screen->r.min.y + (Dy(screen->r) - font->height) / 2 + 1);
	}else
		statp = Pt(screen->r.min.x, screen->r.max.y - font->height);
end:
	if(type <= Rzoom)
		drawfft();
	drawview();
}

void
writepcm(int pause)
{
	int fd, n, sz;
	char path[256];
	uchar *p;

	memset(path, 0, sizeof path);
	if(!pause)
		qlock(&lck);
	n = enter("path:", path, sizeof(path)-UTFmax, mc, kc, nil);
	if(!pause)
		qunlock(&lck);
	if(n < 0)
		return;
	if((fd = create(path, OWRITE, 0664)) < 0){
		fprint(2, "create: %r\n");
		return;
	}
	if((sz = iounit(fd)) == 0)
		sz = 8192;
	for(p=loops; p<loope; p+=sz){
		n = loope - p < sz ? loope - p : sz;
		if(write(fd, p, n) != n){
			fprint(2, "write: %r\n");
			goto end;
		}
	}
end:
	close(fd);
}

void
setnfft(int n)
{
	if(n < 1<<2 || n > 1<<26)
		return;
	nfft = n;
	redrawbg(Rall);
}

void
setscale(int n)
{
	if(n < 1 || n > 1<<30)
		return;
	scale = n;
	redrawbg(Rzoom);
}

void
setpan(int dx, int dy)
{
	Rectangle r;

	r = rectaddpt(viewbg->r, pan);
	if(dx > 0){
		r.max.x = r.min.x + dx;
		draw(screen, r, display->black, nil, ZP);
	}else if(dx < 0){
		r.min.x = r.max.x + dx;
		draw(screen, r, display->black, nil, ZP);
	}
	r = rectaddpt(viewbg->r, pan);
	if(dy > 0){
		r.max.y = r.min.y + dy;
		draw(screen, r, display->black, nil, ZP);
	}else if(dy < 0){
		r.min.y = r.max.y + dy;
		draw(screen, r, display->black, nil, ZP);
	}
	pan.x += dx;
	pan.y += dy;
	redrawbg(Rpan);
}

void
setloop(void)
{
	int n;
	uchar *p;

	n = (mc->xy.x - screen->r.min.x) * nfft * 4;
	p = viewp + n;
	if(p < buf || p > bufe)
		return;
	if(p < bufp)
		loops = p;
	else
		loope = p;
	drawview();
}

void
setpos(void)
{
	int n;
	uchar *p;

	n = (mc->xy.x - screen->r.min.x) * nfft * 4;
	p = viewp + n;
	if(p < loops || p > loope - Nchunk)
		return;
	bufp = p;
	update();
}

void
bufrealloc(ulong n)
{
	int off;

	off = bufp - buf;
	if((buf = realloc(buf, n)) == nil)
		sysfatal("realloc: %r");
	bufe = buf + n;
	bufp = buf + off;
}

void
usage(void)
{
	fprint(2, "usage: %s [-cs] [pcm]\n", argv0);
	threadexits("usage");
}

void
threadmain(int argc, char **argv)
{
	int n, dx, dy, sz, fd, pause;
	double f;
	Mouse mo;
	Rune r;

	ARGBEGIN{
	case 'c': cat = 1; break;
	case 's': stereo = 1; break;
	default: usage();
	}ARGEND
	if((fd = *argv != nil ? open(*argv, OREAD) : 0) < 0)
		sysfatal("open: %r");
	if(sz = iounit(fd), sz == 0)
		sz = 8192;
	bufrealloc(nbuf += 4*1024*1024);
	while((n = read(fd, bufp, sz)) > 0){
		bufp += n;
		if(bufp + sz >= bufe)
			bufrealloc(nbuf += 4*1024*1024);
	}
	if(n < 0)
		sysfatal("read: %r");
	close(fd);
	bufrealloc(nbuf = bufp - buf);
	nbuf /= 4;
	bufp = buf;
	if(initdraw(nil, nil, "fplay") < 0)
		sysfatal("initdraw: %r");
	if(kc = initkeyboard(nil), kc == nil)
		sysfatal("initkeyboard: %r");
	if(mc = initmouse(nil, screen), mc == nil)
		sysfatal("initmouse: %r");
	mo.xy = ZP;
	col[Csamp] = eallocimage(Rect(0,0,1,1), 1, 0x440000FF);
	col[Cft] = eallocimage(Rect(0,0,1,1), 1, 0x660000FF);
	col[Cline] = eallocimage(Rect(0,0,1,1), 1, 0x884400FF);
	col[Cloop] = eallocimage(Rect(0,0,1,1), 1, 0x777777FF);
	for(n=0; n<nelem(cmapa); n++)
		cmap[n] = eallocimage(Rect(0,0,1,1), 1, cmapa[n]);
	viewp = buf;
	loops = buf;
	loope = bufe;
	redrawbg(Rall);
	f = pow(2, 1./12);
	for(n=0; n<nelem(freq); n++)
		freq[n] = 440 * pow(f, n - 69);
	pause = 0;
	dx = dy = 0;
	Alt a[] = {
		{mc->resizec, nil, CHANRCV},
		{mc->c, &mc->Mouse, CHANRCV},
		{kc->c, &r, CHANRCV},
		{nil, nil, CHANEND}
	};
	if(threadcreate(athread, nil, mainstacksize) < 0)
		sysfatal("threadcreate: %r");
	for(;;){
		switch(alt(a)){
		case 0:
			if(getwindow(display, Refnone) < 0)
				sysfatal("resize failed: %r");
			redrawbg(Rall);
			mo = mc->Mouse;
			break;
		case 1:
			if(eqpt(mo.xy, ZP))
				mo = mc->Mouse;
			switch(mc->buttons){
			case 0: if(dx != 0 || dy != 0) redrawbg(Rzoom); dx = dy = 0; break;
			case 1: setpos(); break;
			case 2: setloop(); break;
			case 4: dx = mo.xy.x - mc->xy.x; dy = mo.xy.y - mc->xy.y; setpan(dx, dy); break;
			case 8: setscale(scale << 1); break;
			case 16: setscale(scale >> 1); break;
			}
			mo = mc->Mouse;
			break;
		case 2:
			switch(r){
			case ' ':
				if(pause ^= 1)
					qlock(&lck);
				else
					qunlock(&lck);
				break;
			case 'b': bufp = loops; update(); break;
			case 'r': loops = buf; loope = bufe; drawview(); break;
			case Kdel:
			case 'q': threadexitsall(nil);
			case 'z':
				if(scale == 1)
					break;
				scale = 1;
				pan = (Point){0, 0};
				redrawbg(Rzoom);
				break;
			case '9': setnfft(nfft >> 1); break;
			case '0': setnfft(nfft << 1); break;
			case '-': setscale(scale >> 1); break;
			case '=':
			case '+': setscale(scale << 1); break;
			case 'w': writepcm(pause); break;
			}
			break;
		}
	}
}
