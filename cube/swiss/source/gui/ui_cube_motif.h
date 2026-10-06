#ifndef UI_CUBE_MOTIF_H
#define UI_CUBE_MOTIF_H

#include "ui_home.h"
#include "ui_motion.h"

/* Columns are right, up and outward normal in cube-local coordinates.
 * faceCount is the ring the faces were placed for: Library and the faces on
 * its sides. shown is false for a face with no side of its own, which draws
 * nothing: a face outside the ring, such as Apps without apps, and in a ring
 * of five the face two ahead of the one in front (a cube's band has four
 * sides for five faces). */
typedef struct {
	float face[UI_HOME_FACE_COUNT][3][3];
	bool shown[UI_HOME_FACE_COUNT];
	int faceCount;
} uiCubeMotifBasis_t;

/* Each face's glyph fades on its own: one that has to move to another side
 * fades out where it is, moves, and fades in; the others stay. alpha is a
 * face's opacity, 0 for a face not shown. */
typedef struct {
	uiCubeMotifBasis_t basis;
	uiCubeMotifBasis_t pending;
	float alpha[UI_HOME_FACE_COUNT];
	bool changing;
} uiCubeMotifState_t;

/* NULL, or a malformed state, selects the authored background's original
 * four lateral faces, Apps not shown. Otherwise the face in front, the next
 * one in the ring on the right (below, turning vertically), the previous one
 * on the left (above), and the remaining one of a ring of four behind. A ring
 * of five puts the face two behind the one in front at the back and does not
 * show the one two ahead: turning either way then only moves glyphs on sides
 * facing away. In Classic every face keeps its own side wherever the cube
 * turns: Library front and each other face on its side, by default Source
 * top, Settings left, System right and Apps bottom. */
void UICubeMotif_Build(const uiHomeState_t *home, uiCubeMotifBasis_t *out);
void UICubeMotif_Reset(uiCubeMotifState_t *state);
void UICubeMotif_Request(uiCubeMotifState_t *state,
	const uiHomeState_t *home, uiMotionMode_t mode);
void UICubeMotif_Update(uiCubeMotifState_t *state,
	float deltaSeconds, uiMotionMode_t mode);
/* Every glyph where it goes, at its full opacity or none. */
bool UICubeMotif_Settled(const uiCubeMotifState_t *state);

#endif
