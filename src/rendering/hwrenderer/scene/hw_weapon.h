#pragma once

#include "vectors.h"

class DPSprite;
class player_t;
class AActor;
enum area_t : int;
struct FSpriteModelFrame;
struct HWDrawInfo;
class FGameTexture;


struct WeaponPosition
{
	float wx, wy;
	float bobx, boby;
	DPSprite *weapon;
};

struct WeaponLighting
{
	FColormap cm;
	int lightlevel;
	bool isbelow;
};

struct HUDSprite
{
	AActor *owner;
	DPSprite *weapon;
	FGameTexture *texture;
	FSpriteModelFrame *mframe;

	FColormap cm;
	int lightlevel;
	PalEntry ObjectColor;

	player_t* player;

	FRenderStyle RenderStyle;
	float alpha;
	int OverrideShader;

	//TODO Remove added for VR weapon sprites fields
	float x1, y1, x2, y2, u1, v1, u2, v2;
	
	float mx, my;
	float dynrgb[3];

	int lightindex;

	void SetBright(bool isbelow);
	bool GetWeaponRenderStyle(DPSprite *psp, AActor *playermo, sector_t *viewsector, WeaponLighting &light);
	bool GetWeaponRect(HWDrawInfo *di, DPSprite *psp, float sx, float sy, player_t *player, double ticfrac);

};
