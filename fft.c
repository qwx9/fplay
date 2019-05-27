#include <u.h>
#include <libc.h>
#include "dat.h"
#include "fns.h"

/* from numerical recipes 2nd and 3rd ed */

#define SWAP(a,b)	{tempr=(a); (a)=(b); (b)=tempr;}

void
four1(double *d, int n, int isign)
{
	int nn, mmax, m, j, istep, i;
	double wtemp, wr, wpr, wpi, wi, theta, tempr, tempi;

	if(n < 2 || n & n-1)
		sysfatal("non power of two");
	nn = n << 1;
	j = 1;
	for(i=1; i<nn; i+=2){
		if(j > i){
			SWAP(d[j-1], d[i-1]);
			SWAP(d[j], d[i]);
		}
		m = n;
		while(m >= 2 && j > m){
			j -= m;
			m >>= 1;
		}
		j += m;
	}
	mmax = 2;
	while(nn > mmax){
		istep = mmax << 1;
		theta = isign * (6.28318530717959 / mmax);
		wtemp = sin(0.5 * theta);
		wpr = -2.0 * wtemp * wtemp;
		wpi = sin(theta);
		wr = 1.0;
		wi = 0.0;
		for(m=1; m<mmax; m+=2){
			for(i=m; i<=nn; i+=istep){
				j = i + mmax;
				tempr = wr * d[j-1] - wi * d[j];
				tempi = wr * d[j] + wi * d[j-1];
				d[j-1] = d[i-1] - tempr;
				d[j] = d[i] - tempi;
				d[i-1] += tempr;
				d[i] += tempi;
			}
			wr = (wtemp = wr) * wpr - wi * wpi + wr;
			wi = wi * wpr + wtemp * wpi + wi;
		}
		mmax = istep;
	}
}

void
realft(double *d, int n, int isign)
{
	int i, i1, i2, i3, i4;
	double c1 = 0.5, c2, h1r, h1i, h2r, h2i, wr, wi, wpr, wpi, wtemp;
	double theta = 3.141592653589793238 / (double)(n >> 1);

	if(isign == 1){
		c2 = -0.5;
		four1(d, n>>1, 1);
	}else{
		c2 = 0.5;
		theta = -theta;
	}
	wtemp = sin(0.5 * theta);
	wpr = -2.0 * wtemp * wtemp;
	wpi = sin(theta);
	wr = 1.0 + wpr;
	wi = wpi;
	h1r = 0;
	for(i=1; i<n>>2; i++){
		i2 = 1 + (i1 = i+i);
		i4 = 1 + (i3 = n - i1);
		h1r = c1 * (d[i1] + d[i3]);
		h1i = c1 * (d[i2] - d[i4]);
		h2r = -c2 * (d[i2] + d[i4]);
		h2i = c2 * (d[i1] - d[i3]);
		d[i1] = h1r + wr * h2r - wi * h2i;
		d[i2] = h1i + wr * h2i + wi * h2r;
		d[i3] = h1r - wr * h2r + wi * h2i;
		d[i4] = -h1i + wr * h2i + wi * h2r;
		wr = (wtemp = wr) * wpr - wi * wpi + wr;
		wi = wi * wpr + wtemp * wpi + wi;
	}
	if(isign == 1){
		d[0] = (h1r = d[0]) + d[1];
		d[1] = h1r - d[1];
	}else{
		d[0] = c1 * ((h1r == d[0]) + d[1]);
		d[1] = c1 * (h1r - d[1]);
		four1(d, n>>1, -1);
	}
}
