#include <gccore.h>
#include <math.h>

#include "indigo_background.h"
#include "ui_color.h"

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
#define FACE_POLYGON_MAX 48
#define FACE_BAND_MAX 80
#define CONTROLLER_IDLE_HOLD 2.0f

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

/* Controller icon state: stick axes in -1..1 (y up) and PAD_* bits held. */
typedef struct controllerPose {
	float stickX;
	float stickY;
	float substickX;
	float substickY;
	u32 pressed;
} controllerPose_t;

/* Idle play resumes only once the controller has been left alone, so it is
 * never mistaken for the user's own input. */
typedef struct controllerIdle {
	float lastLiveInput;
	bool liveSeen;
} controllerIdle_t;

typedef struct waveOscillator {
	float sine;
	float cosine;
	float stepSine;
	float stepCosine;
} waveOscillator_t;

static void putVertex(indigoPoint_t point, GXColor color)
{
	UIColor_Apply(&color.r, &color.g, &color.b);
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
	UIColor_Apply(&color.r, &color.g, &color.b);
	GX_Position3f32(x, y, z);
	GX_Color4u8(color.r, color.g, color.b, color.a);
}

static guVector cubeViewNormal(const cubeRasterTransform_t *raster, guVector n)
{
	guVector eye = {
		raster->model[0][0] * n.x + raster->model[0][1] * n.y + raster->model[0][2] * n.z,
		raster->model[1][0] * n.x + raster->model[1][1] * n.y + raster->model[1][2] * n.z,
		raster->model[2][0] * n.x + raster->model[2][1] * n.y + raster->model[2][2] * n.z
	};
	float length = sqrtf(eye.x * eye.x + eye.y * eye.y + eye.z * eye.z);
	if(length < 0.0001f) return (guVector) {0.0f, 0.0f, 1.0f};
	return (guVector) {eye.x / length, eye.y / length, eye.z / length};
}

/* Outward body axes in buildCubeFaces order: back, left, right, bottom, top, front. */
static const guVector cubeFaceAxes[6] = {
	{0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
	{0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}
};

/* Tints are authored per camera direction in cubeFaceAxes order. Squared
 * unit-normal components sum to one, so a turning face blends between them
 * and the light stays with the camera instead of rotating with the cube. */
static void litCubeTints(const cubeRasterTransform_t *raster,
		const GXColor tints[6], GXColor lit[6])
{
	for(int face = 0; face < 6; face++) {
		guVector n = cubeViewNormal(raster, cubeFaceAxes[face]);
		const GXColor *x = &tints[n.x < 0.0f ? 1 : 2];
		const GXColor *y = &tints[n.y < 0.0f ? 3 : 4];
		const GXColor *z = &tints[n.z < 0.0f ? 0 : 5];
		float wx = n.x * n.x, wy = n.y * n.y, wz = n.z * n.z;
		lit[face] = (GXColor) {
			(u8)(x->r * wx + y->r * wy + z->r * wz + 0.5f),
			(u8)(x->g * wx + y->g * wy + z->g * wz + 0.5f),
			(u8)(x->b * wx + y->b * wy + z->b * wz + 0.5f),
			(u8)(x->a * wx + y->a * wy + z->a * wz + 0.5f)
		};
	}
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

/* One book on a shelf: three pieces whose two gaps read as the spine's
 * bands. lean tips it about its bottom-left corner into the row. */
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

/* A convex polygon on a semantic face, clockwise in face space, in its own
 * primitives: a solid fan inset by half a pixel and a coverage fringe that
 * falls to zero one native pixel out, as the motif quads do. Back-facing or
 * collapsed polygons draw nothing. */
static void drawFacePolygon(const cubeRasterTransform_t *raster, int face,
		const indigoPoint_t *corners, int count, float plane, GXColor color)
{
	guVector eyes[FACE_POLYGON_MAX];
	indigoPoint_t points[FACE_POLYGON_MAX], joins[FACE_POLYGON_MAX];
	indigoPoint_t center = {0.0f, 0.0f};
	float area = 0.0f, clearance = 1000.0f;

	color.a = (u8)((float)color.a * raster->motifAlpha);
	if(count < 3 || count > FACE_POLYGON_MAX || color.a == 0) return;
	for(int i = 0; i < count; i++) {
		guVector point = semanticFacePoint(raster, face, corners[i].x, corners[i].y, plane);
		if(!projectRailPoint(raster, point.x, point.y, point.z,
			&eyes[i], &points[i])) return;
		center.x += points[i].x / (float)count;
		center.y += points[i].y / (float)count;
	}
	for(int i = 0; i < count; i++) {
		int next = (i + 1) % count;
		area += points[i].x * points[next].y - points[next].x * points[i].y;
	}
	if(area >= -0.001f) return;
	for(int i = 0; i < count; i++) {
		int next = (i + 1) % count;
		float dx = points[next].x - points[i].x;
		float dy = points[next].y - points[i].y;
		float length = sqrtf(dx * dx + dy * dy);
		if(length <= 0.0f || !railJoin(points[(i + count - 1) % count],
			points[i], points[next], &joins[i])) return;
		float distance = fabsf(dx * (center.y - points[i].y) -
			dy * (center.x - points[i].x)) / length;
		if(distance < clearance) clearance = distance;
	}
	float inset = fminf(0.5f, clearance * 0.5f);
	float outside = 1.0f - inset;
	color.a = (u8)((float)color.a * fminf(1.0f, clearance * 2.0f));
	GXColor transparent = color;
	transparent.a = 0;
	GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, (u16)count);
	for(int i = 0; i < count; i++) {
		putProjectedRailVertex(raster, eyes[i], joins[i], -inset, color);
	}
	GX_End();
	GX_Begin(GX_QUADS, GX_VTXFMT0, (u16)(count * 4));
	for(int i = 0; i < count; i++) {
		int next = (i + 1) % count;
		putProjectedRailVertex(raster, eyes[i], joins[i], -inset, color);
		putProjectedRailVertex(raster, eyes[i], joins[i], outside, transparent);
		putProjectedRailVertex(raster, eyes[next], joins[next], outside, transparent);
		putProjectedRailVertex(raster, eyes[next], joins[next], -inset, color);
	}
	GX_End();
}

/* A closed band of face-space width centred on a clockwise polyline: the
 * controller's outline and its stick gates. Neighbouring quads share their
 * mitred corners and each boundary fades over one native pixel, so the
 * additive icon pass never double-lights a seam. */
static void drawFaceBand(const cubeRasterTransform_t *raster, int face,
		const indigoPoint_t *centre, int count, float halfWidth, float plane,
		GXColor color)
{
	guVector eyes[2][FACE_BAND_MAX];
	indigoPoint_t points[2][FACE_BAND_MAX], joins[2][FACE_BAND_MAX];
	float area = 0.0f;

	color.a = (u8)((float)color.a * raster->motifAlpha);
	if(count < 3 || count > FACE_BAND_MAX || color.a == 0) return;
	for(int i = 0; i < count; i++) {
		indigoPoint_t prev = centre[(i + count - 1) % count];
		indigoPoint_t next = centre[(i + 1) % count];
		float ax = centre[i].x - prev.x, ay = centre[i].y - prev.y;
		float bx = next.x - centre[i].x, by = next.y - centre[i].y;
		float al = sqrtf(ax * ax + ay * ay), bl = sqrtf(bx * bx + by * by);
		if(al <= 0.0f || bl <= 0.0f) return;
		/* Clockwise with v up: each edge's outward normal is its left side. */
		float nx = -ay / al - by / bl, ny = ax / al + bx / bl;
		float nl = sqrtf(nx * nx + ny * ny);
		if(nl <= 0.0f) return;
		nx /= nl; ny /= nl;
		float miter = nx * (-ay / al) + ny * (ax / al);
		float reach = halfWidth / (miter > 0.5f ? miter : 0.5f);
		for(int side = 0; side < 2; side++) {
			float offset = side == 0 ? -reach : reach;
			guVector point = semanticFacePoint(raster, face,
				centre[i].x + nx * offset, centre[i].y + ny * offset, plane);
			if(!projectRailPoint(raster, point.x, point.y, point.z,
				&eyes[side][i], &points[side][i])) return;
		}
	}
	for(int i = 0; i < count; i++) {
		int next = (i + 1) % count;
		area += points[1][i].x * points[1][next].y - points[1][next].x * points[1][i].y;
	}
	if(area >= -0.001f) return;
	for(int side = 0; side < 2; side++) {
		for(int i = 0; i < count; i++) {
			if(!railJoin(points[side][(i + count - 1) % count], points[side][i],
				points[side][(i + 1) % count], &joins[side][i])) return;
		}
	}
	GXColor transparent = color;
	transparent.a = 0;
	/* Solid band, then the outer fringe outward and the inner fringe inward. */
	GX_Begin(GX_QUADS, GX_VTXFMT0, (u16)(count * 12));
	for(int i = 0; i < count; i++) {
		int next = (i + 1) % count;
		putProjectedRailVertex(raster, eyes[0][i], joins[0][i], 0.0f, color);
		putProjectedRailVertex(raster, eyes[1][i], joins[1][i], 0.0f, color);
		putProjectedRailVertex(raster, eyes[1][next], joins[1][next], 0.0f, color);
		putProjectedRailVertex(raster, eyes[0][next], joins[0][next], 0.0f, color);
		for(int side = 0; side < 2; side++) {
			float fade = side == 0 ? -1.0f : 1.0f;
			putProjectedRailVertex(raster, eyes[side][i], joins[side][i], 0.0f, color);
			putProjectedRailVertex(raster, eyes[side][i], joins[side][i], fade, transparent);
			putProjectedRailVertex(raster, eyes[side][next], joins[side][next], fade, transparent);
			putProjectedRailVertex(raster, eyes[side][next], joins[side][next], 0.0f, color);
		}
	}
	GX_End();
}

/* The Controller icon: an original GameCube controller drawn in face units
 * (v up) in the same glowing strokes and solids as the other icons. */
static void drawControllerCircle(const cubeRasterTransform_t *raster, int face,
		float x, float y, float radius, int segments, float plane, GXColor color)
{
	indigoPoint_t corners[FACE_POLYGON_MAX];

	if(segments < 3 || segments > FACE_POLYGON_MAX) return;
	for(int i = 0; i < segments; i++) {
		float angle = -INDIGO_TAU * (float)i / (float)segments;
		corners[i] = (indigoPoint_t) {x + radius * cosf(angle),
			y + radius * sinf(angle)};
	}
	drawFacePolygon(raster, face, corners, segments, plane, color);
}

/* A ring of segments sides with a vertex on each axis: 8 for the stick
 * gates, more for the round rims of the Disc and Gear icons. */
static void drawFaceRing(const cubeRasterTransform_t *raster, int face,
		float x, float y, float radius, int segments, float halfWidth,
		float plane, GXColor color)
{
	indigoPoint_t corners[FACE_BAND_MAX];

	if(segments < 3 || segments > FACE_BAND_MAX) return;
	for(int i = 0; i < segments; i++) {
		float angle = -INDIGO_TAU * (float)i / (float)segments;
		corners[i] = (indigoPoint_t) {x + radius * cosf(angle),
			y + radius * sinf(angle)};
	}
	drawFaceBand(raster, face, corners, segments, halfWidth, plane, color);
}

static void drawControllerRect(const cubeRasterTransform_t *raster, int face,
		float u0, float v0, float u1, float v1, float plane, GXColor color)
{
	const indigoPoint_t corners[4] = {{u0, v0}, {u0, v1}, {u1, v1}, {u1, v0}};
	drawFacePolygon(raster, face, corners, 4, plane, color);
}

/* The controller's silhouette, clockwise in face units: the union of its
 * two main lobes, the bridge carrying Start, the D-pad and C-stick lobes and
 * the flared grips, traced and simplified by
 * buildtools/ui/controller_outline.py. */
static const float controllerOutline[][2] = {
		{-0.316f, 0.307f}, {-0.23f, 0.285f}, {-0.186f, 0.28f}, {0.144f, 0.279f}, {0.217f, 0.282f},
		{0.288f, 0.303f}, {0.352f, 0.307f}, {0.418f, 0.293f}, {0.468f, 0.266f}, {0.527f, 0.204f},
		{0.562f, 0.129f}, {0.57f, 0.022f}, {0.611f, -0.109f}, {0.667f, -0.324f}, {0.664f, -0.36f},
		{0.645f, -0.399f}, {0.615f, -0.424f}, {0.571f, -0.438f}, {0.539f, -0.434f}, {0.502f, -0.415f},
		{0.473f, -0.38f}, {0.376f, -0.211f}, {0.352f, -0.18f}, {0.324f, -0.158f}, {0.305f, -0.159f},
		{0.295f, -0.17f}, {0.284f, -0.232f}, {0.255f, -0.283f}, {0.215f, -0.308f}, {0.16f, -0.317f},
		{0.118f, -0.308f}, {0.077f, -0.282f}, {0.046f, -0.234f}, {0.035f, -0.164f}, {0.021f, -0.153f},
		{-0.005f, -0.15f}, {-0.022f, -0.153f}, {-0.036f, -0.164f}, {-0.044f, -0.228f}, {-0.073f, -0.277f},
		{-0.112f, -0.305f}, {-0.161f, -0.317f}, {-0.215f, -0.308f}, {-0.256f, -0.283f}, {-0.285f, -0.232f},
		{-0.295f, -0.17f}, {-0.306f, -0.159f}, {-0.325f, -0.158f}, {-0.353f, -0.18f}, {-0.376f, -0.211f},
		{-0.471f, -0.378f}, {-0.497f, -0.411f}, {-0.527f, -0.43f}, {-0.562f, -0.438f}, {-0.595f, -0.434f},
		{-0.628f, -0.417f}, {-0.659f, -0.38f}, {-0.667f, -0.329f}, {-0.632f, -0.182f}, {-0.571f, 0.023f},
		{-0.566f, 0.109f}, {-0.55f, 0.165f}, {-0.51f, 0.23f}, {-0.454f, 0.276f}, {-0.387f, 0.302f}
};

/* A bean: a band of face-space width along a circular arc with round ends,
 * swept clockwise from a0 down to a1 (radians): X, Y and the triggers. */
static void drawFaceBean(const cubeRasterTransform_t *raster, int face,
		float cx, float cy, float radius, float a0, float a1, float halfWidth,
		float plane, GXColor color)
{
	enum { ARC = 10, CAP = 5, POINTS = 2 * ARC + 2 * CAP };
	indigoPoint_t outline[POINTS + 2];
	guVector eyes[POINTS + 2];
	indigoPoint_t points[POINTS + 2], joins[POINTS];
	float area = 0.0f;
	int count = 0;

	color.a = (u8)((float)color.a * raster->motifAlpha);
	if(color.a == 0) return;
	/* Outer arc, the a1 end cap, the inner arc back, then the a0 end cap;
	 * the two cap centres follow for their fans. */
	for(int i = 0; i <= ARC; i++) {
		float angle = a0 + (a1 - a0) * (float)i / ARC;
		outline[count++] = (indigoPoint_t) {cx + (radius + halfWidth) * cosf(angle),
			cy + (radius + halfWidth) * sinf(angle)};
	}
	for(int j = 1; j < CAP; j++) {
		float angle = a1 - INDIGO_TAU * 0.5f * (float)j / CAP;
		outline[count++] = (indigoPoint_t) {cx + radius * cosf(a1) + halfWidth * cosf(angle),
			cy + radius * sinf(a1) + halfWidth * sinf(angle)};
	}
	for(int i = ARC; i >= 0; i--) {
		float angle = a0 + (a1 - a0) * (float)i / ARC;
		outline[count++] = (indigoPoint_t) {cx + (radius - halfWidth) * cosf(angle),
			cy + (radius - halfWidth) * sinf(angle)};
	}
	for(int j = 1; j < CAP; j++) {
		float angle = a0 + INDIGO_TAU * 0.5f * (1.0f - (float)j / CAP);
		outline[count++] = (indigoPoint_t) {cx + radius * cosf(a0) + halfWidth * cosf(angle),
			cy + radius * sinf(a0) + halfWidth * sinf(angle)};
	}
	outline[POINTS] = (indigoPoint_t) {cx + radius * cosf(a1), cy + radius * sinf(a1)};
	outline[POINTS + 1] = (indigoPoint_t) {cx + radius * cosf(a0), cy + radius * sinf(a0)};
	for(int i = 0; i < POINTS + 2; i++) {
		guVector point = semanticFacePoint(raster, face, outline[i].x, outline[i].y, plane);
		if(!projectRailPoint(raster, point.x, point.y, point.z,
			&eyes[i], &points[i])) return;
	}
	for(int i = 0; i < POINTS; i++) {
		int next = (i + 1) % POINTS;
		area += points[i].x * points[next].y - points[next].x * points[i].y;
	}
	if(area >= -0.001f) return;
	for(int i = 0; i < POINTS; i++) {
		if(!railJoin(points[(i + POINTS - 1) % POINTS], points[i],
			points[(i + 1) % POINTS], &joins[i])) return;
	}
	GXColor transparent = color;
	transparent.a = 0;
	/* The arc as a strip of quads: outer i, i+1 against the inner arc. */
	GX_Begin(GX_QUADS, GX_VTXFMT0, ARC * 4);
	for(int i = 0; i < ARC; i++) {
		putProjectedRailVertex(raster, eyes[i], joins[i], 0.0f, color);
		putProjectedRailVertex(raster, eyes[i + 1], joins[i + 1], 0.0f, color);
		putProjectedRailVertex(raster, eyes[2 * ARC + CAP - i - 1],
			joins[2 * ARC + CAP - i - 1], 0.0f, color);
		putProjectedRailVertex(raster, eyes[2 * ARC + CAP - i],
			joins[2 * ARC + CAP - i], 0.0f, color);
	}
	GX_End();
	/* Each round end fans from its centre across outer end, cap, inner end. */
	for(int end = 0; end < 2; end++) {
		int first = end == 0 ? ARC : 2 * ARC + CAP;
		GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, CAP + 2);
		putProjectedRailVertex(raster, eyes[POINTS + end], joins[0], 0.0f, color);
		for(int j = 0; j <= CAP; j++) {
			int index = (first + j) % POINTS;
			putProjectedRailVertex(raster, eyes[index], joins[index], 0.0f, color);
		}
		GX_End();
	}
	GX_Begin(GX_QUADS, GX_VTXFMT0, POINTS * 4);
	for(int i = 0; i < POINTS; i++) {
		int next = (i + 1) % POINTS;
		putProjectedRailVertex(raster, eyes[i], joins[i], 0.0f, color);
		putProjectedRailVertex(raster, eyes[i], joins[i], 1.0f, transparent);
		putProjectedRailVertex(raster, eyes[next], joins[next], 1.0f, transparent);
		putProjectedRailVertex(raster, eyes[next], joins[next], 0.0f, color);
	}
	GX_End();
}

/* Normalised stick axis with a small dead zone, so a resting stick's drift
 * never wiggles the icon. */
static float controllerAxis(s8 value, float fullScale)
{
	float axis = (float)value / fullScale;

	if(fabsf(axis) < 0.1f) return 0.0f;
	return axis > 1.0f ? 1.0f : (axis < -1.0f ? -1.0f : axis);
}

/* Live input wins. After CONTROLLER_IDLE_HOLD seconds without any, the sticks
 * drift and the buttons play a slow press sequence; reduced motion keeps the
 * live mirror and drops that idle play. */
static void controllerPose(const indigoPadFrame_t *pad, float seconds,
		bool animated, controllerIdle_t *idle, controllerPose_t *pose)
{
	static const struct {
		u32 button;
		float start;
		float length;
	} presses[] = {
		{PAD_BUTTON_A, 0.30f, 0.26f}, {PAD_BUTTON_B, 1.20f, 0.22f},
		{PAD_BUTTON_Y, 2.20f, 0.22f}, {PAD_BUTTON_X, 2.80f, 0.22f},
		{PAD_BUTTON_RIGHT, 3.80f, 0.20f}, {PAD_BUTTON_DOWN, 4.20f, 0.20f},
		{PAD_BUTTON_START, 5.10f, 0.22f}
	};
	const u32 shown = PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_X |
		PAD_BUTTON_Y | PAD_BUTTON_START | PAD_BUTTON_UP | PAD_BUTTON_DOWN |
		PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT | PAD_TRIGGER_L | PAD_TRIGGER_R;
	float idleWeight;

	*pose = (controllerPose_t) {0.0f, 0.0f, 0.0f, 0.0f, 0u};
	if(pad != NULL && pad->available) {
		pose->stickX = controllerAxis(pad->stickX, 80.0f);
		pose->stickY = controllerAxis(pad->stickY, 80.0f);
		pose->substickX = controllerAxis(pad->substickX, 72.0f);
		pose->substickY = controllerAxis(pad->substickY, 72.0f);
		pose->pressed = pad->buttons & shown;
		if(pose->pressed != 0u || pose->stickX != 0.0f ||
			pose->stickY != 0.0f || pose->substickX != 0.0f ||
			pose->substickY != 0.0f) {
			idle->lastLiveInput = seconds;
			idle->liveSeen = true;
		}
	}
	idleWeight = !animated ? 0.0f : (!idle->liveSeen ? 1.0f :
		(seconds - idle->lastLiveInput - CONTROLLER_IDLE_HOLD) / 0.8f);
	if(idleWeight <= 0.0f) return;
	if(idleWeight > 1.0f) idleWeight = 1.0f;
	pose->stickX += idleWeight * 0.38f * sinf(seconds * 0.8f);
	pose->stickY += idleWeight * 0.32f * sinf(seconds * 1.1f + 0.6f);
	pose->substickX += idleWeight * 0.34f * sinf(seconds * 1.3f + 2.0f);
	pose->substickY += idleWeight * 0.30f * cosf(seconds * 0.9f);
	if(idleWeight < 1.0f) return;
	float phase = fmodf(seconds, 6.0f);
	for(unsigned i = 0; i < sizeof(presses) / sizeof(presses[0]); i++) {
		if(phase >= presses[i].start &&
			phase < presses[i].start + presses[i].length) {
			pose->pressed |= presses[i].button;
		}
	}
}

/* Every part shares its face's glow; a pressed part sinks to 84% of its
 * size and brightens, the way a lit button reads on the other icons. */
static GXColor controllerGlow(GXColor color, float weight, bool pressed)
{
	float alpha = (float)color.a * weight * (pressed ? 1.3f : 1.0f);

	color.a = (u8)(alpha > 255.0f ? 255.0f : alpha);
	return color;
}

static void drawControllerIcon(const cubeRasterTransform_t *raster, int face,
		GXColor glow, float seconds, bool animated, const indigoPadFrame_t *pad)
{
	static controllerIdle_t idle;
	const float plane = 1.012f;
	controllerPose_t pose;

	controllerPose(pad, seconds, animated, &idle, &pose);
	bool l = (pose.pressed & PAD_TRIGGER_L) != 0u;
	bool r = (pose.pressed & PAD_TRIGGER_R) != 0u;
	bool a = (pose.pressed & PAD_BUTTON_A) != 0u;
	bool b = (pose.pressed & PAD_BUTTON_B) != 0u;
	bool x = (pose.pressed & PAD_BUTTON_X) != 0u;
	bool y = (pose.pressed & PAD_BUTTON_Y) != 0u;
	bool start = (pose.pressed & PAD_BUTTON_START) != 0u;
	float magnitude = sqrtf(pose.stickX * pose.stickX + pose.stickY * pose.stickY);
	float cMagnitude = sqrtf(pose.substickX * pose.substickX +
		pose.substickY * pose.substickY);
	/* A cap travels until a small gap remains inside its gate's inner edge. */
	float reach = (magnitude > 1.0f ? 1.0f / magnitude : 1.0f) * 0.030f;
	float cReach = (cMagnitude > 1.0f ? 1.0f / cMagnitude : 1.0f) * 0.016f;
	const float degree = INDIGO_TAU / 360.0f;
	indigoPoint_t outline[FACE_BAND_MAX];
	int outlineCount = (int)(sizeof(controllerOutline) / sizeof(controllerOutline[0]));

	for(int i = 0; i < outlineCount; i++) {
		outline[i] = (indigoPoint_t) {controllerOutline[i][0], controllerOutline[i][1]};
	}
	/* Nothing overlaps: the pass is additive. */
	drawFaceBand(raster, face, outline, outlineCount, 0.018f,
		plane, controllerGlow(glow, 0.78f, false));
	/* The triggers ride the shoulders and press in toward the body. */
	drawFaceBean(raster, face, -0.335f, 0.075f, l ? 0.28f : 0.30f,
		152.0f * degree, 100.0f * degree, 0.022f, plane, controllerGlow(glow, 0.9f, l));
	drawFaceBean(raster, face, 0.335f, 0.075f, r ? 0.28f : 0.30f,
		80.0f * degree, 28.0f * degree, 0.022f, plane, controllerGlow(glow, 0.9f, r));

	/* Both sticks follow the live controller inside their octagonal gates. */
	drawFaceRing(raster, face, -0.335f, 0.075f, 0.12f, 8, 0.014f, plane,
		controllerGlow(glow, 0.9f, false));
	drawControllerCircle(raster, face, -0.335f + pose.stickX * reach,
		0.075f + pose.stickY * reach, 0.058f, 20, plane, controllerGlow(glow, 1.0f, false));
	drawFaceRing(raster, face, 0.165f, -0.19f, 0.054f, 8, 0.012f, plane,
		controllerGlow(glow, 0.9f, false));
	drawControllerCircle(raster, face, 0.165f + pose.substickX * cReach,
		-0.19f + pose.substickY * cReach, 0.024f, 14, plane,
		controllerGlow(glow, 1.0f, false));

	/* The face cluster: a large A, B below-left, X and Y curving round A. */
	drawControllerCircle(raster, face, 0.335f, 0.075f, a ? 0.066f : 0.078f, 24, plane,
		controllerGlow(glow, 1.05f, a));
	drawControllerCircle(raster, face, 0.215f, -0.025f, b ? 0.030f : 0.036f, 16, plane,
		controllerGlow(glow, 1.0f, b));
	drawFaceBean(raster, face, 0.335f, 0.075f, 0.128f,
		42.0f * degree, -34.0f * degree, x ? 0.019f : 0.024f, plane,
		controllerGlow(glow, 1.0f, x));
	drawFaceBean(raster, face, 0.335f, 0.075f, 0.128f,
		172.0f * degree, 102.0f * degree, y ? 0.019f : 0.024f, plane,
		controllerGlow(glow, 1.0f, y));
	drawControllerCircle(raster, face, 0.0f, 0.10f, start ? 0.018f : 0.022f, 12,
		plane, controllerGlow(glow, 0.9f, start));

	/* Four separate D-pad arms around an open centre; a held arm brightens. */
	drawControllerRect(raster, face, -0.182f, -0.167f, -0.148f, -0.117f, plane,
		controllerGlow(glow, 1.0f, (pose.pressed & PAD_BUTTON_UP) != 0u));
	drawControllerRect(raster, face, -0.182f, -0.263f, -0.148f, -0.213f, plane,
		controllerGlow(glow, 1.0f, (pose.pressed & PAD_BUTTON_DOWN) != 0u));
	drawControllerRect(raster, face, -0.238f, -0.207f, -0.188f, -0.173f, plane,
		controllerGlow(glow, 1.0f, (pose.pressed & PAD_BUTTON_LEFT) != 0u));
	drawControllerRect(raster, face, -0.142f, -0.207f, -0.092f, -0.173f, plane,
		controllerGlow(glow, 1.0f, (pose.pressed & PAD_BUTTON_RIGHT) != 0u));
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

static void buildChamferStrips(cubeSurfaceQuad_t quads[12], float outer, float inset,
		const GXColor lit[6])
{
	const float o = outer;
	const float i = inset;
	enum { BACK, LEFT, RIGHT, BOTTOM, TOP, FRONT };

	/* Each bevel runs from the lit tint of one neighbouring face to the other.
	 * Its place before or after the glass is determined by camera-facing
	 * culling, so a vertical turn cannot leave an old front strip on top of
	 * the pane. */
	buildChamferStrip(&quads[0], -i,i,-o, i,i,-o, i,o,-i, -i,o,-i, lit[BACK], lit[TOP]);
	buildChamferStrip(&quads[1], -i,-o,-i, i,-o,-i, i,-i,-o, -i,-i,-o, lit[BOTTOM], lit[BACK]);
	buildChamferStrip(&quads[2], -i,-i,-o, -i,i,-o, -o,i,-i, -o,-i,-i, lit[BACK], lit[LEFT]);
	buildChamferStrip(&quads[3], o,-i,-i, o,i,-i, i,i,-o, i,-i,-o, lit[RIGHT], lit[BACK]);

	buildChamferStrip(&quads[4], -o,i,-i, -o,i,i, -i,o,i, -i,o,-i, lit[LEFT], lit[TOP]);
	buildChamferStrip(&quads[5], i,o,-i, i,o,i, o,i,i, o,i,-i, lit[TOP], lit[RIGHT]);
	buildChamferStrip(&quads[6], -i,-o,-i, -i,-o,i, -o,-i,i, -o,-i,-i, lit[BOTTOM], lit[LEFT]);
	buildChamferStrip(&quads[7], o,-i,-i, o,-i,i, i,-o,i, i,-o,-i, lit[RIGHT], lit[BOTTOM]);
	buildChamferStrip(&quads[8], -i,o,i, i,o,i, i,i,o, -i,i,o, lit[TOP], lit[FRONT]);
	buildChamferStrip(&quads[9], -i,-i,o, i,-i,o, i,-o,i, -i,-o,i, lit[FRONT], lit[BOTTOM]);
	buildChamferStrip(&quads[10], -o,-i,i, -o,i,i, -i,i,o, -i,-i,o, lit[LEFT], lit[FRONT]);
	buildChamferStrip(&quads[11], i,-i,o, i,i,o, o,i,i, o,-i,i, lit[FRONT], lit[RIGHT]);
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

/* A central port with four linked endpoints: 9 quads, 180 vertices. */
static void drawHubIcon(const cubeRasterTransform_t *raster, int face, GXColor glow)
{
	const float plane = 1.012f;

	GX_Begin(GX_QUADS, GX_VTXFMT0, 180);
		putSemanticFaceDiamond(raster, face, 0.0f, 0.0f, 0.17f, plane, glow);
		putSemanticFaceDiamond(raster, face, -0.48f, 0.0f, 0.09f, plane, glow);
		putSemanticFaceDiamond(raster, face, 0.48f, 0.0f, 0.09f, plane, glow);
		putSemanticFaceDiamond(raster, face, 0.0f, -0.48f, 0.09f, plane, glow);
		putSemanticFaceDiamond(raster, face, 0.0f, 0.48f, 0.09f, plane, glow);
		putSemanticMotifRect(raster, face, -0.40f, -0.025f, -0.16f, 0.025f, plane, glow);
		putSemanticMotifRect(raster, face, 0.16f, -0.025f, 0.40f, 0.025f, plane, glow);
		putSemanticMotifRect(raster, face, -0.025f, -0.40f, 0.025f, -0.16f, plane, glow);
		putSemanticMotifRect(raster, face, -0.025f, 0.16f, 0.025f, 0.40f, plane, glow);
	GX_End();
}

/* Three calm tracks with independently placed controls: 6 quads, 120
 * vertices. slider drifts the knobs while motion is on. */
static void drawSlidersIcon(const cubeRasterTransform_t *raster, int face,
		GXColor glow, float slider)
{
	const float plane = 1.012f;

	GX_Begin(GX_QUADS, GX_VTXFMT0, 120);
	for(int track = 0; track < 3; ++track) {
		float y = -0.42f + (float)track * 0.42f;
		float knob = (track == 0 ? -0.25f : (track == 1 ? 0.12f : 0.34f));
		knob += (track == 1 ? slider : -slider * 0.45f);
		putSemanticMotifRect(raster, face, -0.55f, y - 0.025f,
			0.55f, y + 0.025f, plane, glow);
		putSemanticMotifRect(raster, face, knob - 0.065f,
			y - 0.13f, knob + 0.065f, y + 0.13f, plane, glow);
	}
	GX_End();
}

/* A framed live clock with three civil-time hands and cardinal ticks; the
 * hand tails form a compact luminous hub. 11 quads, 220 vertices. Invalid
 * civil time emits transparent degenerate hands, never an invented time. */
static void drawClockIcon(const cubeRasterTransform_t *raster, int face,
		GXColor glow, const uiClockFrame_t *clock)
{
	const float plane = 1.012f;
	GXColor secondHand = {glow.r, glow.g, glow.b, 228};
	GXColor hiddenHand = {0, 0, 0, 0};
	bool clockAvailable = clock != NULL && clock->available;

	GX_Begin(GX_QUADS, GX_VTXFMT0, 220);
		putSemanticFaceHand(raster, face,
			clockAvailable ? clock->hourX : 0.0f,
			clockAvailable ? clock->hourY : 0.0f,
			clockAvailable ? 0.34f : 0.0f,
			clockAvailable ? 0.040f : 0.0f,
			clockAvailable ? 0.045f : 0.0f, plane,
			clockAvailable ? glow : hiddenHand);
		putSemanticFaceHand(raster, face,
			clockAvailable ? clock->minuteX : 0.0f,
			clockAvailable ? clock->minuteY : 0.0f,
			clockAvailable ? 0.49f : 0.0f,
			clockAvailable ? 0.027f : 0.0f,
			clockAvailable ? 0.055f : 0.0f, plane,
			clockAvailable ? glow : hiddenHand);
		putSemanticFaceHand(raster, face,
			clockAvailable ? clock->secondX : 0.0f,
			clockAvailable ? clock->secondY : 0.0f,
			clockAvailable ? 0.56f : 0.0f,
			clockAvailable ? 0.013f : 0.0f,
			clockAvailable ? 0.090f : 0.0f, plane,
			clockAvailable ? secondHand : hiddenHand);
		putSemanticMotifRect(raster, face, -0.55f, 0.55f, 0.55f, 0.61f, plane, glow);
		putSemanticMotifRect(raster, face, -0.55f, -0.61f, 0.55f, -0.55f, plane, glow);
		putSemanticMotifRect(raster, face, -0.61f, -0.55f, -0.55f, 0.55f, plane, glow);
		putSemanticMotifRect(raster, face, 0.55f, -0.55f, 0.61f, 0.55f, plane, glow);
		putSemanticMotifRect(raster, face, -0.035f, 0.55f, 0.035f, 0.66f, plane, glow);
		putSemanticMotifRect(raster, face, -0.035f, -0.66f, 0.035f, -0.55f, plane, glow);
		putSemanticMotifRect(raster, face, -0.66f, -0.035f, -0.55f, 0.035f, plane, glow);
		putSemanticMotifRect(raster, face, 0.55f, -0.035f, 0.66f, 0.035f, plane, glow);
	GX_End();
}

/* Banded books on a shelf, the last leaning into the row so it reads as a
 * bookshelf, not a bar chart: 13 quads, 260 vertices. The lean clears the
 * third spine; additive blending would double-light any overlap. */
static void drawBooksIcon(const cubeRasterTransform_t *raster, int face, GXColor glow)
{
	const float plane = 1.012f;

	GX_Begin(GX_QUADS, GX_VTXFMT0, 260);
		putSemanticFaceBook(raster, face, -0.585f, -0.46f, 0.20f, 0.90f, 0.0f, plane, glow);
		putSemanticFaceBook(raster, face, -0.35f, -0.46f, 0.25f, 1.00f, 0.0f, plane, glow);
		putSemanticFaceBook(raster, face, -0.065f, -0.46f, 0.17f, 0.80f, 0.0f, plane, glow);
		putSemanticFaceBook(raster, face, 0.412f, -0.46f, 0.19f, 0.84f, 0.349f, plane, glow);
		putSemanticMotifRect(raster, face, -0.64f, -0.555f, 0.64f, -0.49f, plane, glow);
	GX_End();
}

/* A GameCube disc: its rim, the ring round the centre hole and two glints
 * between them, which spin turns while motion is on. */
static void drawDiscIcon(const cubeRasterTransform_t *raster, int face,
		GXColor glow, float spin)
{
	const float plane = 1.012f;
	const float degree = INDIGO_TAU / 360.0f;

	drawFaceRing(raster, face, 0.0f, 0.0f, 0.52f, 32, 0.020f, plane, glow);
	drawFaceRing(raster, face, 0.0f, 0.0f, 0.14f, 16, 0.018f, plane, glow);
	for(int glint = 0; glint < 2; glint++) {
		float from = spin + 125.0f * degree + (float)glint * INDIGO_TAU * 0.5f;
		drawFaceBean(raster, face, 0.0f, 0.0f, 0.33f, from, from - 50.0f * degree,
			0.028f, plane, glow);
	}
}

/* A gear: six rounded teeth and its centre hole, turned by turn. tanh
 * squares each tooth off; at this sharpness every corner of the 78-point
 * outline stays under the 45 degrees drawFaceBand needs. */
static void drawGearIcon(const cubeRasterTransform_t *raster, int face,
		GXColor glow, float turn)
{
	enum { TEETH = 6, POINTS = TEETH * 13 };
	const float plane = 1.012f;
	indigoPoint_t outline[POINTS];

	for(int i = 0; i < POINTS; i++) {
		float angle = -INDIGO_TAU * (float)i / (float)POINTS;
		float radius = 0.45f + 0.085f * tanhf(2.0f * sinf(TEETH * angle)) / tanhf(2.0f);
		outline[i] = (indigoPoint_t) {radius * cosf(angle + turn),
			radius * sinf(angle + turn)};
	}
	drawFaceBand(raster, face, outline, POINTS, 0.018f, plane, glow);
	drawFaceRing(raster, face, 0.0f, 0.0f, 0.15f, 16, 0.018f, plane, glow);
}

/* Every face shows the icon chosen for it in Settings (a uiHomeIcon_t per
 * face), in one additive pass without depth writes, and every icon glows in
 * the same lilac. Each icon's primitive count is fixed, so a face turning
 * away draws transparent degenerates or nothing; None, and any value out of
 * range, draws nothing. */
static void drawFaceIcons(float seconds, bool animated,
		const uiClockFrame_t *clock, const indigoPadFrame_t *pad,
		const int icons[UI_HOME_FACE_COUNT], const cubeRasterTransform_t *raster)
{
	float pulse = animated ? 0.5f + sinf(seconds * 1.10f) * 0.5f : 0.62f;
	float slider = animated ? sinf(seconds * 0.43f) * 0.12f : 0.0f;
	GXColor glow = {196, 177, 255, (u8)(142.0f + pulse * 42.0f)};
	Mtx identity;

	guMtxIdentity(identity);
	GX_LoadPosMtxImm(identity, GX_PNMTX0);
	GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_FALSE);
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
	for(int face = 0; face < UI_HOME_FACE_COUNT; face++) {
		switch(icons[face]) {
			case UI_HOME_ICON_CONTROLLER:
				drawControllerIcon(raster, face, glow, seconds, animated, pad);
				break;
			case UI_HOME_ICON_BOOKS: drawBooksIcon(raster, face, glow); break;
			case UI_HOME_ICON_HUB: drawHubIcon(raster, face, glow); break;
			case UI_HOME_ICON_DISC:
				drawDiscIcon(raster, face, glow, animated ? seconds * 0.6f : 0.0f);
				break;
			case UI_HOME_ICON_SLIDERS: drawSlidersIcon(raster, face, glow, slider); break;
			case UI_HOME_ICON_GEAR:
				drawGearIcon(raster, face, glow, animated ? seconds * 0.2f : 0.0f);
				break;
			case UI_HOME_ICON_CLOCK: drawClockIcon(raster, face, glow, clock); break;
			default: break;
		}
	}
	GX_LoadPosMtxImm(raster->model, GX_PNMTX0);
	GX_SetCullMode(GX_CULL_BACK);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
		GX_LO_CLEAR);
}

static float glassSmoothstep(float edge0, float edge1, float x)
{
	float t = (x - edge0) / (edge1 - edge0);
	t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	return t * t * (3.0f - 2.0f * t);
}

/* What the glass reflects at an eye-space point with unit normal n: soft
 * light cards fixed to the camera, standing in for the environment the IPL's
 * cube mirrors. A broad key lays a gentle gradient across the faces; narrow
 * cards catch the rounded bevels as thin glints. Fresnel brightens glancing
 * glass, and the last few degrees before the silhouette fade out so the
 * reflection never draws a hard aliased rim. */
static GXColor glassReflection(guVector eye, guVector n)
{
	static const struct {
		guVector direction;
		float start, full, weight;
		GXColor color;
	} lights[] = {
		{{0.861f, 0.148f, 0.487f}, 0.74f, 0.99f, 0.60f, {200, 214, 255, 0}}, /* key */
		{{0.097f, 0.970f, 0.222f}, 0.88f, 0.97f, 1.20f, {246, 244, 255, 0}}, /* overhead */
		{{0.958f, 0.240f, 0.157f}, 0.88f, 0.97f, 1.00f, {226, 206, 255, 0}}, /* right */
		{{0.000f, 0.350f, -0.937f}, 0.75f, 0.97f, 0.25f, {168, 146, 255, 0}}  /* rim */
	};
	GXColor out = {0, 0, 0, 0};
	float length = sqrtf(eye.x * eye.x + eye.y * eye.y + eye.z * eye.z);
	guVector view = {-eye.x / length, -eye.y / length, -eye.z / length};
	float facing = n.x * view.x + n.y * view.y + n.z * view.z;
	if(facing <= 0.0f) return out;
	guVector r = {2.0f * facing * n.x - view.x, 2.0f * facing * n.y - view.y,
		2.0f * facing * n.z - view.z};
	float glancing = 1.0f - facing;
	float fresnel = 0.35f + 0.65f * glancing * glancing;
	float light = 0.0f, red = 0.0f, green = 0.0f, blue = 0.0f;
	for(unsigned i = 0; i < sizeof(lights) / sizeof(lights[0]); i++) {
		float d = r.x * lights[i].direction.x + r.y * lights[i].direction.y +
			r.z * lights[i].direction.z;
		float s = lights[i].weight * glassSmoothstep(lights[i].start, lights[i].full, d);
		light += s;
		red += s * lights[i].color.r;
		green += s * lights[i].color.g;
		blue += s * lights[i].color.b;
	}
	if(light <= 0.0f) return out;
	float alpha = 255.0f * fresnel * light * fminf(1.0f, facing * 6.0f);
	return (GXColor) {(u8)(red / light), (u8)(green / light), (u8)(blue / light),
		(u8)(alpha > 255.0f ? 255.0f : alpha)};
}

/* Face and bevel vertices each lie on exactly one outer plane, whose axis is
 * their normal. Interpolating those normals across a flat bevel shades it
 * as a rounded, polished edge that the reflection rolls over. */
static guVector glassVertexNormal(const cubeRasterTransform_t *raster,
		guVector p, float outer)
{
	guVector axis = {
		fabsf(p.x) >= outer - 0.001f ? (p.x < 0.0f ? -1.0f : 1.0f) : 0.0f,
		fabsf(p.y) >= outer - 0.001f ? (p.y < 0.0f ? -1.0f : 1.0f) : 0.0f,
		fabsf(p.z) >= outer - 0.001f ? (p.z < 0.0f ? -1.0f : 1.0f) : 0.0f
	};
	return cubeViewNormal(raster, axis);
}

static bool glassSameNormal(guVector a, guVector b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

static bool glassCellLit(const GXColor *color, int first, int stride)
{
	return (color[first].a | color[first + 1].a | color[first + stride].a |
		color[first + stride + 1].a) != 0;
}

static guVector glassBilinear(const guVector corner[4], float u, float v)
{
	float w0 = (1.0f - u) * (1.0f - v), w1 = u * (1.0f - v), w2 = u * v, w3 = (1.0f - u) * v;
	return (guVector) {
		w0 * corner[0].x + w1 * corner[1].x + w2 * corner[2].x + w3 * corner[3].x,
		w0 * corner[0].y + w1 * corner[1].y + w2 * corner[2].y + w3 * corner[3].y,
		w0 * corner[0].z + w1 * corner[1].z + w2 * corner[2].z + w3 * corner[3].z
	};
}

/* Additive reflection over the camera-facing outer glass. Each quad is split
 * into a grid, keeping its winding, so the per-vertex highlight stays smooth:
 * finely across a bevel, where its normal turns, and coarsely along it.
 * Unlit cells are skipped. Corner triangles shade at their three vertices. */
static void drawGlassReflection(const cubeRasterTransform_t *raster,
		const cubeSurfaceQuad_t *quads, int count, int vertexCount, float outer)
{
	enum { STEPS = 10, BEVEL_STEPS = 4 };

	for(int quad = 0; quad < count; quad++) {
		guVector eyes[4], normals[4], body[(STEPS + 1) * (STEPS + 1)];
		indigoPoint_t points[4];
		GXColor color[(STEPS + 1) * (STEPS + 1)];
		float area = 0.0f;
		int cells = 0;
		for(int vertex = 0; vertex < vertexCount; vertex++) {
			guVector p = quads[quad].point[vertex];
			if(!projectRailPoint(raster, p.x, p.y, p.z, &eyes[vertex], &points[vertex])) return;
			normals[vertex] = glassVertexNormal(raster, p, outer);
		}
		for(int vertex = 0; vertex < vertexCount; vertex++) {
			int next = (vertex + 1) % vertexCount;
			area += points[vertex].x * points[next].y - points[next].x * points[vertex].y;
		}
		if(area >= -0.001f) continue;
		if(vertexCount == 3) {
			for(int vertex = 0; vertex < 3; vertex++)
				color[vertex] = glassReflection(eyes[vertex], normals[vertex]);
			if((color[0].a | color[1].a | color[2].a) == 0) continue;
			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 3);
			for(int vertex = 0; vertex < 3; vertex++) {
				guVector p = quads[quad].point[vertex];
				putCubeVertex(p.x, p.y, p.z, color[vertex]);
			}
			GX_End();
			continue;
		}
		/* A bevel's normal changes only between its two sides. */
		bool acrossU = !glassSameNormal(normals[0], normals[1]);
		bool acrossV = !glassSameNormal(normals[0], normals[3]);
		int columns = acrossU || !acrossV ? STEPS : BEVEL_STEPS;
		int rows = acrossV || !acrossU ? STEPS : BEVEL_STEPS;
		for(int row = 0; row <= rows; row++) for(int column = 0; column <= columns; column++) {
			float u = (float)column / columns, v = (float)row / rows;
			int at = row * (STEPS + 1) + column;
			guVector n = glassBilinear(normals, u, v);
			float length = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
			n = (guVector) {n.x / length, n.y / length, n.z / length};
			body[at] = glassBilinear(quads[quad].point, u, v);
			color[at] = glassReflection(glassBilinear(eyes, u, v), n);
		}
		for(int row = 0; row < rows; row++) for(int column = 0; column < columns; column++)
			cells += glassCellLit(color, row * (STEPS + 1) + column, STEPS + 1);
		if(cells == 0) continue;
		GX_Begin(GX_QUADS, GX_VTXFMT0, cells * 4);
		for(int row = 0; row < rows; row++) for(int column = 0; column < columns; column++) {
			static const int corner[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
			int first = row * (STEPS + 1) + column;
			if(!glassCellLit(color, first, STEPS + 1)) continue;
			for(int vertex = 0; vertex < 4; vertex++) {
				int at = first + corner[vertex][1] * (STEPS + 1) + corner[vertex][0];
				putCubeVertex(body[at].x, body[at].y, body[at].z, color[at]);
			}
		}
		GX_End();
	}
}

/* Screen-space glass. The frame behind the cube is copied from the EFB at
 * half resolution into one RGBA8 texture, which the glass then refracts,
 * and after the cube is drawn the same texture carries its bloom. Half
 * resolution is the look, not only the budget: a soft copy reads as depth
 * inside the glass and as a wide glow. RGBA8 keeps the dark indigo
 * gradient free of the banding a 16-bit copy would show. */
#define GLASS_COPY_MAX_W 320
#define GLASS_COPY_MAX_H 288
#define GLASS_BLOOM_TAPS 5

static u8 glassTexels[GLASS_COPY_MAX_W * GLASS_COPY_MAX_H * 4] ATTRIBUTE_ALIGN(32);
static GXTexObj glassTexObj;
static u16 glassEfbWidth = 640;
static u16 glassEfbHeight = 480;
static u16 glassCopyWidth;
static u16 glassCopyHeight;
static bool glassTexelsFlushed;

void IndigoBackground_SetFramebuffer(u16 width, u16 height)
{
	glassEfbWidth = width;
	glassEfbHeight = height;
}

/* Where the light that refracts and glints comes from, and how much of it a
 * scene shows. Home and Source carry the full effect; the cube behind the
 * Library, a game's details and Settings keeps a quieter version of it. */
typedef struct glassSun {
	float peak;
	float x;
	float y;
	float weight;
} glassSun_t;

typedef struct glassRefraction {
	float centerX;
	float centerY;
	float bend;
	float magnify;
	float dispersion;
	float sScale;
	float tScale;
	GXColor tint;
} glassRefraction_t;

/* Toward the key light, up and to the right of the camera, in eye space. */
static const guVector glassSunDirection = {0.595f, 0.672f, 0.441f};

static float glassSceneStrength(const uiSceneFrame_t *scene)
{
	float strength = scene->orbitStrength < 0.0f ? 0.0f :
		(scene->orbitStrength > 1.0f ? 1.0f : scene->orbitStrength);
	if(scene->scene == UI_SCENE_SETTINGS) {
		/* Settings covers the whole screen; the cube there is only seen
		 * while the page fades, so it keeps just the vertex light. */
		return 0.0f;
	}
	/* During the boot reveal the overlay's cube is plain glass. The light
	 * fades in once the background cube takes over, so nothing pops. */
	return strength * glassSmoothstep(BOOT_CUBE_HANDOFF, 1.0f, scene->introProgress);
}

/* The copy runs in the pixel pipeline behind every draw already queued, so
 * it holds exactly what the frame shows at this point. The CPU never reads
 * the texels; their cache lines are written back once, before the GPU first
 * writes the buffer, so no stale line can later overwrite a copy. */
static bool glassCopyFrame(void)
{
	u16 width = (u16)((glassEfbWidth / 2u) & ~3u);
	u16 height = (u16)((glassEfbHeight / 2u) & ~3u);

	if(width < 4u || height < 4u) return false;
	if(width > GLASS_COPY_MAX_W) width = GLASS_COPY_MAX_W;
	if(height > GLASS_COPY_MAX_H) height = GLASS_COPY_MAX_H;
	if(!glassTexelsFlushed) {
		DCFlushRange(glassTexels, sizeof(glassTexels));
		glassTexelsFlushed = true;
	}
	GX_SetTexCopySrc(0, 0, (u16)(width * 2u), (u16)(height * 2u));
	GX_SetTexCopyDst(width, height, GX_TF_RGBA8, GX_TRUE);
	GX_CopyTex(glassTexels, GX_FALSE);
	GX_PixModeSync();
	GX_InitTexObj(&glassTexObj, glassTexels, width, height, GX_TF_RGBA8,
		GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&glassTexObj, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f,
		GX_FALSE, GX_FALSE, GX_ANISO_1);
	GX_InvalidateTexAll();
	GX_LoadTexObj(&glassTexObj, GX_TEXMAP0);
	glassCopyWidth = width;
	glassCopyHeight = height;
	return true;
}

/* Texture coordinates of a screen point (640 x 480 units) in the copy. */
static float glassCopyS(void)
{
	return (float)glassEfbWidth / (float)(glassCopyWidth * 2u) / 640.0f;
}

static float glassCopyT(void)
{
	return (float)glassEfbHeight / (float)(glassCopyHeight * 2u) / 480.0f;
}

/* Vertex color back to the only TEV input: the state every cube pass and
 * setupCubePipeline share. */
static void restoreCubeRaster(void)
{
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetNumTexGens(0);
	GX_SetNumTevStages(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_1);
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_1);
}

/* Dispersion: three samples of the copy, one per channel, each bent a
 * little further than the last, so edges part into red, green and blue the
 * way a prism does. Stage 3 multiplies the vertex tint in at double scale,
 * so 128 leaves the light as it came through. */
static void setupGlassRefractionPipeline(void)
{
	static const GXColor masks[3] = {{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}};

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX1, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX2, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX2, GX_TEX_ST, GX_F32, 0);
	GX_SetNumTexGens(3);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_TEX2, GX_IDENTITY);
	GX_SetNumTevStages(4);
	for(int channel = 0; channel < 3; channel++) {
		u8 stage = (u8)(GX_TEVSTAGE0 + channel);
		GX_SetTevKColor((u8)(GX_KCOLOR0 + channel), masks[channel]);
		GX_SetTevKColorSel(stage, (u8)(GX_TEV_KCSEL_K0 + channel));
		GX_SetTevOrder(stage, (u8)(GX_TEXCOORD0 + channel), GX_TEXMAP0, GX_COLOR0A0);
		GX_SetTevColorIn(stage, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST,
			channel == 0 ? GX_CC_ZERO : GX_CC_CPREV);
		GX_SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
		GX_SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
		GX_SetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	}
	GX_SetTevOrder(GX_TEVSTAGE3, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevColorIn(GX_TEVSTAGE3, GX_CC_ZERO, GX_CC_CPREV, GX_CC_RASC, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2, GX_ENABLE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE3, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
}

/* What one glass vertex refracts: its body position, tint and the three
 * channels' coordinates in the copy. */
typedef struct glassRefractedVertex {
	guVector body;
	GXColor color;
	float s[3];
	float t[3];
} glassRefractedVertex_t;

/* A convex surface bends the rays behind it toward its middle, so the frame
 * is sampled nearer the cube's centre: a slight magnification across the
 * faces and a strong pull round the rounded bevels, where the normal turns
 * sideways. Each channel bends a little more than the last. The layer fades
 * out over the last glancing degrees, so at the silhouette the bent image
 * meets the straight one. The sun term finds where the glass mirrors the
 * key light toward the camera, weighted hard toward the brightest point. */
static void refractGlassVertex(const cubeRasterTransform_t *raster,
		const glassRefraction_t *glass, guVector body, guVector eye, guVector n,
		glassSun_t *sun, glassRefractedVertex_t *out)
{
	float length = sqrtf(eye.x * eye.x + eye.y * eye.y + eye.z * eye.z);
	guVector view = {-eye.x / length, -eye.y / length, -eye.z / length};
	float facing = n.x * view.x + n.y * view.y + n.z * view.z;
	float sx = 320.0f + raster->scaleX * eye.x / -eye.z;
	float sy = 240.0f - raster->scaleY * eye.y / -eye.z;
	float bx = glass->centerX + (sx - glass->centerX) * (1.0f - glass->magnify);
	float by = glass->centerY + (sy - glass->centerY) * (1.0f - glass->magnify);
	float ox = -n.x * glass->bend, oy = n.y * glass->bend;

	if(facing > 0.0f) {
		guVector r = {2.0f * facing * n.x - view.x, 2.0f * facing * n.y - view.y,
			2.0f * facing * n.z - view.z};
		float d = r.x * glassSunDirection.x + r.y * glassSunDirection.y +
			r.z * glassSunDirection.z;
		float glint = glassSmoothstep(0.955f, 0.9985f, d) *
			glassSmoothstep(0.06f, 0.30f, facing);
		if(glint > sun->peak) sun->peak = glint;
		glint *= glint * glint * glint;
		sun->x += sx * glint;
		sun->y += sy * glint;
		sun->weight += glint;
	}
	out->body = body;
	out->color = glass->tint;
	out->color.a = (u8)(255.0f * glassSmoothstep(0.0f, 0.34f, facing) + 0.5f);
	UIColor_Apply(&out->color.r, &out->color.g, &out->color.b);
	for(int channel = 0; channel < 3; channel++) {
		float k = 1.0f + glass->dispersion * (float)(channel - 1);
		out->s[channel] = (bx + ox * k) * glass->sScale;
		out->t[channel] = (by + oy * k) * glass->tScale;
	}
}

static void putGlassRefractedVertex(const glassRefractedVertex_t *vertex)
{
	GX_Position3f32(vertex->body.x, vertex->body.y, vertex->body.z);
	GX_Color4u8(vertex->color.r, vertex->color.g, vertex->color.b, vertex->color.a);
	for(int channel = 0; channel < 3; channel++)
		GX_TexCoord2f32(vertex->s[channel], vertex->t[channel]);
}

/* The camera-facing outer glass as a refracting layer over the copy: faces
 * in a coarse grid, where only perspective needs the extra vertices, and
 * bevels finely across their width, where the normal rolls. It is drawn
 * before the dark core, which then covers the middle, so what refracts is
 * the frame seen through the glass around it. With emit false the same walk
 * only measures the sun glint (no copy this frame). */
static void drawGlassRefraction(const cubeRasterTransform_t *raster,
		const glassRefraction_t *glass, const cubeSurfaceQuad_t *quads, int count,
		int vertexCount, float outer, glassSun_t *sun, bool emit)
{
	enum { FACE_STEPS = 4, ACROSS_STEPS = 6, ALONG_STEPS = 3, GRID = 7 };

	for(int quad = 0; quad < count; quad++) {
		guVector eyes[4], normals[4];
		glassRefractedVertex_t grid[GRID * GRID];
		indigoPoint_t points[4];
		float area = 0.0f;
		for(int vertex = 0; vertex < vertexCount; vertex++) {
			guVector p = quads[quad].point[vertex];
			if(!projectRailPoint(raster, p.x, p.y, p.z, &eyes[vertex], &points[vertex])) return;
			normals[vertex] = glassVertexNormal(raster, p, outer);
		}
		for(int vertex = 0; vertex < vertexCount; vertex++) {
			int next = (vertex + 1) % vertexCount;
			area += points[vertex].x * points[next].y - points[next].x * points[vertex].y;
		}
		if(area >= -0.001f) continue;
		if(vertexCount == 3) {
			for(int vertex = 0; vertex < 3; vertex++)
				refractGlassVertex(raster, glass, quads[quad].point[vertex],
					eyes[vertex], normals[vertex], sun, &grid[vertex]);
			if(!emit) continue;
			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 3);
			for(int vertex = 0; vertex < 3; vertex++) putGlassRefractedVertex(&grid[vertex]);
			GX_End();
			continue;
		}
		bool acrossU = !glassSameNormal(normals[0], normals[1]);
		bool acrossV = !glassSameNormal(normals[0], normals[3]);
		int columns = acrossU ? ACROSS_STEPS : (acrossV ? ALONG_STEPS : FACE_STEPS);
		int rows = acrossV ? ACROSS_STEPS : (acrossU ? ALONG_STEPS : FACE_STEPS);
		for(int row = 0; row <= rows; row++) for(int column = 0; column <= columns; column++) {
			float u = (float)column / columns, v = (float)row / rows;
			guVector normal = glassBilinear(normals, u, v);
			float normalLength = sqrtf(normal.x * normal.x + normal.y * normal.y +
				normal.z * normal.z);
			normal = (guVector) {normal.x / normalLength, normal.y / normalLength,
				normal.z / normalLength};
			refractGlassVertex(raster, glass, glassBilinear(quads[quad].point, u, v),
				glassBilinear(eyes, u, v), normal, sun, &grid[row * GRID + column]);
		}
		if(!emit) continue;
		GX_Begin(GX_QUADS, GX_VTXFMT0, (u16)(rows * columns * 4));
		for(int row = 0; row < rows; row++) for(int column = 0; column < columns; column++) {
			static const int corner[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
			for(int vertex = 0; vertex < 4; vertex++)
				putGlassRefractedVertex(&grid[(row + corner[vertex][1]) * GRID +
					column + corner[vertex][0]]);
		}
		GX_End();
	}
}

/* While Home rests, a band of light passes over the glass every
 * GLASS_SHEEN_PERIOD seconds: a plane moving through eye space from lower
 * left to upper right, so it rolls over the bevels the way a real light
 * crossing the cube would. front is the band's place along that path in
 * cube units from the centre; weight fades it in and out. Cells it does not
 * reach are skipped. */
#define GLASS_SHEEN_PERIOD 7.0f
#define GLASS_SHEEN_LENGTH 1.35f

static void drawGlassSheen(const cubeRasterTransform_t *raster,
		const cubeSurfaceQuad_t *quads, int count, int vertexCount, float outer,
		float front, float width, float weight)
{
	enum { FACE_STEPS = 8, ACROSS_STEPS = 6, ALONG_STEPS = 4, GRID = 9 };
	const guVector axis = {0.8908f, 0.4543f, 0.0f};
	guVector center = {raster->model[0][3], raster->model[1][3], raster->model[2][3]};

	for(int quad = 0; quad < count; quad++) {
		guVector eyes[4], normals[4], body[GRID * GRID];
		GXColor color[GRID * GRID];
		indigoPoint_t points[4];
		float area = 0.0f;
		int cells = 0, columns, rows;
		for(int vertex = 0; vertex < vertexCount; vertex++) {
			guVector p = quads[quad].point[vertex];
			if(!projectRailPoint(raster, p.x, p.y, p.z, &eyes[vertex], &points[vertex])) return;
			normals[vertex] = glassVertexNormal(raster, p, outer);
		}
		for(int vertex = 0; vertex < vertexCount; vertex++) {
			int next = (vertex + 1) % vertexCount;
			area += points[vertex].x * points[next].y - points[next].x * points[vertex].y;
		}
		if(area >= -0.001f || vertexCount != 4) continue;
		bool acrossU = !glassSameNormal(normals[0], normals[1]);
		bool acrossV = !glassSameNormal(normals[0], normals[3]);
		columns = acrossU ? ACROSS_STEPS : (acrossV ? ALONG_STEPS : FACE_STEPS);
		rows = acrossV ? ACROSS_STEPS : (acrossU ? ALONG_STEPS : FACE_STEPS);
		for(int row = 0; row <= rows; row++) for(int column = 0; column <= columns; column++) {
			float u = (float)column / columns, v = (float)row / rows;
			int at = row * GRID + column;
			guVector eye = glassBilinear(eyes, u, v), n = glassBilinear(normals, u, v);
			float length = sqrtf(eye.x * eye.x + eye.y * eye.y + eye.z * eye.z);
			float nLength = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
			float facing = -(n.x * eye.x + n.y * eye.y + n.z * eye.z) / (length * nLength);
			float d = ((eye.x - center.x) * axis.x + (eye.y - center.y) * axis.y) - front;
			float bump = 1.0f - (d * d) / (width * width);
			bump = bump <= 0.0f ? 0.0f : bump * bump;
			body[at] = glassBilinear(quads[quad].point, u, v);
			color[at] = (GXColor) {236, 230, 255,
				(u8)(255.0f * fminf(1.0f, 0.24f * weight * bump *
				glassSmoothstep(0.0f, 0.30f, facing)) + 0.5f)};
		}
		for(int row = 0; row < rows; row++) for(int column = 0; column < columns; column++)
			cells += glassCellLit(color, row * GRID + column, GRID);
		if(cells == 0) continue;
		GX_Begin(GX_QUADS, GX_VTXFMT0, (u16)(cells * 4));
		for(int row = 0; row < rows; row++) for(int column = 0; column < columns; column++) {
			static const int corner[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
			int first = row * GRID + column;
			if(!glassCellLit(color, first, GRID)) continue;
			for(int vertex = 0; vertex < 4; vertex++) {
				int at = first + corner[vertex][1] * GRID + corner[vertex][0];
				putCubeVertex(body[at].x, body[at].y, body[at].z, color[at]);
			}
		}
		GX_End();
	}
}

/* A soft round light in screen units: a centre and three rings whose alphas
 * follow a gaussian, so additive glows have no visible cone or edge. */
static void drawSoftGlow(float x, float y, float radiusX, float radiusY,
		GXColor color, float alpha)
{
	static const float ring[4] = {0.0f, 0.28f, 0.58f, 1.0f};
	static const float falloff[4] = {1.0f, 0.74f, 0.30f, 0.0f};
	const float step = INDIGO_TAU / RADIAL_SEGMENTS;
	const float stepCos = cosf(step), stepSin = sinf(step);

	if(alpha <= 0.004f || radiusX <= 0.0f || radiusY <= 0.0f) return;
	if(alpha > 1.0f) alpha = 1.0f;
	for(int band = 0; band < 3; band++) {
		float unitX = 1.0f, unitY = 0.0f;
		GXColor inner = color, outerColor = color;
		inner.a = (u8)(255.0f * alpha * falloff[band] + 0.5f);
		outerColor.a = (u8)(255.0f * alpha * falloff[band + 1] + 0.5f);
		GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, (RADIAL_SEGMENTS + 1) * 2);
		for(int i = 0; i <= RADIAL_SEGMENTS; i++) {
			putVertex((indigoPoint_t) {x + radiusX * ring[band] * unitX,
				y + radiusY * ring[band] * unitY}, inner);
			putVertex((indigoPoint_t) {x + radiusX * ring[band + 1] * unitX,
				y + radiusY * ring[band + 1] * unitY}, outerColor);
			float nextX = unitX * stepCos - unitY * stepSin;
			unitY = unitX * stepSin + unitY * stepCos;
			unitX = nextX;
		}
		GX_End();
	}
}

/* A thin tapered ray from a light's centre: bright at the root, gone at the
 * tip, a little wider in the middle than a line would be. */
static void drawLightRay(float x, float y, float angle, float length, float width,
		GXColor color, float alpha)
{
	float dx = cosf(angle), dy = sinf(angle);
	float px = -dy * width, py = dx * width;
	GXColor root = color, tip = color;

	if(alpha <= 0.004f) return;
	root.a = (u8)(255.0f * fminf(1.0f, alpha) + 0.5f);
	tip.a = 0;
	GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, 6);
		putVertex((indigoPoint_t) {x + px * 0.2f, y + py * 0.2f}, root);
		putVertex((indigoPoint_t) {x - px * 0.2f, y - py * 0.2f}, root);
		putVertex((indigoPoint_t) {x + dx * length * 0.30f + px, y + dy * length * 0.30f + py}, root);
		putVertex((indigoPoint_t) {x + dx * length * 0.30f - px, y + dy * length * 0.30f - py}, root);
		putVertex((indigoPoint_t) {x + dx * length, y + dy * length}, tip);
		putVertex((indigoPoint_t) {x + dx * length, y + dy * length}, tip);
	GX_End();
}

/* Light on the glass's outline: a fine rim that is brightest on the edges
 * turned toward the key light and gone on the far ones, as glass edges
 * catch light by Fresnel reflection. Just outside it a warm fringe and just
 * inside a cool one part the rim the way dispersion parts a bright edge.
 * The outline is the convex hull buildCubeOutline gives (screen units
 * about the centre, y up). */
static void drawGlassRim(const cubeOutline_t *outline, float strength)
{
	static const struct {
		float offset;
		float alpha;
		GXColor color;
	} layers[3] = {
		{0.0f, 0.62f, {236, 230, 255, 255}},
		{1.35f, 0.20f, {255, 184, 150, 255}},
		{-1.35f, 0.22f, {90, 224, 246, 255}}
	};
	static const float profile[4] = {-1.0f, -0.25f, 0.25f, 1.0f};
	const float lightX = 0.68f, lightY = -0.73f;
	indigoPoint_t points[25], joins[25];
	GXColor colors[25];
	int count = outline->count;

	if(count < 3 || count > 24 || strength <= 0.01f) return;
	for(int i = 0; i < count; i++) {
		points[i] = (indigoPoint_t) {320.0f + outline->point[i].x, 240.0f - outline->point[i].y};
	}
	if(!buildRasterJoins(points, joins, count, true)) return;
	points[count] = points[0];
	joins[count] = joins[0];
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
	for(int layer = 0; layer < 3; layer++) {
		indigoPoint_t shifted[25];
		for(int i = 0; i <= count; i++) {
			/* The joins point along the outline's outward normal. */
			float length = sqrtf(joins[i].x * joins[i].x + joins[i].y * joins[i].y);
			float nx = length > 0.0f ? joins[i].x / length : 0.0f;
			float ny = length > 0.0f ? joins[i].y / length : 0.0f;
			float lit = nx * lightX + ny * lightY;
			lit = lit <= 0.0f ? 0.0f : lit * lit;
			colors[i] = layers[layer].color;
			colors[i].a = (u8)(255.0f * fminf(1.0f, layers[layer].alpha * lit * strength) + 0.5f);
			shifted[i] = (indigoPoint_t) {points[i].x + nx * layers[layer].offset,
				points[i].y + ny * layers[layer].offset};
		}
		drawRasterStroke(shifted, joins, colors, count + 1, profile, 3);
	}
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
}

/* The sun caught in the glass: a hot white core where the bevel mirrors the
 * key light, a lilac bloom round it, a long anamorphic streak, six fine
 * rays that turn with the cube, and faint ghosts strung through the middle
 * of the screen the way a lens repeats a bright light. Nothing shows until
 * the glass actually turns the light toward the camera. */
static void drawSunFlare(const glassSun_t *sun, float strength, float spin,
		float scale)
{
	static const GXColor core = {255, 247, 236, 255};
	static const GXColor bloom = {204, 190, 255, 255};
	static const GXColor streak = {186, 196, 255, 255};
	static const struct {
		float along;
		float radius;
		float alpha;
		GXColor color;
	} ghosts[3] = {
		{1.28f, 11.0f, 0.10f, {164, 138, 255, 255}},
		{1.62f, 24.0f, 0.055f, {134, 120, 236, 255}},
		{2.05f, 8.0f, 0.085f, {214, 204, 255, 255}}
	};
	float intensity;
	float x, y;

	if(sun->weight <= 0.0f || sun->peak <= 0.01f || strength <= 0.0f) return;
	intensity = glassSmoothstep(0.0f, 1.0f, sun->peak) * strength;
	x = sun->x / sun->weight;
	y = sun->y / sun->weight;
	if(!isfinite(x) || !isfinite(y)) return;
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
	drawSoftGlow(x, y, 54.0f * scale, 54.0f * scale, bloom, 0.30f * intensity);
	drawSoftGlow(x, y, 176.0f * scale, 3.0f * scale, streak, 0.36f * intensity);
	drawSoftGlow(x, y, 112.0f * scale, 8.0f * scale, streak, 0.12f * intensity);
	for(int ray = 0; ray < 6; ray++) {
		float angle = spin + INDIGO_TAU * (float)ray / 6.0f;
		float length = (ray & 1 ? 46.0f : 78.0f) * scale;
		drawLightRay(x, y, angle, length, 1.5f * scale, bloom, 0.34f * intensity);
	}
	drawSoftGlow(x, y, 15.0f * scale, 15.0f * scale, core, 0.95f * intensity);
	for(int ghost = 0; ghost < 3; ghost++) {
		float gx = x + (320.0f - x) * ghosts[ghost].along;
		float gy = y + (240.0f - y) * ghosts[ghost].along;
		drawSoftGlow(gx, gy, ghosts[ghost].radius * scale, ghosts[ghost].radius * scale,
			ghosts[ghost].color, ghosts[ghost].alpha * intensity);
	}
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
}

/* Bloom: the finished cube copied again, blurred by five taps, cut to what
 * is brighter than the glass itself, and screened back over it. Highlights,
 * glints and the icons' strokes glow; the tinted body does not. Screening
 * (the frame gains glow * (1 - frame)) rather than adding keeps a face that
 * turns through the key light's reflection, bright across its whole width,
 * from burning to white and hiding its icon for a few frames. */
static void drawGlassBloom(float left, float top, float right, float bottom,
		float strength)
{
	static const float taps[GLASS_BLOOM_TAPS][2] = {
		{0.0f, 0.0f}, {-3.0f, -2.0f}, {3.0f, -2.0f}, {-3.0f, 2.0f}, {3.0f, 2.0f}
	};
	GXColor glow = {220, 208, 255, 255};
	float sScale, tScale;

	if(strength <= 0.01f || !glassCopyFrame()) return;
	sScale = glassCopyS();
	tScale = glassCopyT();
	left = fmaxf(0.0f, left); top = fmaxf(0.0f, top);
	right = fminf(640.0f, right); bottom = fminf(480.0f, bottom);
	if(right - left < 2.0f || bottom - top < 2.0f) return;
	setupRasterPipeline();
	GX_SetNumTexGens(GLASS_BLOOM_TAPS);
	for(int tap = 0; tap < GLASS_BLOOM_TAPS; tap++) {
		Mtx offset;
		guMtxIdentity(offset);
		/* Offsets in copy texels, about two screen pixels each. */
		offset[0][3] = taps[tap][0] / (float)glassCopyWidth;
		offset[1][3] = taps[tap][1] / (float)glassCopyHeight;
		GX_LoadTexMtxImm(offset, GX_TEXMTX0 + tap * 3u, GX_MTX2x4);
		GX_SetTexCoordGen((u16)(GX_TEXCOORD0 + tap), GX_TG_MTX2x4, GX_TG_TEX0,
			GX_TEXMTX0 + tap * 3u);
	}
	/* K0 weighs the centre tap, K1 each of the four around it, K2 is the
	 * brightness the glass body already reaches, which does not glow. It is
	 * the glass's own hue, so a lilac highlight blooms lilac rather than
	 * pink, and it turns with Menu Color like everything else. */
	GXColor threshold = {150, 136, 196, 255};

	UIColor_Apply(&threshold.r, &threshold.g, &threshold.b);
	GX_SetTevKColor(GX_KCOLOR0, (GXColor) {92, 92, 92, 255});
	GX_SetTevKColor(GX_KCOLOR1, (GXColor) {41, 41, 41, 255});
	GX_SetTevKColor(GX_KCOLOR2, threshold);
	GX_SetNumTevStages(GLASS_BLOOM_TAPS + 2);
	for(int tap = 0; tap < GLASS_BLOOM_TAPS; tap++) {
		u8 stage = (u8)(GX_TEVSTAGE0 + tap);
		GX_SetTevOrder(stage, (u8)(GX_TEXCOORD0 + tap), GX_TEXMAP0, GX_COLOR0A0);
		GX_SetTevKColorSel(stage, tap == 0 ? GX_TEV_KCSEL_K0 : GX_TEV_KCSEL_K1);
		GX_SetTevColorIn(stage, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST,
			tap == 0 ? GX_CC_ZERO : GX_CC_CPREV);
		GX_SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
		GX_SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
		GX_SetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	}
	GX_SetTevOrder(GX_TEVSTAGE5, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevKColorSel(GX_TEVSTAGE5, GX_TEV_KCSEL_K2);
	GX_SetTevColorIn(GX_TEVSTAGE5, GX_CC_ZERO, GX_CC_ONE, GX_CC_KONST, GX_CC_CPREV);
	GX_SetTevColorOp(GX_TEVSTAGE5, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_4, GX_ENABLE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE5, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE5, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	GX_SetTevOrder(GX_TEVSTAGE6, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevColorIn(GX_TEVSTAGE6, GX_CC_ZERO, GX_CC_CPREV, GX_CC_RASC, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE6, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2, GX_ENABLE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE6, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE6, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	/* A screen has no source alpha, so the strength rides in the colour, and
	 * the stage doubles it back: screening gives the bright strokes less
	 * than adding did, and at rest the glow should read as before. */
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_INVDSTCLR, GX_BL_ONE, GX_LO_CLEAR);
	UIColor_Apply(&glow.r, &glow.g, &glow.b);
	strength = 0.8f * fminf(1.0f, strength);
	glow.r = (u8)(glow.r * strength + 0.5f);
	glow.g = (u8)(glow.g * strength + 0.5f);
	glow.b = (u8)(glow.b * strength + 0.5f);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32(left, top, 0.0f);
		GX_Color4u8(glow.r, glow.g, glow.b, glow.a);
		GX_TexCoord2f32(left * sScale, top * tScale);
		GX_Position3f32(right, top, 0.0f);
		GX_Color4u8(glow.r, glow.g, glow.b, glow.a);
		GX_TexCoord2f32(right * sScale, top * tScale);
		GX_Position3f32(right, bottom, 0.0f);
		GX_Color4u8(glow.r, glow.g, glow.b, glow.a);
		GX_TexCoord2f32(right * sScale, bottom * tScale);
		GX_Position3f32(left, bottom, 0.0f);
		GX_Color4u8(glow.r, glow.g, glow.b, glow.a);
		GX_TexCoord2f32(left * sScale, bottom * tScale);
	GX_End();
	setupRasterPipeline();
}

static void drawCube(const uiSceneFrame_t *scene, float seconds, bool animated,
		const uiClockFrame_t *clock, const indigoPadFrame_t *pad,
		const int icons[UI_HOME_FACE_COUNT], bool screenGlass)
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
	static const GXColor bevelColors[6] = {
		{25, 18, 70, 88}, {132, 108, 218, 98}, {196, 180, 255, 134},
		{65, 48, 143, 84}, {235, 228, 255, 154}, {132, 108, 218, 98}
	};
	GXColor lit[6];
	cubeRasterTransform_t raster;
	cubeSurfaceQuad_t core[6], shell[6], strips[12], corners[8];
	cubeOutline_t coreOutline, shellOutline;
	glassSun_t sun = {0.0f, 0.0f, 0.0f, 0.0f};
	const float outer = 1.0f;
	const float inset = 0.78f;
	float strength = glassSceneStrength(scene);
	bool refract;

	setupCubePipeline(scene, seconds, animated, &raster);
	litCubeTints(&raster, coreColors, lit);
	buildCubeFaces(core, 0.46f, 0.46f, lit);
	litCubeTints(&raster, backGlassColors, lit);
	buildCubeFaces(shell, outer, inset, lit);
	litCubeTints(&raster, bevelColors, lit);
	buildChamferStrips(strips, outer, inset, lit);
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

	/* Every semantic lateral face receives the same restrained inset pane,
	 * in one tint so every face's icon reads in the same shade; no
	 * destination becomes a blank reverse side of the Library artwork. */
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_FALSE);
	GX_SetCullMode(GX_CULL_BACK);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 16);
	for(int face = 0; face < UI_HOME_FACE_COUNT; face++) {
		putSemanticFaceRect(&raster, face, -0.72f, -0.72f,
			0.72f, 0.72f, 0.86f, (GXColor) {57, 42, 122, 58});
	}
	GX_End();

	/* Convex glass is rendered back-to-front with depth writes disabled. */
	GX_SetCullMode(GX_CULL_FRONT);
	drawCubeSurfacePass(&raster, &shellOutline, shell, 6, GX_CULL_FRONT, false);
	/* Everything behind the front glass so far (backdrop, halo, core, far
	 * bevels, the inset panes and the back glass) is copied and bent through
	 * the camera-facing outer glass, as a solid glass block shows its own far
	 * edges: magnified across the faces, wrapped round the bevels and parted
	 * into colour where the bend is strongest. The front tint, near bevels,
	 * reflections and icons then sit on the glass, sharp. Without a copy the
	 * walk still finds the sun glint. */
	refract = screenGlass && strength > 0.01f && glassCopyFrame();
	{
		float radius = raster.scaleY * scene->cubeScale / -CUBE_CAMERA_Z;
		glassRefraction_t glass = {
			320.0f + raster.model[0][3] * raster.scaleX / -raster.model[2][3],
			240.0f - raster.model[1][3] * raster.scaleY / -raster.model[2][3],
			0.46f * radius, 0.05f, 0.24f,
			refract ? glassCopyS() : 0.0f, refract ? glassCopyT() : 0.0f,
			{128, 125, 134, 255}
		};
		if(refract) {
			setupGlassRefractionPipeline();
			GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
			GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_FALSE);
			GX_SetCullMode(GX_CULL_BACK);
		}
		drawGlassRefraction(&raster, &glass, shell, 6, 4, outer, &sun, refract);
		drawGlassRefraction(&raster, &glass, strips, 12, 4, outer, &sun, refract);
		drawGlassRefraction(&raster, &glass, corners, 8, 3, outer, &sun, refract);
		if(refract) restoreCubeRaster();
	}

	GX_SetCullMode(GX_CULL_BACK);
	litCubeTints(&raster, frontGlassColors, lit);
	buildCubeFaces(shell, outer, inset, lit);
	drawCubeSurfacePass(&raster, &shellOutline, shell, 6, GX_CULL_BACK, false);

	/* Only near structural surfaces composite in front of the shell. Corner
	 * triangles close the chamfer and share its camera-facing depth policy. */
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	drawCubeSurfacePass(&raster, &shellOutline, strips, 12, GX_CULL_BACK, false);
	drawCubeSurfacePassVertices(&raster, &shellOutline, corners, 8, 3, GX_CULL_BACK, false);

	/* The outer glass reflects light over everything the shell shows,
	 * additively and without depth writes. */
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
	GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_FALSE);
	GX_SetCullMode(GX_CULL_BACK);
	drawGlassReflection(&raster, shell, 6, 4, outer);
	drawGlassReflection(&raster, strips, 12, 4, outer);
	drawGlassReflection(&raster, corners, 8, 3, outer);
	if(animated && scene->homeIdleBlend > 0.01f && strength > 0.01f) {
		float phase = fmodf(seconds, GLASS_SHEEN_PERIOD);
		if(phase < GLASS_SHEEN_LENGTH) {
			float progress = glassSmoothstep(0.0f, GLASS_SHEEN_LENGTH, phase);
			float reach = 1.9f * scene->cubeScale;
			float weight = sinf(progress * INDIGO_TAU * 0.5f) * scene->homeIdleBlend;
			drawGlassSheen(&raster, shell, 6, 4, outer, -reach + 2.0f * reach * progress,
				0.42f * scene->cubeScale, weight);
			drawGlassSheen(&raster, strips, 12, 4, outer, -reach + 2.0f * reach * progress,
				0.42f * scene->cubeScale, weight);
		}
	}
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	drawFaceIcons(seconds, animated, clock, pad, icons, &raster);

	/* Light leaving the finished glass: its bloom, then the sun it catches. */
	if(strength > 0.01f && shellOutline.count >= 3) {
		float left = 640.0f, top = 480.0f, right = 0.0f, bottom = 0.0f;
		for(int i = 0; i < shellOutline.count; i++) {
			float x = 320.0f + shellOutline.point[i].x;
			float y = 240.0f - shellOutline.point[i].y;
			left = fminf(left, x); right = fmaxf(right, x);
			top = fminf(top, y); bottom = fmaxf(bottom, y);
		}
		if(screenGlass) {
			drawGlassBloom(left - 28.0f, top - 28.0f, right + 28.0f, bottom + 28.0f,
				0.78f * strength);
		}
		setupRasterPipeline();
		drawGlassRim(&shellOutline, strength);
		drawSunFlare(&sun, strength, (animated ? seconds * 0.05f : 0.0f) +
			scene->cubeYaw * 0.5f, scene->cubeScale / 0.92f);
	}
}

/* The light around the cube, drawn behind it: a wide halo, as if the key
 * light stood behind the glass, and a caustic where the glass focuses that
 * light on the floor inside its shadow. The caustic swells as a face turns
 * square to the light and the lens it makes is strongest. */
static void drawCubeLight(const uiSceneFrame_t *scene, float seconds, bool animated,
		float floorX, float floorY, float floorScale)
{
	const float perUnit = 1.0f / tanf(21.0f * INDIGO_TAU / 360.0f) * 240.0f /
		-CUBE_CAMERA_Z;
	float strength = glassSceneStrength(scene);
	float bob = animated ? sinf(seconds * 0.62f) * 0.035f : 0.0f;
	float x = 320.0f + scene->cubeX * perUnit;
	float y = 240.0f - (scene->cubeY + bob) * perUnit;
	float radius = scene->cubeScale * perUnit;
	float breath = animated ? 0.92f + 0.08f * sinf(seconds * 0.9f) : 1.0f;
	float focus = 0.55f + 0.45f * fabsf(cosf(2.0f * scene->cubeYaw));

	if(strength <= 0.01f) return;
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
	drawSoftGlow(x + radius * 0.16f, y - radius * 0.10f, radius * 2.35f, radius * 2.10f,
		(GXColor) {138, 116, 255, 255}, 0.14f * strength * breath);
	drawSoftGlow(x, y, radius * 1.42f, radius * 1.36f,
		(GXColor) {172, 150, 255, 255}, 0.09f * strength * breath);
	drawSoftGlow(floorX - 14.0f * floorScale, floorY, 74.0f * floorScale, 9.5f * floorScale,
		(GXColor) {214, 202, 255, 255}, 0.20f * strength * focus);
	drawSoftGlow(floorX - 20.0f * floorScale, floorY - 1.0f, 26.0f * floorScale,
		4.0f * floorScale, (GXColor) {244, 238, 255, 255}, 0.24f * strength * focus);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
}

void IndigoBackground_Draw(float seconds, bool backdropAnimated,
	bool cubeAnimated, const uiSceneFrame_t *scene,
	const uiClockFrame_t *clock, const indigoPadFrame_t *pad,
	const int icons[UI_HOME_FACE_COUNT])
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
		drawCubeLight(scene, seconds, cubeMotionActive, centerX,
			centerY + 132.0f * orbitScale, orbitScale);
		drawCube(scene, seconds, cubeMotionActive, clock, pad, icons, true);
	}
}

void IndigoBackground_DrawBootOverlay(float seconds, bool animated,
	const uiSceneFrame_t *scene, const uiClockFrame_t *clock,
	const int icons[UI_HOME_FACE_COUNT])
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
		drawCube(scene, seconds, animated, clock, NULL, icons, false);
	}
	setupRasterPipeline();
	drawBootVeil(veilAlpha);
}
