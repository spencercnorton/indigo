#ifndef UI_CUBE_MOTIF_H
#define UI_CUBE_MOTIF_H

#include "ui_home.h"
#include "ui_motion.h"

/* Columns are right, up and outward normal in cube-local coordinates. */
typedef struct {
	float face[UI_HOME_FACE_COUNT][3][3];
} uiCubeMotifBasis_t;

typedef struct {
	uiCubeMotifBasis_t basis;
	uiCubeMotifBasis_t pending;
	float alpha;
	bool changing;
} uiCubeMotifState_t;

/* NULL selects the authored background's original four lateral faces. */
void UICubeMotif_Build(const uiHomeState_t *home, uiCubeMotifBasis_t *out);
void UICubeMotif_Reset(uiCubeMotifState_t *state);
void UICubeMotif_Request(uiCubeMotifState_t *state,
	const uiHomeState_t *home, uiMotionMode_t mode);
void UICubeMotif_Update(uiCubeMotifState_t *state,
	float deltaSeconds, uiMotionMode_t mode);

#endif
