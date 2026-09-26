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
static const uiHomeCapabilities_t caps = {true, true};

static bool same(const uiCubeMotifBasis_t *a, const uiCubeMotifBasis_t *b)
{
	for(int f=0; f<UI_HOME_FACE_COUNT; ++f)
		for(int r=0;r<3;++r) for(int c=0;c<3;++c)
			if(a->face[f][r][c] != b->face[f][r][c]) return false;
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
	CHECK(isfinite(state->alpha) && state->alpha>=0.0f && state->alpha<=1.0f);
	proper_basis(&state->basis);
	if(!same(&before.basis,&state->basis)) {
		CHECK(before.changing);
		/* A basis may change inside a frame only after reaching zero opacity.
		 * The frame's remaining time belongs to the new map's fade-in. */
		CHECK(before.alpha*0.070f<=dt+0.000001f);
		CHECK(same(&state->basis,&before.pending));
		CHECK(fabsf(state->alpha-(dt-before.alpha*0.070f)/0.100f)<0.00001f);
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
	CHECK(state.changing && state.alpha==1.0f && same(&state.basis,&original));
	update_without_visible_basis_swap(&state,0.025f,UI_MOTION_FULL);
	CHECK(same(&state.basis,&original));
	CHECK(state.alpha>0.0f && state.alpha<1.0f);
	/* Repeated requests replace one pending map without restarting alpha. */
	for(int i=0;i<10000;++i) {
		uiCubeMotifBasis_t drawn=state.basis;
		float alpha=state.alpha;
		uiHomeInput_t input=i%3==0 ? UI_HOME_INPUT_DOWN:
			(i%3==1 ? UI_HOME_INPUT_RIGHT:UI_HOME_INPUT_UP);
		(void)UIHome_Apply(&home,input,caps);
		UICubeMotif_Request(&state,&home,UI_MOTION_FULL);
		UICubeMotif_Build(&home,&wanted);
		CHECK(same(&state.pending,&wanted));
		CHECK(same(&state.basis,&drawn) && state.alpha==alpha);
		update_without_visible_basis_swap(&state,0.001f,UI_MOTION_FULL);
	}
	for(int i=0;i<30;++i) update_without_visible_basis_swap(&state,0.01f,UI_MOTION_FULL);
	CHECK(same(&state.basis,&wanted) && state.alpha==1.0f && !state.changing);
	CHECK(sizeof(state)<=512u);
	/* Returning to the currently displayed map cancels an obsolete swap. */
	UICubeMotif_Reset(&state); UIHome_Init(&home,caps);
	(void)UIHome_Apply(&home,UI_HOME_INPUT_UP,caps);
	UICubeMotif_Request(&state,&home,UI_MOTION_FULL);
	update_without_visible_basis_swap(&state,0.03f,UI_MOTION_FULL);
	UICubeMotif_Request(&state,NULL,UI_MOTION_FULL);
	CHECK(!state.changing && same(&state.basis,&original));
	for(int i=0;i<10;++i) update_without_visible_basis_swap(&state,0.01f,UI_MOTION_FULL);
	CHECK(state.alpha==1.0f && same(&state.basis,&original));
}

static void frame_rate_and_motion_modes(void)
{
	uiHomeState_t home;
	uiCubeMotifState_t a,b,c;
	uiCubeMotifBasis_t wanted;
	UIHome_Init(&home,caps); (void)UIHome_Apply(&home,UI_HOME_INPUT_UP,caps);
	UICubeMotif_Build(&home,&wanted);
	for(int mode=UI_MOTION_FULL;mode<=UI_MOTION_REDUCED;++mode) {
		UICubeMotif_Reset(&a); UICubeMotif_Reset(&b);
		UICubeMotif_Request(&a,&home,(uiMotionMode_t)mode);
		UICubeMotif_Request(&b,&home,(uiMotionMode_t)mode);
		for(int i=0;i<5;++i) update_without_visible_basis_swap(&a,1.0f/50.0f,(uiMotionMode_t)mode);
		for(int i=0;i<6;++i) update_without_visible_basis_swap(&b,1.0f/60.0f,(uiMotionMode_t)mode);
		CHECK(same(&a.basis,&b.basis) && same(&a.basis,&wanted));
		CHECK(fabsf(a.alpha-b.alpha)<0.00001f && fabsf(a.alpha-0.3f)<0.00001f);
		for(int i=0;i<10;++i) update_without_visible_basis_swap(&a,1.0f/50.0f,(uiMotionMode_t)mode);
		for(int i=0;i<12;++i) update_without_visible_basis_swap(&b,1.0f/60.0f,(uiMotionMode_t)mode);
		CHECK(a.alpha==1.0f && b.alpha==1.0f);
	}
	UICubeMotif_Reset(&a); UICubeMotif_Request(&a,&home,UI_MOTION_OFF);
	CHECK(a.alpha==1.0f && !a.changing && same(&a.basis,&wanted));
	UICubeMotif_Reset(&b); UICubeMotif_Request(&b,&home,UI_MOTION_FULL);
	UICubeMotif_Update(&b,0.0f,UI_MOTION_OFF);
	CHECK(b.alpha==1.0f && !b.changing && same(&b.basis,&wanted));
	UICubeMotif_Reset(&a); UICubeMotif_Request(&a,&home,UI_MOTION_FULL); b=a;
	UICubeMotif_Update(&a,50.0f,UI_MOTION_FULL); UICubeMotif_Update(&b,0.05f,UI_MOTION_FULL);
	CHECK(a.alpha==b.alpha && same(&a.basis,&b.basis));
	c=a; UICubeMotif_Update(&a,NAN,UI_MOTION_FULL); UICubeMotif_Update(&a,INFINITY,UI_MOTION_FULL);
	UICubeMotif_Update(&a,-1.0f,UI_MOTION_FULL); UICubeMotif_Update(&a,0.0f,UI_MOTION_FULL);
	CHECK(a.alpha==c.alpha && same(&a.basis,&c.basis));
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
}

int main(void)
{
	all_orientations_and_semantic_faces();
	same_axis_four_turns_leave_physical_glyphs_unchanged();
	mixed_halfway_retarget_and_latest_pending();
	frame_rate_and_motion_modes();
	malformed_request_falls_back();
	printf("Cube motif: %lu independent mapping/continuity checks passed\n",checks);
	return 0;
}
