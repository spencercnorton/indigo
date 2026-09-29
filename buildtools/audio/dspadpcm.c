/* GameCube DSP-ADPCM round trip: encode mono s16 PCM to 4-bit DSP ADPCM and decode it
 * back, so a mix carries the console's own music compression texture.
 * Coefficients: per-frame least-squares 2nd-order predictors clustered to 8 pairs
 * (k-means), then per frame the (pair, scale) with the least squared error.
 * usage: dspadpcm in.raw out.raw   (s16 little-endian mono) */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAME 14

static int16_t clamp16(int v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : v; }

static void lpc2(const int16_t *x, int n, int h1, int h2, double *c1, double *c2) {
	double r11 = 0, r12 = 0, r22 = 0, r01 = 0, r02 = 0;
	int p1 = h1, p2 = h2;
	for (int i = 0; i < n; i++) {
		r11 += (double)p1 * p1; r12 += (double)p1 * p2; r22 += (double)p2 * p2;
		r01 += (double)x[i] * p1; r02 += (double)x[i] * p2;
		p2 = p1; p1 = x[i];
	}
	double det = r11 * r22 - r12 * r12;
	if (fabs(det) < 1e-6) { *c1 = r11 > 0 ? r01 / r11 : 0; *c2 = 0; }
	else { *c1 = (r01 * r22 - r02 * r12) / det; *c2 = (r02 * r11 - r01 * r12) / det; }
	if (*c1 > 3.99) *c1 = 3.99; if (*c1 < -4) *c1 = -4;
	if (*c2 > 3.99) *c2 = 3.99; if (*c2 < -4) *c2 = -4;
}

/* Encode one frame with a given pair and scale; returns squared error, writes decoded samples. */
static double try_frame(const int16_t *x, int n, int c1, int c2, int shift, int *h1, int *h2, int16_t *out) {
	int a = *h1, b = *h2;
	double err = 0;
	for (int i = 0; i < n; i++) {
		int pred = (c1 * a + c2 * b + 1024) >> 11;
		int d = x[i] - pred;
		int step = 1 << shift;
		int q = (int)lrint((double)d / step);
		if (q > 7) q = 7; if (q < -8) q = -8;
		/* mirror the hardware decoder exactly */
		int s = clamp16((q * step * 2048 + 1024 + c1 * a + c2 * b) >> 11);
		err += (double)(x[i] - s) * (x[i] - s);
		out[i] = (int16_t)s;
		b = a; a = s;
	}
	*h1 = a; *h2 = b;
	return err;
}

int main(int argc, char **argv) {
	if (argc != 3) { fprintf(stderr, "usage: %s in.raw out.raw\n", argv[0]); return 2; }
	FILE *f = fopen(argv[1], "rb"); if (!f) return 1;
	fseek(f, 0, SEEK_END); long bytes = ftell(f); fseek(f, 0, SEEK_SET);
	int n = (int)(bytes / 2);
	int16_t *x = malloc(sizeof(int16_t) * (n + FRAME)), *y = malloc(sizeof(int16_t) * (n + FRAME));
	if (fread(x, 2, n, f) != (size_t)n) return 1; fclose(f);
	memset(x + n, 0, FRAME * 2);
	int frames = (n + FRAME - 1) / FRAME;

	/* 1. per-frame predictors from the source (history = source), k-means to 8 pairs */
	double *pc = malloc(sizeof(double) * 2 * frames);
	for (int k = 0; k < frames; k++) {
		int h1 = k ? x[k * FRAME - 1] : 0, h2 = k > 0 && k * FRAME >= 2 ? x[k * FRAME - 2] : 0;
		lpc2(x + k * FRAME, FRAME, h1, h2, &pc[2 * k], &pc[2 * k + 1]);
	}
	double cent[8][2] = {{0, 0}, {1, 0}, {2, -1}, {1.5, -0.6}, {1.9, -0.95}, {0.5, 0}, {1.2, -0.3}, {1.8, -0.82}};
	for (int it = 0; it < 12; it++) {
		double sum[8][2] = {{0}}; int cnt[8] = {0};
		for (int k = 0; k < frames; k++) {
			int best = 0; double bd = 1e30;
			for (int c = 0; c < 8; c++) {
				double dx = pc[2 * k] - cent[c][0], dy = pc[2 * k + 1] - cent[c][1], d = dx * dx + dy * dy;
				if (d < bd) { bd = d; best = c; }
			}
			sum[best][0] += pc[2 * k]; sum[best][1] += pc[2 * k + 1]; cnt[best]++;
		}
		for (int c = 1; c < 8; c++) if (cnt[c]) { cent[c][0] = sum[c][0] / cnt[c]; cent[c][1] = sum[c][1] / cnt[c]; }
	}
	int coef[8][2];
	for (int c = 0; c < 8; c++) { coef[c][0] = (int)lrint(cent[c][0] * 2048); coef[c][1] = (int)lrint(cent[c][1] * 2048); }
	coef[0][0] = 0; coef[0][1] = 0; /* keep a silence-safe pair */

	/* 2. encode + decode frame by frame, best (pair, scale) against the real decoder history */
	int h1 = 0, h2 = 0;
	int16_t tmp[FRAME], best[FRAME];
	for (int k = 0; k < frames; k++) {
		double be = 1e300; int bh1 = 0, bh2 = 0;
		for (int c = 0; c < 8; c++)
			for (int s = 0; s <= 12; s++) {
				int a = h1, b = h2;
				double e = try_frame(x + k * FRAME, FRAME, coef[c][0], coef[c][1], s, &a, &b, tmp);
				if (e < be) { be = e; bh1 = a; bh2 = b; memcpy(best, tmp, sizeof tmp); }
			}
		memcpy(y + k * FRAME, best, sizeof best);
		h1 = bh1; h2 = bh2;
	}
	f = fopen(argv[2], "wb"); fwrite(y, 2, n, f); fclose(f);
	double se = 0, sx = 0;
	for (int i = 0; i < n; i++) { se += (double)(x[i] - y[i]) * (x[i] - y[i]); sx += (double)x[i] * x[i]; }
	fprintf(stderr, "dspadpcm: %d samples, SNR %.1f dB\n", n, 10 * log10(sx / (se + 1)));
	return 0;
}
