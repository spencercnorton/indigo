/* Independent mapping oracle and visibility-state tests for cube-face glyphs. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_cube_motif.h"

static unsigned long checks;
#define CHECK(c) do { ++checks; if(!(c)) { \
	fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#c); exit(1); \
} } while(0)
static const uiHomeCapabilities_t caps = {true, true, false};
/* With an app on the source: a ring of five faces. */
static const uiHomeCapabilities_t appsCaps = {true, true, true};

static bool placed_alike(const uiCubeMotifBasis_t *a, const uiCubeMotifBasis_t *b, int f)
{
	for(int r=0;r<3;++r) for(int c=0;c<3;++c)
		if(a->face[f][r][c] != b->face[f][r][c]) return false;
	return true;
}

static bool same(const uiCubeMotifBasis_t *a, const uiCubeMotifBasis_t *b)
{
	for(int f=0; f<UI_HOME_FACE_COUNT; ++f)
		if(!placed_alike(a,b,f) || a->shown[f] != b->shown[f]) return false;
	return true;
}

static int determinant(const uiHomeOrientation_t *a)
{
	return a->m[0][0]*(a->m[1][1]*a->m[2][2]-a->m[1][2]*a->m[2][1])
		-a->m[0][1]*(a->m[1][0]*a->m[2][2]-a->m[1][2]*a->m[2][0])
		+a->m[0][2]*(a->m[1][0]*a->m[2][1]-a->m[1][1]*a->m[2][0]);
}

/* Enumerate all proper cube orientations without using the runtime turn helper. */
static size_t orientations(uiHomeOrientation_t out[24])
{
	size_t n=0u;
	for(int x=0;x<3;++x) for(int y=0;y<3;++y) for(int z=0;z<3;++z) {
		if(x==y || x==z || y==z) continue;
		for(unsigned signs=0u;signs<8u;++signs) {
			uiHomeOrientation_t m={{{0}}};
			m.m[0][x]=(int8_t)((signs&1u) ? -1:1);
			m.m[1][y]=(int8_t)((signs&2u) ? -1:1);
			m.m[2][z]=(int8_t)((signs&4u) ? -1:1);
			if(determinant(&m)==1) { CHECK(n<24u); out[n++]=m; }
		}
	}
	CHECK(n==24u);
	return n;
}

static void proper_basis(const uiCubeMotifBasis_t *basis)
{
	for(int f=0;f<UI_HOME_FACE_COUNT;++f) {
		uiHomeOrientation_t m;
		for(int r=0;r<3;++r) for(int c=0;c<3;++c) {
			float v=basis->face[f][r][c];
			CHECK(v==-1.0f || v==0.0f || v==1.0f);
			m.m[r][c]=(int8_t)v;
		}
		CHECK(determinant(&m)==1);
		for(int a=0;a<3;++a) for(int b=0;b<3;++b) {
			int dot=0;
			for(int r=0;r<3;++r) dot+=m.m[r][a]*m.m[r][b];
			CHECK(dot==(a==b ? 1:0));
		}
	}
}

static void all_orientations_and_semantic_faces(void)
{
	/* Product-facing oracle: next is right horizontally, bottom vertically.
	 * Up vectors specify upright text on the corresponding unfolded ring. */
	static const int normalH[4][3]={{0,0,1},{1,0,0},{0,0,-1},{-1,0,0}};
	static const int normalV[4][3]={{0,0,1},{0,-1,0},{0,0,-1},{0,1,0}};
	static const int upV[4][3]={{0,1,0},{0,0,1},{0,-1,0},{0,0,-1}};
	uiHomeOrientation_t all[24];
	size_t count=orientations(all);
	for(size_t o=0u;o<count;++o) for(int f=0;f<4;++f) for(int a=0;a<3;++a) {
		uiHomeState_t home;
		uiCubeMotifBasis_t map;
		UIHome_Init(&home,caps); home.orientation=all[o]; home.face=(uiHomeFace_t)f;
		home.turnAxis=(uiHomeTurnAxis_t)a;
		UICubeMotif_Build(&home,&map); proper_basis(&map);
		for(int glyph=0;glyph<4;++glyph) {
			int relative=(glyph-f+4)%4;
			int n[3],u[3],right[3];
			for(int r=0;r<3;++r) {
				n[r]=a==UI_HOME_TURN_VERTICAL ? normalV[relative][r]:normalH[relative][r];
				u[r]=a==UI_HOME_TURN_VERTICAL ? upV[relative][r]:(r==1 ? 1:0);
			}
			right[0]=u[1]*n[2]-u[2]*n[1];
			right[1]=u[2]*n[0]-u[0]*n[2];
			right[2]=u[0]*n[1]-u[1]*n[0];
			for(int r=0;r<3;++r) for(int c=0;c<3;++c) {
				float world=0.0f;
				for(int k=0;k<3;++k) world+=(float)home.orientation.m[r][k]*map.face[glyph][k][c];
				CHECK(world==(float)(c==0 ? right[r]:(c==1 ? u[r]:n[r])));
			}
		}
	}
}

static void update_without_visible_basis_swap(uiCubeMotifState_t *state,float dt,uiMotionMode_t mode);

/* The side of the band a face's glyph is on, written from the contract: the
 * face in front, the next one (right, or below turning vertically), the
 * previous one (left, or above), and the rest behind; in a ring of five the
 * face two ahead has no side and is not shown, nor is Apps outside a ring. */
static int band_side(int glyph, int face, int ring, bool *shown)
{
	int relative;

	*shown=glyph<ring;
	if(glyph>=ring) return 2;
	relative=(glyph-face+ring)%ring;
	if(relative==0) return 0;
	if(relative==1) return 1;
	if(relative==ring-1) return 3;
	if(ring==5 && relative==2) *shown=false;
	return 2;
}

static void five_face_ring_glyphs(void)
{
	static const int normalH[4][3]={{0,0,1},{1,0,0},{0,0,-1},{-1,0,0}};
	static const int normalV[4][3]={{0,0,1},{0,-1,0},{0,0,-1},{0,1,0}};
	uiHomeOrientation_t all[24];
	size_t count=orientations(all);
	for(int ring=4;ring<=5;++ring)
	for(size_t o=0u;o<count;++o) for(int f=0;f<ring;++f) for(int a=0;a<3;++a) {
		uiHomeState_t home;
		uiCubeMotifBasis_t map;
		UIHome_Init(&home,ring==5 ? appsCaps:caps); home.orientation=all[o];
		home.face=(uiHomeFace_t)f; home.turnOrdinal=(int32_t)f;
		home.turnAxis=(uiHomeTurnAxis_t)a;
		CHECK(home.faceCount==ring);
		UICubeMotif_Build(&home,&map); proper_basis(&map);
		CHECK(map.faceCount==ring);
		for(int glyph=0;glyph<UI_HOME_FACE_COUNT;++glyph) {
			bool shown;
			int side=band_side(glyph,f,ring,&shown);
			CHECK(map.shown[glyph]==shown);
			if(!shown) {
				/* One fixed place behind the authored front: it never
				 * moves, so it never starts a fade. */
				static const float behind[3][3]={{-1,0,0},{0,1,0},{0,0,-1}};
				for(int r=0;r<3;++r) for(int c=0;c<3;++c)
					CHECK(map.face[glyph][r][c]==behind[r][c]);
				continue;
			}
			for(int r=0;r<3;++r) {
				float world=0.0f;
				int n=a==UI_HOME_TURN_VERTICAL ? normalV[side][r]:normalH[side][r];
				for(int k=0;k<3;++k) world+=(float)home.orientation.m[r][k]*map.face[glyph][k][2];
				CHECK(world==(float)n);
			}
		}
	}
	/* Round the ring of five: after every turn the face in front has its
	 * glyph in front, the one before it on the side it came from. */
	for(int a=UI_HOME_TURN_HORIZONTAL;a<=UI_HOME_TURN_VERTICAL;++a)
		for(int direction=-1;direction<=1;direction+=2) {
		uiHomeState_t home;
		UIHome_Init(&home,appsCaps);
		for(int step=0;step<11;++step) {
			uiCubeMotifBasis_t map;
			uiHomeInput_t input=a==UI_HOME_TURN_HORIZONTAL ?
				(direction<0 ? UI_HOME_INPUT_LEFT:UI_HOME_INPUT_RIGHT):
				(direction<0 ? UI_HOME_INPUT_UP:UI_HOME_INPUT_DOWN);
			CHECK(UIHome_Apply(&home,input,appsCaps)==UI_HOME_EFFECT_NONE);
			UICubeMotif_Build(&home,&map);
			for(int glyph=0;glyph<UI_HOME_FACE_COUNT;++glyph) {
				float front=0.0f;
				for(int k=0;k<3;++k) front+=(float)home.orientation.m[2][k]*map.face[glyph][k][2];
				if(map.shown[glyph]) CHECK((front==1.0f)==(glyph==(int)home.face));
			}
		}
	}
	/* The point of that placement: turning either way, the glyphs in front
	 * and on the previous side keep their places on the cube (they only
	 * turn with it), and a right or down turn keeps the next side's too, so
	 * no glyph the camera can see ever fades. What moves is behind. */
	{
		uiHomeOrientation_t all[24];
		size_t count=orientations(all);
		for(size_t o=0u;o<count;++o) for(int f=0;f<5;++f)
		for(int a=UI_HOME_TURN_HORIZONTAL;a<=UI_HOME_TURN_VERTICAL;++a)
		for(int direction=-1;direction<=1;direction+=2) {
			uiHomeState_t home;
			uiCubeMotifBasis_t before,after;
			UIHome_Init(&home,appsCaps); home.orientation=all[o];
			home.face=(uiHomeFace_t)f; home.turnOrdinal=(int32_t)f;
			home.turnAxis=(uiHomeTurnAxis_t)a; home.turnDirection=direction;
			UICubeMotif_Build(&home,&before);
			(void)UIHome_Apply(&home,a==UI_HOME_TURN_HORIZONTAL ?
				(direction<0 ? UI_HOME_INPUT_LEFT:UI_HOME_INPUT_RIGHT):
				(direction<0 ? UI_HOME_INPUT_UP:UI_HOME_INPUT_DOWN),appsCaps);
			UICubeMotif_Build(&home,&after);
			CHECK(placed_alike(&before,&after,f));
			CHECK(placed_alike(&before,&after,(f+4)%5));
			if(direction>0) CHECK(placed_alike(&before,&after,(f+1)%5));
		}
	}
	/* Apps coming: its glyph fades in on the previous side while the
	 * others stay lit; going, it fades out and Apps is not shown. */
	{
		uiHomeState_t home;
		uiCubeMotifState_t state;
		UIHome_Init(&home,caps); UICubeMotif_Reset(&state);
		UICubeMotif_Request(&state,&home,UI_MOTION_FULL);
		for(int i=0;i<5;++i) update_without_visible_basis_swap(&state,0.02f,UI_MOTION_FULL);
		CHECK(UICubeMotif_Settled(&state) && state.alpha[UI_HOME_FACE_APPS]==0.0f);
		(void)UIHome_Apply(&home,UI_HOME_INPUT_NONE,appsCaps);
		UICubeMotif_Request(&state,&home,UI_MOTION_FULL);
		CHECK(state.changing && state.pending.faceCount==5);
		update_without_visible_basis_swap(&state,0.02f,UI_MOTION_FULL);
		CHECK(state.alpha[UI_HOME_FACE_APPS]>0.0f && state.alpha[UI_HOME_FACE_APPS]<1.0f);
		CHECK(state.alpha[UI_HOME_FACE_LIBRARY]==1.0f && state.alpha[UI_HOME_FACE_SOURCE]==1.0f);
		for(int i=0;i<10;++i) update_without_visible_basis_swap(&state,0.02f,UI_MOTION_FULL);
		CHECK(UICubeMotif_Settled(&state) && state.alpha[UI_HOME_FACE_APPS]==1.0f);
		/* Library in front of five: Settings, two ahead, has no side. */
		CHECK(state.alpha[UI_HOME_FACE_SETTINGS]==0.0f && !state.basis.shown[UI_HOME_FACE_SETTINGS]);
		(void)UIHome_Apply(&home,UI_HOME_INPUT_NONE,caps);
		UICubeMotif_Request(&state,&home,UI_MOTION_FULL);
		for(int i=0;i<10;++i) update_without_visible_basis_swap(&state,0.02f,UI_MOTION_FULL);
		CHECK(UICubeMotif_Settled(&state) && state.alpha[UI_HOME_FACE_APPS]==0.0f);
		CHECK(state.alpha[UI_HOME_FACE_SETTINGS]==1.0f);
	}
}

static void same_axis_four_turns_leave_physical_glyphs_unchanged(void)
{
	uiHomeOrientation_t all[24];
	size_t count=orientations(all);
	for(size_t o=0u;o<count;++o) for(int f=0;f<4;++f)
		for(int a=UI_HOME_TURN_HORIZONTAL;a<=UI_HOME_TURN_VERTICAL;++a)
			for(int direction=-1;direction<=1;direction+=2) {
		uiHomeState_t home;
		uiCubeMotifBasis_t initial,next;
		UIHome_Init(&home,caps); home.orientation=all[o]; home.face=(uiHomeFace_t)f;
		home.turnOrdinal=(int32_t)f; home.turnAxis=(uiHomeTurnAxis_t)a;
		UICubeMotif_Build(&home,&initial);
		for(int step=0;step<4;++step) {
			uiHomeInput_t input=a==UI_HOME_TURN_HORIZONTAL ?
				(direction<0 ? UI_HOME_INPUT_LEFT:UI_HOME_INPUT_RIGHT):
				(direction<0 ? UI_HOME_INPUT_UP:UI_HOME_INPUT_DOWN);
			CHECK(UIHome_Apply(&home,input,caps)==UI_HOME_EFFECT_NONE);
			UICubeMotif_Build(&home,&next); CHECK(same(&initial,&next));
		}
		CHECK(home.face==(uiHomeFace_t)f);
		CHECK(memcmp(&home.orientation,&all[o],sizeof(all[o]))==0);
	}
}

static void update_without_visible_basis_swap(uiCubeMotifState_t *state,float dt,uiMotionMode_t mode)
{
	uiCubeMotifState_t before=*state;
	UICubeMotif_Update(state,dt,mode);
	proper_basis(&state->basis);
	for(int f=0;f<UI_HOME_FACE_COUNT;++f) {
		float a=state->alpha[f];
		CHECK(isfinite(a) && a>=0.0f && a<=1.0f);
		/* A face with no side only fades out. */
		if(!state->basis.shown[f]) CHECK(a<=before.alpha[f]);
		if(!placed_alike(&before.basis,&state->basis,f)) {
			/* A glyph may move inside a frame only after reaching zero
			 * opacity. The frame's remaining time belongs to its fade-in. */
			float left=dt-before.alpha[f]*0.070f;
			float expected=state->basis.shown[f] ? fminf(1.0f,left/0.100f):0.0f;
			CHECK(before.changing);
			CHECK(before.alpha[f]*0.070f<=dt+0.000001f);
			CHECK(placed_alike(&state->basis,&before.pending,f));
			CHECK(fabsf(a-expected)<0.00001f);
		}
	}
}

static void mixed_halfway_retarget_and_latest_pending(void)
{
	uiHomeState_t home;
	uiCubeMotifState_t state;
	uiCubeMotifBasis_t original,wanted;
	UIHome_Init(&home,caps); UICubeMotif_Reset(&state); original=state.basis;
	(void)UIHome_Apply(&home,UI_HOME_INPUT_RIGHT,caps);
	UICubeMotif_Request(&state,&home,UI_MOTION_FULL);
	CHECK(!state.changing && same(&original,&state.basis));
	/* At the body's halfway yaw, Up remaps the visible outgoing Library to
	 * the top physical face. Keeping the old map avoids that exact teleport. */
	(void)UIHome_Apply(&home,UI_HOME_INPUT_UP,caps);
	UICubeMotif_Build(&home,&wanted);
	CHECK(!same(&wanted,&original));
	CHECK(wanted.face[UI_HOME_FACE_LIBRARY][2][2]!=original.face[UI_HOME_FACE_LIBRARY][2][2]);
	UICubeMotif_Request(&state,&home,UI_MOTION_FULL);
	CHECK(state.changing && state.alpha[UI_HOME_FACE_LIBRARY]==1.0f &&
		same(&state.basis,&original));
	update_without_visible_basis_swap(&state,0.025f,UI_MOTION_FULL);
	CHECK(same(&state.basis,&original));
	CHECK(state.alpha[UI_HOME_FACE_LIBRARY]>0.0f &&
		state.alpha[UI_HOME_FACE_LIBRARY]<1.0f);
	/* Only the glyphs that move fade; one keeping its side stays lit. */
	for(int f=0;f<UI_HOME_FACE_COUNT;++f)
		if(placed_alike(&original,&wanted,f) && wanted.shown[f])
			CHECK(state.alpha[f]==1.0f);
	/* Repeated requests replace one pending map without restarting alpha. */
	for(int i=0;i<10000;++i) {
		uiCubeMotifBasis_t drawn=state.basis;
		float alpha[UI_HOME_FACE_COUNT];
		uiHomeInput_t input=i%3==0 ? UI_HOME_INPUT_DOWN:
			(i%3==1 ? UI_HOME_INPUT_RIGHT:UI_HOME_INPUT_UP);
		memcpy(alpha,state.alpha,sizeof(alpha));
		(void)UIHome_Apply(&home,input,caps);
		UICubeMotif_Request(&state,&home,UI_MOTION_FULL);
		UICubeMotif_Build(&home,&wanted);
		CHECK(same(&state.pending,&wanted));
		CHECK(same(&state.basis,&drawn) && memcmp(alpha,state.alpha,sizeof(alpha))==0);
		update_without_visible_basis_swap(&state,0.001f,UI_MOTION_FULL);
	}
	for(int i=0;i<30;++i) update_without_visible_basis_swap(&state,0.01f,UI_MOTION_FULL);
	CHECK(same(&state.basis,&wanted) && UICubeMotif_Settled(&state) && !state.changing);
	CHECK(sizeof(state)<=512u);
	/* Returning to the currently displayed map cancels an obsolete swap. */
	UICubeMotif_Reset(&state); UIHome_Init(&home,caps);
	(void)UIHome_Apply(&home,UI_HOME_INPUT_UP,caps);
	UICubeMotif_Request(&state,&home,UI_MOTION_FULL);
	update_without_visible_basis_swap(&state,0.03f,UI_MOTION_FULL);
	UICubeMotif_Request(&state,NULL,UI_MOTION_FULL);
	CHECK(!state.changing && same(&state.basis,&original));
	for(int i=0;i<10;++i) update_without_visible_basis_swap(&state,0.01f,UI_MOTION_FULL);
	CHECK(UICubeMotif_Settled(&state) && same(&state.basis,&original));
}

static void frame_rate_and_motion_modes(void)
{
	uiHomeState_t home;
	uiCubeMotifState_t a,b,c;
	uiCubeMotifBasis_t wanted,authored;
	int moving=-1;
	UIHome_Init(&home,caps); (void)UIHome_Apply(&home,UI_HOME_INPUT_UP,caps);
	UICubeMotif_Build(&home,&wanted); UICubeMotif_Build(NULL,&authored);
	for(int f=0;f<UI_HOME_FACE_COUNT && moving<0;++f)
		if(!placed_alike(&authored,&wanted,f) && wanted.shown[f]) moving=f;
	CHECK(moving>=0);
	for(int mode=UI_MOTION_FULL;mode<=UI_MOTION_REDUCED;++mode) {
		UICubeMotif_Reset(&a); UICubeMotif_Reset(&b);
		UICubeMotif_Request(&a,&home,(uiMotionMode_t)mode);
		UICubeMotif_Request(&b,&home,(uiMotionMode_t)mode);
		for(int i=0;i<5;++i) update_without_visible_basis_swap(&a,1.0f/50.0f,(uiMotionMode_t)mode);
		for(int i=0;i<6;++i) update_without_visible_basis_swap(&b,1.0f/60.0f,(uiMotionMode_t)mode);
		CHECK(same(&a.basis,&b.basis) && same(&a.basis,&wanted));
		CHECK(fabsf(a.alpha[moving]-b.alpha[moving])<0.00001f &&
			fabsf(a.alpha[moving]-0.3f)<0.00001f);
		for(int i=0;i<10;++i) update_without_visible_basis_swap(&a,1.0f/50.0f,(uiMotionMode_t)mode);
		for(int i=0;i<12;++i) update_without_visible_basis_swap(&b,1.0f/60.0f,(uiMotionMode_t)mode);
		CHECK(UICubeMotif_Settled(&a) && UICubeMotif_Settled(&b));
	}
	UICubeMotif_Reset(&a); UICubeMotif_Request(&a,&home,UI_MOTION_OFF);
	CHECK(UICubeMotif_Settled(&a) && !a.changing && same(&a.basis,&wanted));
	UICubeMotif_Reset(&b); UICubeMotif_Request(&b,&home,UI_MOTION_FULL);
	UICubeMotif_Update(&b,0.0f,UI_MOTION_OFF);
	CHECK(UICubeMotif_Settled(&b) && !b.changing && same(&b.basis,&wanted));
	UICubeMotif_Reset(&a); UICubeMotif_Request(&a,&home,UI_MOTION_FULL); b=a;
	UICubeMotif_Update(&a,50.0f,UI_MOTION_FULL); UICubeMotif_Update(&b,0.05f,UI_MOTION_FULL);
	CHECK(memcmp(a.alpha,b.alpha,sizeof(a.alpha))==0 && same(&a.basis,&b.basis));
	c=a; UICubeMotif_Update(&a,NAN,UI_MOTION_FULL); UICubeMotif_Update(&a,INFINITY,UI_MOTION_FULL);
	UICubeMotif_Update(&a,-1.0f,UI_MOTION_FULL); UICubeMotif_Update(&a,0.0f,UI_MOTION_FULL);
	CHECK(memcmp(a.alpha,c.alpha,sizeof(a.alpha))==0 && same(&a.basis,&c.basis));
	UICubeMotif_Reset(NULL); UICubeMotif_Request(NULL,NULL,UI_MOTION_OFF);
	UICubeMotif_Update(NULL,0.1f,UI_MOTION_FULL); UICubeMotif_Build(NULL,NULL);
}

static void malformed_request_falls_back(void)
{
	uiHomeState_t home;
	uiCubeMotifBasis_t authored, actual;
	UICubeMotif_Build(NULL,&authored);
	UIHome_Init(&home,caps); home.face=(uiHomeFace_t)-9;
	UICubeMotif_Build(&home,&actual); CHECK(same(&authored,&actual));
	home.face=(uiHomeFace_t)127;
	UICubeMotif_Build(&home,&actual); CHECK(same(&authored,&actual));
	UIHome_Init(&home,caps); home.turnAxis=(uiHomeTurnAxis_t)7;
	UICubeMotif_Build(&home,&actual); CHECK(same(&authored,&actual));
	UIHome_Init(&home,caps); home.orientation.m[0][0]=-1;
	UICubeMotif_Build(&home,&actual); CHECK(same(&authored,&actual));
	UIHome_Init(&home,caps); home.orientation.m[0][0]=0;
	UICubeMotif_Build(&home,&actual); CHECK(same(&authored,&actual));
	UIHome_Init(&home,caps); home.orientation.m[1][2]=2;
	UICubeMotif_Build(&home,&actual); CHECK(same(&authored,&actual));
	/* A ring that is neither four nor five faces, or Apps in front of a
	 * ring without it. */
	UIHome_Init(&home,caps); home.faceCount=3;
	UICubeMotif_Build(&home,&actual); CHECK(same(&authored,&actual));
	UIHome_Init(&home,caps); home.faceCount=6;
	UICubeMotif_Build(&home,&actual); CHECK(same(&authored,&actual));
	UIHome_Init(&home,caps); home.face=UI_HOME_FACE_APPS;
	UICubeMotif_Build(&home,&actual); CHECK(same(&authored,&actual));
	CHECK(actual.faceCount==4);
}

int main(void)
{
	all_orientations_and_semantic_faces();
	five_face_ring_glyphs();
	same_axis_four_turns_leave_physical_glyphs_unchanged();
	mixed_halfway_retarget_and_latest_pending();
	frame_rate_and_motion_modes();
	malformed_request_falls_back();
	printf("Cube motif: %lu independent mapping/continuity checks passed\n",checks);
	return 0;
}
