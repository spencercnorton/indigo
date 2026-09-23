#include <gccore.h>
#include <math.h>

#include "indigo_background.h"

#define INDIGO_TAU 6.28318530718f
#define RADIAL_SEGMENTS 24
#define GLOBE_SEGMENTS 24
#define PRIMARY_WAVE_SEGMENTS 16
#define REAR_WAVE_SEGMENTS 12
#define CUBE_CAMERA_Z -5.4f
#define BOOT_CUBE_HANDOFF 0.82f
#define CUBE_IDLE_SWAY_RATE 0.31f
#define CUBE_IDLE_SWAY_RADIANS 0.035f
#define HOME_DECORATIVE_STRENGTH 0.76f

typedef struct indigoPoint {
	float x;
	float y;
} indigoPoint_t;

typedef struct cubeRasterTransform {
	Mtx model;
	Mtx semanticFaces[UI_HOME_FACE_COUNT];
	float motifAlpha;
	float scaleX;
	float scaleY;
} cubeRasterTransform_t;

typedef struct cubeSurfaceQuad {
	guVector point[4];
	GXColor color[4];
} cubeSurfaceQuad_t;

typedef struct cubeOutline {
	indigoPoint_t point[24];
	int count;
} cubeOutline_t;

typedef struct cubeCoverageEdge {
	guVector eye[2];
	indigoPoint_t outward[2];
	GXColor color[2];
} cubeCoverageEdge_t;

typedef struct waveOscillator {
	float sine;
	float cosine;
	float stepSine;
	float stepCosine;
} waveOscillator_t;

static void putVertex(indigoPoint_t point, GXColor color)
{
	GX_Position3f32(point.x, point.y, 0.0f);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2f32(0.0f, 0.0f);
}

static void setupRasterPipeline(void)
{
	Mtx44 projection;
	Mtx modelView;

	guMtxIdentity(modelView);
	GX_LoadPosMtxImm(modelView, GX_PNMTX0);
	guOrtho(projection, 0.0f, 480.0f, 0.0f, 640.0f, 0.0f, 1.0f);
	GX_LoadProjectionMtx(projection, GX_ORTHOGRAPHIC);
	GX_SetCoPlanar(GX_DISABLE);
	GX_SetClipMode(GX_CLIP_ENABLE);
	GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
	GX_SetZMode(GX_DISABLE, GX_ALWAYS, GX_FALSE);
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetNumChans(1);
	GX_SetNumTexGens(0);
	GX_SetNumIndStages(0);
	GX_SetNumTevStages(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	GX_SetTevDirect(GX_TEVSTAGE0);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetColorUpdate(GX_ENABLE);
	GX_SetCullMode(GX_CULL_NONE);
}

static void drawIndigoWash(void)
{
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		/* This pass deliberately replaces the legacy grey backdrop rather than
		 * tinting it. The cube needs a clean, high-contrast stage. */
		putVertex((indigoPoint_t) {0.0f, 0.0f}, (GXColor) {6, 6, 22, 255});
		putVertex((indigoPoint_t) {640.0f, 0.0f}, (GXColor) {9, 7, 27, 255});
		putVertex((indigoPoint_t) {640.0f, 480.0f}, (GXColor) {29, 19, 65, 255});
		putVertex((indigoPoint_t) {0.0f, 480.0f}, (GXColor) {19, 14, 48, 255});
	GX_End();
}

static void drawBootVeil(u8 alpha)
{
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		putVertex((indigoPoint_t) {0.0f, 0.0f}, (GXColor) {7, 6, 25, alpha});
		putVertex((indigoPoint_t) {640.0f, 0.0f}, (GXColor) {10, 8, 33, alpha});
		putVertex((indigoPoint_t) {640.0f, 480.0f}, (GXColor) {18, 13, 49, alpha});
		putVertex((indigoPoint_t) {0.0f, 480.0f}, (GXColor) {12, 9, 39, alpha});
	GX_End();
}

static void initWaveOscillator(waveOscillator_t *oscillator, float phase,
		float stepSine, float stepCosine)
{
	oscillator->sine = sinf(phase);
	oscillator->cosine = cosf(phase);
	oscillator->stepSine = stepSine;
	oscillator->stepCosine = stepCosine;
}

static void advanceWaveOscillator(waveOscillator_t *oscillator)
{
	float sine = oscillator->sine;
	float cosine = oscillator->cosine;

	oscillator->sine = sine * oscillator->stepCosine +
		cosine * oscillator->stepSine;
	oscillator->cosine = cosine * oscillator->stepCosine -
		sine * oscillator->stepSine;
}

static void buildWavePath(float *center, float *halfWidth, int segments,
		float baseY, float amplitudeA, float amplitudeB, float baseHalfWidth,
		float widthAmplitude, float amplitudeScale,
		waveOscillator_t centerA, waveOscillator_t centerB,
		waveOscillator_t width)
{
	for(int i = 0; i <= segments; i++) {
		center[i] = baseY + amplitudeScale *
			(amplitudeA * centerA.sine + amplitudeB * centerB.sine);
		halfWidth[i] = baseHalfWidth + widthAmplitude * width.sine;
		advanceWaveOscillator(&centerA);
		advanceWaveOscillator(&centerB);
		advanceWaveOscillator(&width);
	}
}

static float waveEdgeFade(int point, int segments)
{
	float u = (float)point / (float)segments;
	return 4.0f * u * (1.0f - u);
}

static GXColor waveVertexColor(GXColor color, float fade, float strength)
{
	color.a = (u8)((float)color.a * fade * strength);
	return color;
}

static bool railJoin(indigoPoint_t previous, indigoPoint_t point,
		indigoPoint_t next, indigoPoint_t *join);

static bool buildRasterJoins(const indigoPoint_t *points, indigoPoint_t *joins,
		int count, bool closed)
{
	if(count < 2) return false;
	for(int i = 0; i < count; i++) {
		indigoPoint_t previous = points[(i + count - 1) % count];
		indigoPoint_t next = points[(i + 1) % count];
		if(!closed && i == 0) previous = (indigoPoint_t) {
			2.0f * points[0].x - next.x, 2.0f * points[0].y - next.y};
		if(!closed && i == count - 1) next = (indigoPoint_t) {
			2.0f * points[i].x - previous.x, 2.0f * points[i].y - previous.y};
		if(!railJoin(previous, points[i], next, &joins[i])) return false;
	}
	return true;
}

static void drawRasterStroke(const indigoPoint_t *points,
		const indigoPoint_t *joins, const GXColor *colors, int count,
		const float *offsets, int bands)
{
	/* All segments share their exact joint vertices. Transparent outer rows
	 * replace GX line rasterization without gaps or additive joint overlap. */
	for(int band = 0; band < bands; band++) {
		GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, count * 2);
		for(int i = 0; i < count; i++) for(int side = band; side <= band + 1; side++) {
			GXColor color = colors[i];
			if(side == 0 || side == bands) color.a = 0;
			putVertex((indigoPoint_t) {points[i].x + joins[i].x * offsets[side],
				points[i].y + joins[i].y * offsets[side]}, color);
		}
		GX_End();
	}
}

static void drawWaveFeather(const float *center, const float *halfWidth,
		int segments, float startX, float side, GXColor color, float strength)
{
	indigoPoint_t points[PRIMARY_WAVE_SEGMENTS + 1];
	indigoPoint_t joins[PRIMARY_WAVE_SEGMENTS + 1];
	if(segments < 1 || segments > PRIMARY_WAVE_SEGMENTS) return;
	for(int i = 0; i <= segments; i++) points[i] = (indigoPoint_t) {
		startX + 736.0f * (float)i / segments, center[i] + halfWidth[i] * side};
	if(!buildRasterJoins(points, joins, segments + 1, false)) return;
	GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, (segments + 1) * 2);
	for(int i = 0; i <= segments; i++) {
		GXColor edge = waveVertexColor(color, waveEdgeFade(i, segments), strength);
		putVertex(points[i], edge);
		edge.a = 0;
		putVertex((indigoPoint_t) {points[i].x + joins[i].x * side,
			points[i].y + joins[i].y * side}, edge);
	}
	GX_End();
}

static void drawSilkWaves(float seconds, bool animated, float strength)
{
	static const float rearRows[3] = {-1.0f, 0.0f, 1.0f};
	static const GXColor rearColors[3] = {
		{55, 47, 140, 4}, {128, 105, 232, 48}, {42, 31, 105, 4}
	};
	static const float primaryRows[4] = {-1.0f, -0.28f, 0.30f, 1.0f};
	static const GXColor primaryColors[4] = {
		{86, 60, 168, 6}, {196, 178, 255, 82},
		{113, 85, 210, 46}, {45, 31, 103, 6}
	};
	float rearCenter[REAR_WAVE_SEGMENTS + 1];
	float rearWidth[REAR_WAVE_SEGMENTS + 1];
	float primaryCenter[PRIMARY_WAVE_SEGMENTS + 1];
	float primaryWidth[PRIMARY_WAVE_SEGMENTS + 1];
	float motionTime = animated ? seconds : 0.0f;
	float amplitudeScale;
	float primaryOffsetX;
	waveOscillator_t centerA;
	waveOscillator_t centerB;
	waveOscillator_t width;

	if(strength <= 0.0f) {
		return;
	}
	if(strength > 1.0f) {
		strength = 1.0f;
	}
	amplitudeScale = 0.70f + strength * 0.30f;
	primaryOffsetX = (1.0f - strength) * -24.0f;

	/* Precomputed angular steps keep the per-frame trigonometry bounded to
	 * each slowly changing phase rather than each screen sample. */
	initWaveOscillator(&centerA, 2.05f - motionTime * 0.031f,
		0.328866647f, 0.944376370f);
	initWaveOscillator(&centerB, 0.20f + motionTime * 0.018f,
		0.608761429f, 0.793353340f);
	initWaveOscillator(&width, 1.40f + motionTime * 0.016f,
		0.377840787f, 0.925870585f);
	buildWavePath(rearCenter, rearWidth, REAR_WAVE_SEGMENTS,
		220.0f, 30.0f, 7.0f, 31.0f, 4.0f, amplitudeScale,
		centerA, centerB, width);

	initWaveOscillator(&centerA, 0.35f + motionTime * 0.052f,
		0.301537960f, 0.953454172f);
	initWaveOscillator(&centerB, 1.15f - motionTime * 0.029f,
		0.562083378f, 0.827080574f);
	initWaveOscillator(&width, 0.80f - motionTime * 0.021f,
		0.353474844f, 0.935444031f);
	buildWavePath(primaryCenter, primaryWidth, PRIMARY_WAVE_SEGMENTS,
		272.0f, 43.0f, 10.0f, 24.0f, 5.0f, amplitudeScale,
		centerA, centerB, width);

	GX_SetZMode(GX_DISABLE, GX_ALWAYS, GX_FALSE);
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
		GX_LO_CLEAR);
	/* The rear fringe stays behind both ribbons. Its outward-only coverage
	 * does not overlap its own fill or paint a seam over the primary wave. */
	drawWaveFeather(rearCenter, rearWidth, REAR_WAVE_SEGMENTS,
		-48.0f, -1.0f, rearColors[0], strength);
	drawWaveFeather(rearCenter, rearWidth, REAR_WAVE_SEGMENTS,
		-48.0f, 1.0f, rearColors[2], strength);
	GX_Begin(GX_QUADS, GX_VTXFMT0,
		REAR_WAVE_SEGMENTS * 2 * 4 + PRIMARY_WAVE_SEGMENTS * 3 * 4);
	for(int segment = 0; segment < REAR_WAVE_SEGMENTS; segment++) {
		float x0 = -48.0f + 736.0f * (float)segment / REAR_WAVE_SEGMENTS;
		float x1 = -48.0f + 736.0f * (float)(segment + 1) / REAR_WAVE_SEGMENTS;
		float fade0 = waveEdgeFade(segment, REAR_WAVE_SEGMENTS);
		float fade1 = waveEdgeFade(segment + 1, REAR_WAVE_SEGMENTS);
		for(int row = 0; row < 2; row++) {
			putVertex((indigoPoint_t) {x0,
				rearCenter[segment] + rearWidth[segment] * rearRows[row]},
				waveVertexColor(rearColors[row], fade0, strength));
			putVertex((indigoPoint_t) {x1,
				rearCenter[segment + 1] + rearWidth[segment + 1] * rearRows[row]},
				waveVertexColor(rearColors[row], fade1, strength));
			putVertex((indigoPoint_t) {x1,
				rearCenter[segment + 1] + rearWidth[segment + 1] * rearRows[row + 1]},
				waveVertexColor(rearColors[row + 1], fade1, strength));
			putVertex((indigoPoint_t) {x0,
				rearCenter[segment] + rearWidth[segment] * rearRows[row + 1]},
				waveVertexColor(rearColors[row + 1], fade0, strength));
		}
	}
	for(int segment = 0; segment < PRIMARY_WAVE_SEGMENTS; segment++) {
		float x0 = primaryOffsetX - 48.0f +
			736.0f * (float)segment / PRIMARY_WAVE_SEGMENTS;
		float x1 = primaryOffsetX - 48.0f +
			736.0f * (float)(segment + 1) / PRIMARY_WAVE_SEGMENTS;
		float fade0 = waveEdgeFade(segment, PRIMARY_WAVE_SEGMENTS);
		float fade1 = waveEdgeFade(segment + 1, PRIMARY_WAVE_SEGMENTS);
		for(int row = 0; row < 3; row++) {
			putVertex((indigoPoint_t) {x0,
				primaryCenter[segment] + primaryWidth[segment] * primaryRows[row]},
				waveVertexColor(primaryColors[row], fade0, strength));
			putVertex((indigoPoint_t) {x1,
				primaryCenter[segment + 1] + primaryWidth[segment + 1] * primaryRows[row]},
				waveVertexColor(primaryColors[row], fade1, strength));
			putVertex((indigoPoint_t) {x1,
				primaryCenter[segment + 1] + primaryWidth[segment + 1] * primaryRows[row + 1]},
				waveVertexColor(primaryColors[row + 1], fade1, strength));
			putVertex((indigoPoint_t) {x0,
				primaryCenter[segment] + primaryWidth[segment] * primaryRows[row + 1]},
				waveVertexColor(primaryColors[row + 1], fade0, strength));
		}
	}
	GX_End();
	drawWaveFeather(primaryCenter, primaryWidth, PRIMARY_WAVE_SEGMENTS,
		primaryOffsetX - 48.0f, -1.0f, primaryColors[0], strength);
	drawWaveFeather(primaryCenter, primaryWidth, PRIMARY_WAVE_SEGMENTS,
		primaryOffsetX - 48.0f, 1.0f, primaryColors[3], strength);

	/* A single restrained additive shoulder provides the familiar silk crest
	 * without turning the background into a competing luminous object. Its
	 * original 8/6-pixel integrated width is a 1/3-pixel core plus two linear
	 * one-pixel fringes; the same color, alpha, path and endpoint fade remain. */
	static const float crestOffsets[4] = {-7.0f/6.0f, -1.0f/6.0f, 1.0f/6.0f, 7.0f/6.0f};
	indigoPoint_t crest[PRIMARY_WAVE_SEGMENTS + 1], joins[PRIMARY_WAVE_SEGMENTS + 1];
	GXColor crestColors[PRIMARY_WAVE_SEGMENTS + 1];
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
	for(int i = 0; i <= PRIMARY_WAVE_SEGMENTS; i++) {
		crest[i] = (indigoPoint_t) {primaryOffsetX - 48.0f +
			736.0f * (float)i / PRIMARY_WAVE_SEGMENTS,
			primaryCenter[i] - primaryWidth[i] * 0.28f};
		crestColors[i] = waveVertexColor((GXColor) {238, 232, 255, 34},
			waveEdgeFade(i, PRIMARY_WAVE_SEGMENTS), strength);
	}
	if(buildRasterJoins(crest, joins, PRIMARY_WAVE_SEGMENTS + 1, false))
		drawRasterStroke(crest, joins, crestColors, PRIMARY_WAVE_SEGMENTS + 1, crestOffsets, 3);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
		GX_LO_CLEAR);
}

static void drawGlobeGrid(float centerX, float centerY, float drift)
{
	static const float radii[6][2] = {
		{252.0f, 176.0f}, {252.0f, 116.0f}, {252.0f, 58.0f},
		{70.0f, 184.0f}, {140.0f, 184.0f}, {218.0f, 184.0f}
	};
	const float step = INDIGO_TAU / GLOBE_SEGMENTS;
	const float stepCos = cosf(step);
	const float stepSin = sinf(step);
	GXColor color = {117, 101, 209, 7};
	static const float offsets[3] = {-0.5f, 0.0f, 0.5f};
	indigoPoint_t points[GLOBE_SEGMENTS + 1], joins[GLOBE_SEGMENTS + 1];
	GXColor colors[GLOBE_SEGMENTS + 1];

	/* A one-pixel triangular profile retains the original 3/6-pixel
	 * integrated width. Reuse the first join exactly to close every ring. */
	for(int ring = 0; ring < 6; ring++) {
		float unitX = 1.0f;
		float unitY = 0.0f;
		for(int segment = 0; segment < GLOBE_SEGMENTS; segment++) {
			float nextX = unitX * stepCos - unitY * stepSin;
			float nextY = unitX * stepSin + unitY * stepCos;
			points[segment] = (indigoPoint_t) {
				centerX + radii[ring][0] * unitX,
				centerY + radii[ring][1] * unitY + drift
			};
			colors[segment] = color;
			unitX = nextX;
			unitY = nextY;
		}
		if(!buildRasterJoins(points, joins, GLOBE_SEGMENTS, true)) continue;
		points[GLOBE_SEGMENTS] = points[0];
		joins[GLOBE_SEGMENTS] = joins[0];
		colors[GLOBE_SEGMENTS] = colors[0];
		drawRasterStroke(points, joins, colors, GLOBE_SEGMENTS + 1, offsets, 2);
	}
}

static void drawRadialDisc(float centerX, float centerY, float radiusX, float radiusY,
		GXColor centerColor, GXColor edgeColor)
{
	const float step = INDIGO_TAU / RADIAL_SEGMENTS;
	const float stepCos = cosf(step);
	const float stepSin = sinf(step);
	float unitX = 1.0f;
	float unitY = 0.0f;

	GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, RADIAL_SEGMENTS + 2);
		putVertex((indigoPoint_t) {centerX, centerY}, centerColor);
		for(int i = 0; i <= RADIAL_SEGMENTS; i++) {
			putVertex((indigoPoint_t) {
				centerX + radiusX * unitX,
				centerY + radiusY * unitY
			}, edgeColor);
			float nextX = (unitX * stepCos) - (unitY * stepSin);
			unitY = (unitX * stepSin) + (unitY * stepCos);
			unitX = nextX;
		}
	GX_End();
}

static void setupCubePipeline(const uiSceneFrame_t *scene, float seconds, bool animated,
		cubeRasterTransform_t *raster)
{
	static Mtx44 projection;
	static bool projectionReady;
	static const guVector xAxis = {1.0f, 0.0f, 0.0f};
	static const guVector yAxis = {0.0f, 1.0f, 0.0f};
	Mtx rotateX;
	Mtx rotateY;
	Mtx navigation;
	Mtx rotation;
	Mtx model;
	Mtx translation;
	/* Home faces are semantic destinations. Keep the selected face cardinal;
	 * decorative motion may breathe around it, but must never rotate it away. */
	float idleBlend = scene->homeIdleBlend;
	float idleYaw = animated ? sinf(seconds * CUBE_IDLE_SWAY_RATE) *
		CUBE_IDLE_SWAY_RADIANS * idleBlend : 0.0f;
	float yaw = scene->cubeYaw + idleYaw;
	float pitch = 0.09f + scene->cubePitch +
		(animated ? sinf(seconds * 0.17f) * 0.030f : 0.0f);
	float bob = animated ? sinf(seconds * 0.62f) * 0.035f : 0.0f;

	if(!projectionReady) {
		guPerspective(projection, 42.0f, 640.0f / 480.0f, 0.1f, 20.0f);
		projectionReady = true;
	}
	GX_LoadProjectionMtx(projection, GX_PERSPECTIVE);

	guMtxRotAxisRad(rotateX, &xAxis, pitch);
	guMtxRotAxisRad(rotateY, &yAxis, yaw);
	guMtxConcat(rotateY, rotateX, rotation);
	guMtxIdentity(navigation);
	for(int row = 0; row < 3; row++) {
		for(int column = 0; column < 3; column++) {
			navigation[row][column] = scene->homeOrientation[row][column];
		}
	}
	/* Navigation is a true spatial rotation, separate from the authored camera
	 * tilt. Pre-composed quarter turns preserve mixed-axis order. */
	guMtxConcat(rotation, navigation, rotation);
	for(int face = 0; face < UI_HOME_FACE_COUNT; face++) {
		guMtxIdentity(raster->semanticFaces[face]);
		for(int row = 0; row < 3; row++)
			for(int column = 0; column < 3; column++)
				raster->semanticFaces[face][row][column] =
					scene->homeMotifBasis[face][row][column];
	}
	raster->motifAlpha = scene->homeMotifAlpha;
	guMtxScaleApply(rotation, rotation, scene->cubeScale, scene->cubeScale, scene->cubeScale);
	guMtxIdentity(translation);
	guMtxTransApply(translation, translation, scene->cubeX, scene->cubeY + bob, CUBE_CAMERA_Z);
	guMtxConcat(translation, rotation, model);
	GX_LoadPosMtxImm(model, GX_PNMTX0);
	guMtxCopy(model, raster->model);
	raster->scaleX = projection[0][0] * 320.0f;
	raster->scaleY = projection[1][1] * 240.0f;

	GX_SetCoPlanar(GX_DISABLE);
	GX_SetClipMode(GX_CLIP_ENABLE);
	GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
	GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_TRUE);
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetNumChans(1);
	GX_SetNumTexGens(0);
	GX_SetNumIndStages(0);
	GX_SetNumTevStages(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	GX_SetTevDirect(GX_TEVSTAGE0);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetColorUpdate(GX_ENABLE);
	GX_SetCullMode(GX_CULL_BACK);
}

static void putCubeVertex(float x, float y, float z, GXColor color)
{
	GX_Position3f32(x, y, z);
	GX_Color4u8(color.r, color.g, color.b, color.a);
}

static void buildCubeFaces(cubeSurfaceQuad_t quads[6], float outer, float inset,
		const GXColor colors[6])
{
	const float n = -inset, p = inset, back = -outer, front = outer;
	const guVector points[6][4] = {
		{{p,n,back}, {p,p,back}, {n,p,back}, {n,n,back}},
		{{-outer,n,n}, {-outer,p,n}, {-outer,p,p}, {-outer,n,p}},
		{{outer,n,p}, {outer,p,p}, {outer,p,n}, {outer,n,n}},
		{{n,-outer,p}, {p,-outer,p}, {p,-outer,n}, {n,-outer,n}},
		{{n,outer,n}, {p,outer,n}, {p,outer,p}, {n,outer,p}},
		{{n,n,front}, {n,p,front}, {p,p,front}, {p,n,front}}
	};

	/* Back, left, right, bottom, top, front. GX treats clockwise-to-viewer
	 * primitives as front-facing. Keep every original fill vertex unchanged. */
	for(int face = 0; face < 6; face++) {
		for(int vertex = 0; vertex < 4; vertex++) {
			quads[face].point[vertex] = points[face][vertex];
			quads[face].color[vertex] = colors[face];
		}
	}
}

static bool projectRailPoint(const cubeRasterTransform_t *raster,
		float x, float y, float z, guVector *eye, indigoPoint_t *screen)
{
	eye->x = raster->model[0][0] * x + raster->model[0][1] * y +
		raster->model[0][2] * z + raster->model[0][3];
	eye->y = raster->model[1][0] * x + raster->model[1][1] * y +
		raster->model[1][2] * z + raster->model[1][3];
	eye->z = raster->model[2][0] * x + raster->model[2][1] * y +
		raster->model[2][2] * z + raster->model[2][3];
	if(!isfinite(eye->x) || !isfinite(eye->y) || !isfinite(eye->z) ||
		eye->z >= -0.1f) {
		return false;
	}
	*screen = (indigoPoint_t) {raster->scaleX * eye->x / -eye->z,
		raster->scaleY * eye->y / -eye->z};
	return true;
}

static bool railJoin(indigoPoint_t previous, indigoPoint_t point,
		indigoPoint_t next, indigoPoint_t *join)
{
	float ax = point.x - previous.x, ay = point.y - previous.y;
	float bx = next.x - point.x, by = next.y - point.y;
	float a = sqrtf(ax * ax + ay * ay), b = sqrtf(bx * bx + by * by);
	if(a < 0.001f || b < 0.001f) return false;
	ax /= a; ay /= a; bx /= b; by /= b;
	float denominator = 1.0f + ax * bx + ay * by;
	/* Near edge-on corners do not get an unbounded miter spike. */
	if(denominator < 0.125f) return false;
	*join = (indigoPoint_t) {(-ay - by) / denominator,
		(ax + bx) / denominator};
	return true;
}

static void putProjectedRailVertex(const cubeRasterTransform_t *raster,
		guVector eye, indigoPoint_t join, float offset, GXColor color)
{
	/* Preserve each endpoint's depth; only its camera-space X/Y move. The
	 * existing perspective projection and core depth test remain in force. */
	putCubeVertex(eye.x + join.x * offset * -eye.z / raster->scaleX,
		eye.y + join.y * offset * -eye.z / raster->scaleY, eye.z, color);
}

static float outlineCross(indigoPoint_t a, indigoPoint_t b, indigoPoint_t c)
{
	return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static void buildCubeOutline(const cubeRasterTransform_t *raster,
		const cubeSurfaceQuad_t faces[6], cubeOutline_t *outline)
{
	indigoPoint_t sorted[24], hull[48];
	int count = 0, used = 0;
	outline->count = 0;
	/* Six chamfer faces contain every outer mesh vertex. Their projected
	 * convex hull identifies the silhouette, including the sealed corner caps. */
	for(int face = 0; face < 6; face++) for(int vertex = 0; vertex < 4; vertex++) {
		guVector eye, point = faces[face].point[vertex];
		indigoPoint_t screen;
		if(!projectRailPoint(raster, point.x, point.y, point.z, &eye, &screen)) return;
		int at = 0;
		for(; at < count; at++) {
			if(fabsf(sorted[at].x - screen.x) < 0.0001f &&
				fabsf(sorted[at].y - screen.y) < 0.0001f) break;
		}
		if(at < count) continue;
		at = count++;
		while(at > 0 && (sorted[at - 1].x > screen.x ||
			(sorted[at - 1].x == screen.x && sorted[at - 1].y > screen.y))) {
			sorted[at] = sorted[at - 1];
			at--;
		}
		sorted[at] = screen;
	}
	if(count < 3) return;
	for(int i = 0; i < count; i++) {
		while(used >= 2 && outlineCross(hull[used - 2], hull[used - 1], sorted[i]) <= 0.001f) used--;
		hull[used++] = sorted[i];
	}
	int lower = used + 1;
	for(int i = count - 2; i >= 0; i--) {
		while(used >= lower && outlineCross(hull[used - 2], hull[used - 1], sorted[i]) <= 0.001f) used--;
		hull[used++] = sorted[i];
	}
	if(used < 4) return;
	outline->count = used - 1;
	for(int i = 0; i < outline->count; i++) outline->point[i] = hull[i];
}

static indigoPoint_t cubeOutlineNormal(const cubeOutline_t *outline, int corner,
		indigoPoint_t fallback)
{
	indigoPoint_t inward;
	if(railJoin(outline->point[(corner + outline->count - 1) % outline->count],
		outline->point[corner], outline->point[(corner + 1) % outline->count], &inward)) {
		return (indigoPoint_t) {-inward.x, -inward.y};
	}
	return fallback;
}

static bool cubeOutlineEdge(const cubeOutline_t *outline,
		indigoPoint_t a, indigoPoint_t b, indigoPoint_t outward[2])
{
	for(int i = 0; i < outline->count; i++) {
		int next = (i + 1) % outline->count;
		indigoPoint_t p = outline->point[i], q = outline->point[next];
		float dx = q.x - p.x, dy = q.y - p.y;
		float length = sqrtf(dx * dx + dy * dy);
		if(length < 0.001f) continue;
		float alongA = ((a.x - p.x) * dx + (a.y - p.y) * dy) / length;
		float alongB = ((b.x - p.x) * dx + (b.y - p.y) * dy) / length;
		if(fabsf(outlineCross(p, q, a)) / length > 0.005f ||
			fabsf(outlineCross(p, q, b)) / length > 0.005f ||
			alongA < -0.005f || alongA > length + 0.005f ||
			alongB < -0.005f || alongB > length + 0.005f) continue;
		/* Shared internal edges never reach this branch. Use the same miter
		 * for neighbouring boundary faces so their one-pixel fringes meet. */
		indigoPoint_t normal = {dy / length, -dx / length};
		outward[0] = fabsf(alongA) < 0.005f ? cubeOutlineNormal(outline, i, normal) :
			(fabsf(alongA - length) < 0.005f ? cubeOutlineNormal(outline, next, normal) : normal);
		outward[1] = fabsf(alongB) < 0.005f ? cubeOutlineNormal(outline, i, normal) :
			(fabsf(alongB - length) < 0.005f ? cubeOutlineNormal(outline, next, normal) : normal);
		return true;
	}
	return false;
}

static void drawCubeSurfacePassVertices(const cubeRasterTransform_t *raster,
		const cubeOutline_t *outline, const cubeSurfaceQuad_t *quads, int count, int vertexCount,
		u8 cullMode, bool opaque)
{
	cubeCoverageEdge_t edges[48]; /* at most twelve bevel quads in a pass */
	int edgeCount = 0;
	Mtx identity;
	if(count < 1 || count > 12 || (vertexCount != 3 && vertexCount != 4)) return;
	/* Preserve the old fill vertices, gradients, draw order and opaque depth
	 * writes exactly. Coverage extends outward only, never opens mesh seams. */
	GX_LoadPosMtxImm(raster->model, GX_PNMTX0);
	GX_SetCullMode(cullMode);
	GX_Begin(vertexCount == 3 ? GX_TRIANGLES : GX_QUADS, GX_VTXFMT0,
		count * vertexCount);
	for(int quad = 0; quad < count; quad++) for(int vertex = 0; vertex < vertexCount; vertex++) {
		guVector p = quads[quad].point[vertex];
		putCubeVertex(p.x, p.y, p.z, quads[quad].color[vertex]);
	}
	GX_End();
	if(outline->count < 3) return;
	for(int quad = 0; quad < count; quad++) {
		guVector eyes[4];
		indigoPoint_t points[4];
		bool valid = true;
		float area = 0.0f;
		for(int vertex = 0; vertex < vertexCount; vertex++) {
			guVector p = quads[quad].point[vertex];
			if(!projectRailPoint(raster, p.x, p.y, p.z, &eyes[vertex], &points[vertex])) valid = false;
		}
		if(!valid) continue;
		for(int vertex = 0; vertex < vertexCount; vertex++) {
			int next = (vertex + 1) % vertexCount;
			area += points[vertex].x * points[next].y - points[next].x * points[vertex].y;
		}
		if(fabsf(area) < 0.001f || (cullMode == GX_CULL_BACK && area >= 0.0f) ||
			(cullMode == GX_CULL_FRONT && area <= 0.0f)) continue;
		for(int vertex = 0; vertex < vertexCount; vertex++) {
			int next = (vertex + 1) % vertexCount;
			cubeCoverageEdge_t *edge = &edges[edgeCount];
			if(!cubeOutlineEdge(outline, points[vertex], points[next], edge->outward)) continue;
			edge->eye[0] = eyes[vertex]; edge->eye[1] = eyes[next];
			edge->color[0] = quads[quad].color[vertex];
			edge->color[1] = quads[quad].color[next];
			edgeCount++;
		}
	}
	if(edgeCount == 0) return;
	guMtxIdentity(identity);
	GX_LoadPosMtxImm(identity, GX_PNMTX0);
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_FALSE);
	GX_Begin(GX_QUADS, GX_VTXFMT0, edgeCount * 4);
	for(int i = 0; i < edgeCount; i++) {
		cubeCoverageEdge_t *edge = &edges[i];
		GXColor transparentA = edge->color[0], transparentB = edge->color[1];
		transparentA.a = transparentB.a = 0;
		putProjectedRailVertex(raster, edge->eye[0], edge->outward[0], 0.0f, edge->color[0]);
		putProjectedRailVertex(raster, edge->eye[1], edge->outward[1], 0.0f, edge->color[1]);
		putProjectedRailVertex(raster, edge->eye[1], edge->outward[1], 1.0f, transparentB);
		putProjectedRailVertex(raster, edge->eye[0], edge->outward[0], 1.0f, transparentA);
	}
	GX_End();
	GX_LoadPosMtxImm(raster->model, GX_PNMTX0);
	GX_SetCullMode(cullMode);
	if(opaque) {
		GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
		GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_TRUE);
	}
}

static void drawCubeSurfacePass(const cubeRasterTransform_t *raster,
		const cubeOutline_t *outline, const cubeSurfaceQuad_t *quads, int count,
		u8 cullMode, bool opaque)
{
	drawCubeSurfacePassVertices(raster, outline, quads, count, 4, cullMode, opaque);
}

/* The scene owns cardinal body-space symbol bases. During an axis change it
 * swaps maps only at zero opacity, so symbols never jump across visible faces. */
static guVector semanticFacePoint(const cubeRasterTransform_t *raster,
		int face, float u, float v, float plane)
{
	const float (*basis)[4] = raster->semanticFaces[face];
	return (guVector) {
		basis[0][0] * u + basis[0][1] * v + basis[0][2] * plane,
		basis[1][0] * u + basis[1][1] * v + basis[1][2] * plane,
		basis[2][0] * u + basis[2][1] * v + basis[2][2] * plane
	};
}

static void putSemanticFaceVertex(const cubeRasterTransform_t *raster,
		int face, float u, float v, float plane, GXColor color)
{
	guVector point = semanticFacePoint(raster, face, u, v, plane);
	color.a = (u8)((float)color.a * raster->motifAlpha);
	putCubeVertex(point.x, point.y, point.z, color);
}

static void putSemanticMotifQuad(const cubeRasterTransform_t *raster, int face,
		const indigoPoint_t corners[4], float plane, GXColor color)
{
	color.a = (u8)((float)color.a * raster->motifAlpha);
	guVector eyes[4];
	indigoPoint_t points[4], joins[4], center = {0.0f, 0.0f};
	float area = 0.0f, clearance = 1000.0f;
	for(int i = 0; i < 4; i++) {
		guVector point = semanticFacePoint(raster, face, corners[i].x, corners[i].y, plane);
		if(!projectRailPoint(raster, point.x, point.y, point.z,
			&eyes[i], &points[i])) goto hidden;
		center.x += points[i].x * 0.25f;
		center.y += points[i].y * 0.25f;
	}
	for(int i = 0; i < 4; i++) {
		int next = (i + 1) % 4;
		area += points[i].x * points[next].y - points[next].x * points[i].y;
	}
	/* Match the existing clockwise-to-viewer face winding. Invisible and
	 * collapsed motifs still emit the fixed count as transparent degenerates. */
	if(area >= -0.001f || color.a == 0) goto hidden;
	for(int i = 0; i < 4; i++) {
		int next = (i + 1) % 4;
		float dx = points[next].x - points[i].x;
		float dy = points[next].y - points[i].y;
		float length = sqrtf(dx * dx + dy * dy);
		if(!railJoin(points[(i + 3) % 4], points[i], points[next], &joins[i])) goto hidden;
		float distance = fabsf(dx * (center.y - points[i].y) -
			dy * (center.x - points[i].x)) / length;
		if(distance < clearance) clearance = distance;
	}
	/* Inset the solid core by half a pixel; very narrow oblique strokes
	 * retain area through alpha instead of inverting their inner polygon. */
	float inset = fminf(0.5f, clearance * 0.5f);
	float outside = 1.0f - inset;
	color.a = (u8)((float)color.a * fminf(1.0f, clearance * 2.0f));
	GXColor transparent = color;
	transparent.a = 0;
	for(int i = 0; i < 4; i++) {
		putProjectedRailVertex(raster, eyes[i], joins[i], -inset, color);
	}
	for(int i = 0; i < 4; i++) {
		int next = (i + 1) % 4;
		putProjectedRailVertex(raster, eyes[i], joins[i], -inset, color);
		putProjectedRailVertex(raster, eyes[i], joins[i], outside, transparent);
		putProjectedRailVertex(raster, eyes[next], joins[next], outside, transparent);
		putProjectedRailVertex(raster, eyes[next], joins[next], -inset, color);
	}
	return;

hidden:
	for(int i = 0; i < 20; i++) {
		putCubeVertex(0.0f, 0.0f, CUBE_CAMERA_Z, (GXColor) {0, 0, 0, 0});
	}
}

static void putSemanticMotifRect(const cubeRasterTransform_t *raster, int face,
		float u0, float v0, float u1, float v1, float plane, GXColor color)
{
	const indigoPoint_t corners[4] = {{u0, v0}, {u0, v1}, {u1, v1}, {u1, v0}};
	putSemanticMotifQuad(raster, face, corners, plane, color);
}

static void putSemanticFaceRect(const cubeRasterTransform_t *raster,
		int face, float u0, float v0, float u1,
		float v1, float plane, GXColor color)
{
	putSemanticFaceVertex(raster, face, u0, v0, plane, color);
	putSemanticFaceVertex(raster, face, u0, v1, plane, color);
	putSemanticFaceVertex(raster, face, u1, v1, plane, color);
	putSemanticFaceVertex(raster, face, u1, v0, plane, color);
}

static void putSemanticFaceDiamond(const cubeRasterTransform_t *raster,
		int face, float u, float v, float radius, float plane, GXColor color)
{
	const indigoPoint_t corners[4] = {
		{u, v - radius}, {u - radius, v}, {u, v + radius}, {u + radius, v}
	};
	putSemanticMotifQuad(raster, face, corners, plane, color);
}

static void putSemanticFaceHand(const cubeRasterTransform_t *raster, int face, float x, float y, float length,
		float halfWidth, float tail, float plane, GXColor color)
{
	float perpendicularX = -y * halfWidth;
	float perpendicularY = x * halfWidth;
	float startX = -x * tail;
	float startY = -y * tail;
	float endX = x * length;
	float endY = y * length;

	const indigoPoint_t corners[4] = {
		{startX + perpendicularX, startY + perpendicularY},
		{endX + perpendicularX, endY + perpendicularY},
		{endX - perpendicularX, endY - perpendicularY},
		{startX - perpendicularX, startY - perpendicularY}
	};
	putSemanticMotifQuad(raster, face, corners, plane, color);
}

/* One book on the Library shelf: three pieces whose two gaps read as the
 * spine's bands. lean tips it about its bottom-left corner into the row. */
static void putSemanticFaceBook(const cubeRasterTransform_t *raster, int face,
		float u, float v, float width, float height, float lean, float plane,
		GXColor color)
{
	static const float bands[6] = {0.0f, 0.15f, 0.20f, 0.80f, 0.85f, 1.0f};
	float c = cosf(lean), s = sinf(lean);

	for(int i = 0; i < 6; i += 2) {
		float b0 = bands[i] * height, b1 = bands[i + 1] * height;
		const indigoPoint_t corners[4] = {
			{u - b0 * s, v + b0 * c},
			{u - b1 * s, v + b1 * c},
			{u + width * c - b1 * s, v + width * s + b1 * c},
			{u + width * c - b0 * s, v + width * s + b0 * c}
		};
		putSemanticMotifQuad(raster, face, corners, plane, color);
	}
}

static void buildChamferStrip(cubeSurfaceQuad_t *quad, float ax, float ay, float az,
		float bx, float by, float bz, float cx, float cy, float cz,
		float dx, float dy, float dz, GXColor outerColor, GXColor innerColor)
{
	*quad = (cubeSurfaceQuad_t) {
		{{ax,ay,az}, {bx,by,bz}, {cx,cy,cz}, {dx,dy,dz}},
		{outerColor, outerColor, innerColor, innerColor}
	};
	/* GX front faces wind clockwise when seen from outside the cube. Some
	 * legacy two-sided depth strips used the reverse winding. Normalize it
	 * without moving a corner, its color, or the quad's triangulation seam. */
	guVector a = {bx - ax, by - ay, bz - az};
	guVector b = {cx - ax, cy - ay, cz - az};
	guVector normal = {a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
	if(normal.x * ax + normal.y * ay + normal.z * az > 0.0f) {
		guVector point = quad->point[1];
		GXColor color = quad->color[1];
		quad->point[1] = quad->point[3]; quad->color[1] = quad->color[3];
		quad->point[3] = point; quad->color[3] = color;
	}
}

static void buildChamferStrips(cubeSurfaceQuad_t quads[12], float outer, float inset)
{
	const float o = outer;
	const float i = inset;
	GXColor pearl = {235, 228, 255, 132};
	GXColor bright = {196, 180, 255, 112};
	GXColor medium = {132, 108, 218, 84};
	GXColor dark = {65, 48, 143, 72};
	GXColor shadow = {25, 18, 70, 78};

	/* Keep the original twelve bevels and endpoint gradients. Their place
	 * before or after the glass is now determined by camera-facing culling,
	 * so a vertical turn cannot leave an old front strip on top of the pane. */
	buildChamferStrip(&quads[0], -i,i,-o, i,i,-o, i,o,-i, -i,o,-i, dark, shadow);
	buildChamferStrip(&quads[1], -i,-o,-i, i,-o,-i, i,-i,-o, -i,-i,-o, shadow, dark);
	buildChamferStrip(&quads[2], -i,-i,-o, -i,i,-o, -o,i,-i, -o,-i,-i, dark, shadow);
	buildChamferStrip(&quads[3], o,-i,-i, o,i,-i, i,i,-o, i,-i,-o, shadow, dark);

	buildChamferStrip(&quads[4], -o,i,-i, -o,i,i, -i,o,i, -i,o,-i, medium, bright);
	buildChamferStrip(&quads[5], i,o,-i, i,o,i, o,i,i, o,i,-i, medium, bright);
	buildChamferStrip(&quads[6], -i,-o,-i, -i,-o,i, -o,-i,i, -o,-i,-i, shadow, dark);
	buildChamferStrip(&quads[7], o,-i,-i, o,-i,i, i,-o,i, i,-o,-i, medium, dark);
	buildChamferStrip(&quads[8], -i,o,i, i,o,i, i,i,o, -i,i,o, pearl, bright);
	buildChamferStrip(&quads[9], -i,-i,o, i,-i,o, i,-o,i, -i,-o,i, medium, dark);
	buildChamferStrip(&quads[10], -o,-i,i, -o,i,i, -i,i,o, -i,-i,o, medium, dark);
	buildChamferStrip(&quads[11], i,-i,o, i,i,o, o,i,i, o,-i,i, pearl, bright);
}

static void buildCubeCorners(cubeSurfaceQuad_t corners[8], float outer, float inset)
{
	/* The inset faces and twelve edge bevels leave eight triangular holes.
	 * Seal each with the same existing endpoints; muted facets preserve the
	 * chamfer without adding bright highlights as the cube turns in space. */
	const GXColor color = {115, 96, 179, 82};
	for(int corner = 0; corner < 8; corner++) {
		float x = (corner & 1) ? 1.0f : -1.0f;
		float y = (corner & 2) ? 1.0f : -1.0f;
		float z = (corner & 4) ? 1.0f : -1.0f;
		corners[corner] = (cubeSurfaceQuad_t) {
			{{x * outer, y * inset, z * inset},
			 {x * inset, y * outer, z * inset},
			 {x * inset, y * inset, z * outer}, {0.0f, 0.0f, 0.0f}},
			{color, color, color, color}
		};
		/* A sign reflection reverses orientation. All exterior faces must
		 * share GX's clockwise winding for the far/near culling passes. */
		if(x * y * z > 0.0f) {
			guVector point = corners[corner].point[1];
			corners[corner].point[1] = corners[corner].point[2];
			corners[corner].point[2] = point;
		}
	}
}

static void drawSemanticFaceMotifs(float seconds, bool animated,
		const uiClockFrame_t *clock, const cubeRasterTransform_t *raster)
{
	const float plane = 1.012f;
	float pulse = animated ? 0.5f + sinf(seconds * 1.10f) * 0.5f : 0.62f;
	float slider = animated ? sinf(seconds * 0.43f) * 0.12f : 0.0f;
	GXColor library = {196, 177, 255, (u8)(142.0f + pulse * 42.0f)};
	GXColor source = {151, 190, 255, (u8)(136.0f + pulse * 52.0f)};
	GXColor settings = {218, 162, 255, (u8)(138.0f + pulse * 46.0f)};
	GXColor system = {239, 230, 255, (u8)(148.0f + pulse * 38.0f)};
	GXColor secondHand = {211, 191, 255, 228};
	GXColor hiddenHand = {0, 0, 0, 0};
	bool clockAvailable = clock != NULL && clock->available;
	int face;
	Mtx identity;

	guMtxIdentity(identity);
	GX_LoadPosMtxImm(identity, GX_PNMTX0);

	GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_FALSE);
	GX_SetCullMode(GX_CULL_BACK);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
	/* Library 13 + Source 9 + Settings 6 + System 11 = 39 shapes, each with a
	 * solid quad and four coverage quads: 780 bounded vertices. Invalid civil
	 * time emits transparent degenerate hands, never an invented time. */
	GX_Begin(GX_QUADS, GX_VTXFMT0, 780);
		/* Library: banded books on a shelf, the last leaning into the row so
		 * it reads as a bookshelf, not a bar chart. The lean clears the third
		 * spine; additive blending would double-light any overlap. */
		putSemanticFaceBook(raster, UI_HOME_FACE_LIBRARY, -0.585f, -0.46f,
			0.20f, 0.90f, 0.0f, plane, library);
		putSemanticFaceBook(raster, UI_HOME_FACE_LIBRARY, -0.35f, -0.46f,
			0.25f, 1.00f, 0.0f, plane, library);
		putSemanticFaceBook(raster, UI_HOME_FACE_LIBRARY, -0.065f, -0.46f,
			0.17f, 0.80f, 0.0f, plane, library);
		putSemanticFaceBook(raster, UI_HOME_FACE_LIBRARY, 0.412f, -0.46f,
			0.19f, 0.84f, 0.349f, plane, library);
		putSemanticMotifRect(raster, UI_HOME_FACE_LIBRARY, -0.64f, -0.555f,
			0.64f, -0.49f, plane, library);

		/* Source: a central port with four linked endpoints. */
		putSemanticFaceDiamond(raster, UI_HOME_FACE_SOURCE, 0.0f, 0.0f, 0.17f,
			plane, source);
		putSemanticFaceDiamond(raster, UI_HOME_FACE_SOURCE, -0.48f, 0.0f, 0.09f,
			plane, source);
		putSemanticFaceDiamond(raster, UI_HOME_FACE_SOURCE, 0.48f, 0.0f, 0.09f,
			plane, source);
		putSemanticFaceDiamond(raster, UI_HOME_FACE_SOURCE, 0.0f, -0.48f, 0.09f,
			plane, source);
		putSemanticFaceDiamond(raster, UI_HOME_FACE_SOURCE, 0.0f, 0.48f, 0.09f,
			plane, source);
		putSemanticMotifRect(raster, UI_HOME_FACE_SOURCE, -0.40f, -0.025f,
			-0.16f, 0.025f, plane, source);
		putSemanticMotifRect(raster, UI_HOME_FACE_SOURCE, 0.16f, -0.025f,
			0.40f, 0.025f, plane, source);
		putSemanticMotifRect(raster, UI_HOME_FACE_SOURCE, -0.025f, -0.40f,
			0.025f, -0.16f, plane, source);
		putSemanticMotifRect(raster, UI_HOME_FACE_SOURCE, -0.025f, 0.16f,
			0.025f, 0.40f, plane, source);

		/* Settings: three calm tracks with independently placed controls. */
		for(face = 0; face < 3; ++face) {
			float y = -0.42f + (float)face * 0.42f;
			float knob = (face == 0 ? -0.25f : (face == 1 ? 0.12f : 0.34f));
			knob += (face == 1 ? slider : -slider * 0.45f);
			putSemanticMotifRect(raster, UI_HOME_FACE_SETTINGS, -0.55f, y - 0.025f,
				0.55f, y + 0.025f, plane, settings);
			putSemanticMotifRect(raster, UI_HOME_FACE_SETTINGS, knob - 0.065f,
				y - 0.13f, knob + 0.065f, y + 0.13f, plane, settings);
		}

		/* System: a framed live clock with three civil-time hands and
		 * cardinal ticks. The hand tails form a compact luminous hub. */
		putSemanticFaceHand(raster, UI_HOME_FACE_SYSTEM,
			clockAvailable ? clock->hourX : 0.0f,
			clockAvailable ? clock->hourY : 0.0f,
			clockAvailable ? 0.34f : 0.0f,
			clockAvailable ? 0.040f : 0.0f,
			clockAvailable ? 0.045f : 0.0f, plane,
			clockAvailable ? system : hiddenHand);
		putSemanticFaceHand(raster, UI_HOME_FACE_SYSTEM,
			clockAvailable ? clock->minuteX : 0.0f,
			clockAvailable ? clock->minuteY : 0.0f,
			clockAvailable ? 0.49f : 0.0f,
			clockAvailable ? 0.027f : 0.0f,
			clockAvailable ? 0.055f : 0.0f, plane,
			clockAvailable ? system : hiddenHand);
		putSemanticFaceHand(raster, UI_HOME_FACE_SYSTEM,
			clockAvailable ? clock->secondX : 0.0f,
			clockAvailable ? clock->secondY : 0.0f,
			clockAvailable ? 0.56f : 0.0f,
			clockAvailable ? 0.013f : 0.0f,
			clockAvailable ? 0.090f : 0.0f, plane,
			clockAvailable ? secondHand : hiddenHand);
		putSemanticMotifRect(raster, UI_HOME_FACE_SYSTEM, -0.55f, 0.55f,
			0.55f, 0.61f, plane, system);
		putSemanticMotifRect(raster, UI_HOME_FACE_SYSTEM, -0.55f, -0.61f,
			0.55f, -0.55f, plane, system);
		putSemanticMotifRect(raster, UI_HOME_FACE_SYSTEM, -0.61f, -0.55f,
			-0.55f, 0.55f, plane, system);
		putSemanticMotifRect(raster, UI_HOME_FACE_SYSTEM, 0.55f, -0.55f,
			0.61f, 0.55f, plane, system);
		putSemanticMotifRect(raster, UI_HOME_FACE_SYSTEM, -0.035f, 0.55f,
			0.035f, 0.66f, plane, system);
		putSemanticMotifRect(raster, UI_HOME_FACE_SYSTEM, -0.035f, -0.66f,
			0.035f, -0.55f, plane, system);
		putSemanticMotifRect(raster, UI_HOME_FACE_SYSTEM, -0.66f, -0.035f,
			-0.55f, 0.035f, plane, system);
		putSemanticMotifRect(raster, UI_HOME_FACE_SYSTEM, 0.55f, -0.035f,
			0.66f, 0.035f, plane, system);
	GX_End();
	GX_LoadPosMtxImm(raster->model, GX_PNMTX0);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
		GX_LO_CLEAR);
}

static void drawFrontRailAccents(const cubeRasterTransform_t *raster,
		float inset, float front, GXColor color)
{
	static const float offsets[4] = {-1.1f, -0.35f, 0.35f, 1.1f};
	guVector eyes[4];
	indigoPoint_t points[4], joins[4];
	Mtx identity;
	for(int i = 0; i < 4; i++) {
		if(!projectRailPoint(raster, i == 0 || i == 3 ? -inset : inset,
			i < 2 ? -inset : inset, front, &eyes[i], &points[i])) return;
	}
	for(int i = 0; i < 4; i++) {
		if(!railJoin(points[(i + 3) % 4], points[i], points[(i + 1) % 4],
			&joins[i])) return;
	}
	guMtxIdentity(identity);
	GX_LoadPosMtxImm(identity, GX_PNMTX0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 48);
	for(int edge = 0; edge < 4; edge++) {
		int next = (edge + 1) % 4;
		for(int band = 0; band < 3; band++) {
			GXColor a = color, b = color;
			if(band == 0) a.a = 0;
			if(band == 2) b.a = 0;
			putProjectedRailVertex(raster, eyes[edge], joins[edge], offsets[band], a);
			putProjectedRailVertex(raster, eyes[next], joins[next], offsets[band], a);
			putProjectedRailVertex(raster, eyes[next], joins[next], offsets[band + 1], b);
			putProjectedRailVertex(raster, eyes[edge], joins[edge], offsets[band + 1], b);
		}
	}
	GX_End();
	GX_LoadPosMtxImm(raster->model, GX_PNMTX0);
}

static void drawCube(const uiSceneFrame_t *scene, float seconds, bool animated,
		const uiClockFrame_t *clock)
{
	static const GXColor coreColors[6] = {
		{19, 15, 54, 255}, {28, 20, 76, 255}, {50, 36, 111, 255},
		{24, 18, 64, 255}, {76, 59, 139, 255}, {44, 31, 102, 255}
	};
	static const GXColor backGlassColors[6] = {
		{61, 46, 132, 14}, {72, 55, 151, 18}, {111, 88, 193, 25},
		{62, 47, 136, 17}, {153, 135, 220, 34}, {92, 71, 174, 20}
	};
	static const GXColor frontGlassColors[6] = {
		{69, 54, 145, 24}, {82, 65, 166, 34}, {146, 124, 225, 58},
		{74, 58, 151, 31}, {219, 207, 255, 78}, {119, 96, 203, 42}
	};
	GXColor innerBoundary = {22, 14, 61, 184};
	GXColor accent = {239, 233, 255, 226};
	cubeRasterTransform_t raster;
	cubeSurfaceQuad_t core[6], shell[6], strips[12], corners[8];
	cubeOutline_t coreOutline, shellOutline;
	const float outer = 1.0f;
	const float inset = 0.78f;
	const float front = 1.008f;

	setupCubePipeline(scene, seconds, animated, &raster);
	buildCubeFaces(core, 0.46f, 0.46f, coreColors);
	buildCubeFaces(shell, outer, inset, backGlassColors);
	buildChamferStrips(strips, outer, inset);
	buildCubeCorners(corners, outer, inset);
	buildCubeOutline(&raster, core, &coreOutline);
	buildCubeOutline(&raster, shell, &shellOutline);

	/* The dark inner volume is the sole Z-writing part of the object. Glass,
	 * filaments, rails and highlights layer around it without masking each
	 * other as their draw order changes. */
	GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
	GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_TRUE);
	GX_SetCullMode(GX_CULL_BACK);
	drawCubeSurfacePass(&raster, &coreOutline, core, 6, GX_CULL_BACK, true);

	/* Far structural surfaces must precede the transparent shell; otherwise
	 * rear rails and caps visibly composite across the front pane. */
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_FALSE);
	drawCubeSurfacePass(&raster, &shellOutline, strips, 12, GX_CULL_FRONT, false);
	drawCubeSurfacePassVertices(&raster, &shellOutline, corners, 8, 3, GX_CULL_FRONT, false);

	/* Every semantic lateral face receives the same restrained inset pane;
	 * no destination becomes a blank reverse side of the Library artwork. */
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_FALSE);
	GX_SetCullMode(GX_CULL_BACK);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 16);
		putSemanticFaceRect(&raster, UI_HOME_FACE_LIBRARY, -0.72f, -0.72f,
			0.72f, 0.72f, 0.86f, (GXColor) {57, 42, 122, 58});
		putSemanticFaceRect(&raster, UI_HOME_FACE_SOURCE, -0.72f, -0.72f,
			0.72f, 0.72f, 0.86f, (GXColor) {43, 50, 119, 58});
		putSemanticFaceRect(&raster, UI_HOME_FACE_SETTINGS, -0.72f, -0.72f,
			0.72f, 0.72f, 0.86f, (GXColor) {79, 39, 112, 58});
		putSemanticFaceRect(&raster, UI_HOME_FACE_SYSTEM, -0.72f, -0.72f,
			0.72f, 0.72f, 0.86f, (GXColor) {68, 56, 126, 58});
	GX_End();

	/* Convex glass is rendered back-to-front with depth writes disabled. */
	GX_SetCullMode(GX_CULL_FRONT);
	drawCubeSurfacePass(&raster, &shellOutline, shell, 6, GX_CULL_FRONT, false);
	GX_SetCullMode(GX_CULL_BACK);
	buildCubeFaces(shell, outer, inset, frontGlassColors);
	drawCubeSurfacePass(&raster, &shellOutline, shell, 6, GX_CULL_BACK, false);

	/* Only near structural surfaces composite in front of the shell. Corner
	 * triangles close the chamfer and share its camera-facing depth policy. */
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	drawCubeSurfacePass(&raster, &shellOutline, strips, 12, GX_CULL_BACK, false);
	drawCubeSurfacePassVertices(&raster, &shellOutline, corners, 8, 3, GX_CULL_BACK, false);
	drawSemanticFaceMotifs(seconds, animated, clock, &raster);

	/* Pixel-width coverage replaces GX's hard subpixel line rasterization.
	 * The front rail remains depth-tested and follows the same cube pose. */
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	/* GX_LINES ignored face culling; the replacement coverage quads must
	 * remain two-sided after the semantic motif pass restores back culling. */
	GX_SetCullMode(GX_CULL_NONE);
	drawFrontRailAccents(&raster, inset - 0.035f, front + 0.001f, innerBoundary);
	drawFrontRailAccents(&raster, inset, front, accent);
	GX_SetCullMode(GX_CULL_BACK);
}

void IndigoBackground_Draw(float seconds, bool backdropAnimated,
	bool cubeAnimated, const uiSceneFrame_t *scene,
	const uiClockFrame_t *clock)
{
	bool backdropMotionActive = backdropAnimated && scene->visible;
	bool cubeMotionActive = cubeAnimated && scene->visible;
	float drift = backdropMotionActive ? sinf(seconds * 0.12f) * 5.0f : 0.0f;
	float centerX = 320.0f + (scene->cubeX * 112.0f);
	float centerY = 238.0f - (scene->cubeY * 112.0f);
	float orbitScale = 0.70f + (scene->cubeScale * 0.30f);
	float orbitStrength = scene->orbitStrength < 0.0f ? 0.0f :
		(scene->orbitStrength > 1.0f ? 1.0f : scene->orbitStrength);
	float decorativeStrength = scene->scene == UI_SCENE_HOME ||
		scene->scene == UI_SCENE_SOURCE ?
		orbitStrength * HOME_DECORATIVE_STRENGTH : orbitStrength;

	setupRasterPipeline();
	drawIndigoWash();
	drawGlobeGrid(320.0f, 212.0f, drift * 0.18f);
	if(!scene->visible) {
		return;
	}
	drawSilkWaves(seconds, backdropMotionActive, decorativeStrength);
	drawRadialDisc(centerX, centerY + 132.0f * orbitScale,
		104.0f * orbitScale, 14.0f * orbitScale,
		(GXColor) {3, 2, 12, (u8)(92.0f * orbitStrength)},
		(GXColor) {3, 2, 12, 0});
	if(scene->introProgress >= BOOT_CUBE_HANDOFF) {
		drawCube(scene, seconds, cubeMotionActive, clock);
	}
}

void IndigoBackground_DrawBootOverlay(float seconds, bool animated,
	const uiSceneFrame_t *scene, const uiClockFrame_t *clock)
{
	float hidden;
	float reveal;
	u8 veilAlpha;

	/* Before menu activation, progress dialogs and any required prompts must
	 * remain visible. The boot veil belongs to the interactive scene only. */
	if(!scene->visible || scene->introProgress >= 1.0f) {
		return;
	}

	reveal = (scene->introProgress - 0.10f) / 0.72f;
	if(reveal < 0.0f) {
		reveal = 0.0f;
	}
	else if(reveal > 1.0f) {
		reveal = 1.0f;
	}
	reveal = reveal * reveal * (3.0f - 2.0f * reveal);
	hidden = 1.0f - reveal;
	veilAlpha = (u8)(hidden * 255.0f);
	if(scene->visible && scene->introProgress < BOOT_CUBE_HANDOFF) {
		drawCube(scene, seconds, animated, clock);
	}
	setupRasterPipeline();
	drawBootVeil(veilAlpha);
}
