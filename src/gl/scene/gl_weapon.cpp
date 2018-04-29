// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2000-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_weapon.cpp
** Weapon sprite drawing
**
*/

#include "gl/system/gl_system.h"
#include "sbar.h"
#include "r_utility.h"
#include "v_video.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_levellocals.h"

#include "gl/system/gl_interface.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/scene/hw_weapon.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_scenedrawer.h"
#include "gl/models/gl_models.h"
#include "gl/renderer/gl_quaddrawer.h"
#include "gl/stereo3d/gl_stereo3d.h"
#include "gl/dynlights/gl_lightbuffer.h"

EXTERN_CVAR (Bool, r_drawplayersprites)
EXTERN_CVAR (Bool, r_deathcamera)


//==========================================================================
//
// R_DrawPSprite
//
//==========================================================================

void GLSceneDrawer::DrawPSprite (player_t * player,DPSprite *psp, float sx, float sy, int OverrideShader, bool alphatexture, double ticfrac)
{
	float			fU1,fV1;
	float			fU2,fV2;
	float			tx;
	float			x1,y1,x2,y2;
	float			scale;
	float			scalex;
	float			ftexturemid;
	
	// decide which patch to use
	bool mirror;
	FTextureID lump = sprites[psp->GetSprite()].GetSpriteFrame(psp->GetFrame(), 0, 0., &mirror);
	if (!lump.isValid()) return;

	FMaterial * tex = FMaterial::ValidateTexture(lump, true, false);
	if (!tex) return;

	uint32_t trans = psp->GetTranslation() != 0 ? psp->GetTranslation() : 0;
	if ((psp->Flags & PSPF_PLAYERTRANSLATED)) trans = psp->Owner->mo->Translation;
	gl_RenderState.SetMaterial(tex, CLAMP_XY_NOMIP, trans, OverrideShader, alphatexture);

	float vw = (float)viewwidth;
	float vh = (float)viewheight;

	FloatRect r;
	tex->GetSpriteRect(&r);

	// calculate edges of the shape
	scalex = (320.0f / (240.0f * r_viewwindow.WidescreenRatio)) * vw / 320;

	tx = (psp->Flags & PSPF_MIRROR) ? ((160 - r.width) - (sx + r.left)) : (sx - (160 - r.left));
	x1 = tx * scalex + vw/2;
	// [MC] Disabled these because vertices can be manipulated now.
	//if (x1 > vw)	return; // off the right side
	x1 += viewwindowx;

	tx += r.width;
	x2 = tx * scalex + vw / 2;
	//if (x2 < 0) return; // off the left side
	x2 += viewwindowx;

	// killough 12/98: fix psprite positioning problem
	ftexturemid = 100.f - sy - r.top - psp->GetYAdjust(screenblocks >= 11);

	scale = (SCREENHEIGHT*vw) / (SCREENWIDTH * 200.0f);
	y1 = viewwindowy + vh / 2 - (ftexturemid * scale);
	y2 = y1 + (r.height * scale) + 1;

	const bool flip = (psp->Flags & PSPF_FLIP);
	if (!(mirror) != !(flip))
	{
		fU2 = tex->GetSpriteUL();
		fV1 = tex->GetSpriteVT();
		fU1 = tex->GetSpriteUR();
		fV2 = tex->GetSpriteVB();
	}
	else
	{
		fU1 = tex->GetSpriteUL();
		fV1 = tex->GetSpriteVT();
		fU2 = tex->GetSpriteUR();
		fV2 = tex->GetSpriteVB();
		
	}

	// [MC] Code copied from DTA_Rotate.
	// Big thanks to IvanDobrovski who helped me modify this.

	WeaponInterp Vert;
	Vert.v[0] = FVector2(x1, y1);
	Vert.v[1] = FVector2(x1, y2);
	Vert.v[2] = FVector2(x2, y1);
	Vert.v[3] = FVector2(x2, y2);

	for (int i = 0; i < 4; i++)
	{
		const float cx = (flip) ? -psp->Coord[i].X : psp->Coord[i].X;
		Vert.v[i] += FVector2(cx * scalex, psp->Coord[i].Y * scale);
	}
	if (psp->rotation != 0.0 || !psp->scale.isZero())
	{
		// [MC] Sets up the alignment for starting the pivot at, in a corner.
		float anchorx, anchory;
		switch (psp->VAlign)
		{
			default:
			case PSPA_TOP:		anchory = 0.0;	break;
			case PSPA_CENTER:	anchory = 0.5;	break;
			case PSPA_BOTTOM:	anchory = 1.0;	break;
		}

		switch (psp->HAlign)
		{
			default:
			case PSPA_LEFT:		anchorx = 0.0;	break;
			case PSPA_CENTER:	anchorx = 0.5;	break;
			case PSPA_RIGHT:	anchorx = 1.0;	break;
		}
		// Handle PSPF_FLIP.
		if (flip) anchorx = 1.0 - anchorx;

		FAngle rot = float((flip) ? -psp->rotation.Degrees : psp->rotation.Degrees);
		const float cosang = rot.Cos();
		const float sinang = rot.Sin();
		
		float xcenter, ycenter;
		const float width = x2 - x1;
		const float height = y2 - y1;
		const float px = float((flip) ? -psp->pivot.X : psp->pivot.X);
		const float py = float(psp->pivot.Y);

		// Set up the center and offset accordingly. PivotPercent changes it to be a range [0.0, 1.0]
		// instead of pixels and is enabled by default.
		if (psp->Flags & PSPF_PIVOTPERCENT)
		{
			xcenter = x1 + (width * anchorx + width * px);
			ycenter = y1 + (height * anchory + height * py);
		}
		else
		{
			xcenter = x1 + (width * anchorx + scalex * px);
			ycenter = y1 + (height * anchory + scale * py);
		}

		// Now adjust the position, rotation and scale of the image based on the latter two.
		for (int i = 0; i < 4; i++)
		{
			Vert.v[i] -= {xcenter, ycenter};
			const float xx = xcenter + psp->scale.X * (Vert.v[i].X * cosang + Vert.v[i].Y * sinang);
			const float yy = ycenter - psp->scale.Y * (Vert.v[i].X * sinang - Vert.v[i].Y * cosang);
			Vert.v[i] = {xx, yy};
		}
	}
	psp->Vert = Vert;

	if (psp->scale.X == 0.0 || psp->scale.Y == 0.0)
		return;

	const bool interp = (psp->InterpolateTic || psp->Flags & PSPF_INTERPOLATE);

	for (int i = 0; i < 4; i++)
	{
		FVector2 t = Vert.v[i];
		if (interp)
			t = psp->Prev.v[i] + (psp->Vert.v[i] - psp->Prev.v[i]) * ticfrac;

		Vert.v[i] = t;
	}

	if (tex->tex->GetTranslucency() || OverrideShader != -1)
	{
		gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	}
	gl_RenderState.Apply();
	FQuadDrawer qd;
	qd.Set(0, Vert.v[0].X, Vert.v[0].Y, 0, fU1, fV1);
	qd.Set(1, Vert.v[1].X, Vert.v[1].Y, 0, fU1, fV2);
	qd.Set(2, Vert.v[2].X, Vert.v[2].Y, 0, fU2, fV1);
	qd.Set(3, Vert.v[3].X, Vert.v[3].Y, 0, fU2, fV2);
	qd.Render(GL_TRIANGLE_STRIP);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.5f);
}

//==========================================================================
//
//
//
//==========================================================================

void GLSceneDrawer::SetupWeaponLight()
{
	weapondynlightindex.Clear();

	AActor *camera = r_viewpoint.camera;
	AActor * playermo = players[consoleplayer].camera;
	player_t * player = playermo->player;

	// this is the same as in DrawPlayerSprites below (i.e. no weapon being drawn.)
	if (!player ||
		!r_drawplayersprites ||
		!camera->player ||
		(player->cheats & CF_CHASECAM) ||
		(r_deathcamera && camera->health <= 0))
		return;

	// Check if lighting can be used on this item.
	if (camera->RenderStyle.BlendOp == STYLEOP_Shadow || !gl_lights || !gl_light_sprites || !GLRenderer->mLightCount || FixedColormap != CM_DEFAULT || gl.legacyMode)
		return;

	for (DPSprite *psp = player->psprites; psp != nullptr && psp->GetID() < PSP_TARGETCENTER; psp = psp->GetNext())
	{
		if (psp->GetState() != nullptr)
		{
			FSpriteModelFrame *smf = psp->Caller != nullptr ? FindModelFrame(psp->Caller->GetClass(), psp->GetState()->sprite, psp->GetState()->GetFrame(), false): nullptr;
			if (smf)
			{
				weapondynlightindex[psp] = gl_SetDynModelLight(playermo, -1);
			}
		}
	}
}

//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void GLSceneDrawer::DrawPlayerSprites(sector_t * viewsector, bool hudModelStep)
{
	bool brightflash = false;
	FColormap cm;
	AActor * playermo=players[consoleplayer].camera;
	player_t * player=playermo->player;
	
	s3d::Stereo3DMode::getCurrentMode().AdjustPlayerSprites();

	AActor *camera = r_viewpoint.camera;

	// this is the same as the software renderer
	if (!player ||
		!r_drawplayersprites ||
		!camera->player ||
		(player->cheats & CF_CHASECAM) || 
		(r_deathcamera && camera->health <= 0))
		return;

	WeaponPosition weap = GetWeaponPosition(camera->player);
	WeaponLighting light = GetWeaponLighting(viewsector, r_viewpoint.Pos, FixedColormap, in_area, camera->Pos());

	gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);

	// hack alert! Rather than changing everything in the underlying lighting code let's just temporarily change
	// light mode here to draw the weapon sprite.
	int oldlightmode = level.lightmode;
	if (level.lightmode >= 8) level.lightmode = 2;

	for(DPSprite *psp = player->psprites; psp != nullptr && psp->GetID() < PSP_TARGETCENTER; psp = psp->GetNext())
	{
		WeaponRenderStyle rs = GetWeaponRenderStyle(psp, camera);
		if (rs.RenderStyle.BlendOp == STYLEOP_None) continue;

		gl_SetRenderStyle(rs.RenderStyle, false, false);

		PalEntry ThingColor = (camera->RenderStyle.Flags & STYLEF_ColorIsFixed) ? camera->fillcolor : 0xffffff;
		ThingColor.a = 255;

		// now draw the different layers of the weapon.
		// For stencil render styles brightmaps need to be disabled.
		gl_RenderState.EnableBrightmap(!(rs.RenderStyle.Flags & STYLEF_ColorIsFixed));

		const bool bright = isBright(psp);
		const PalEntry finalcol = bright
			? ThingColor
			: ThingColor.Modulate(viewsector->SpecialColors[sector_t::sprites]);
		gl_RenderState.SetObjectColor(finalcol);

		if (psp->GetState() != nullptr) 
		{
			FColormap cmc = cm;
			int ll = light.lightlevel;
			if (bright)
			{
				if (light.isbelow)	
				{
					cmc.MakeWhite();
				}
				else
				{
					// under water areas keep most of their color for fullbright objects
					cmc.LightColor.r = (3 * cmc.LightColor.r + 0xff) / 4;
					cmc.LightColor.g = (3*cmc.LightColor.g + 0xff)/4;
					cmc.LightColor.b = (3*cmc.LightColor.b + 0xff)/4;
				}
				ll = 255;
			}
			// set the lighting parameters
			if (rs.RenderStyle.BlendOp == STYLEOP_Shadow)
			{
				gl_RenderState.SetColor(0.2f, 0.2f, 0.2f, 0.33f, cmc.Desaturation);
			}
			else
			{
				if (gl_lights && GLRenderer->mLightCount && FixedColormap == CM_DEFAULT && gl_light_sprites)
				{
					FSpriteModelFrame *smf = psp->Caller != nullptr ? FindModelFrame(psp->Caller->GetClass(), psp->GetSprite(), psp->GetState()->GetFrame(), false) : nullptr;
					if (!smf || gl.legacyMode)	// For models with per-pixel lighting this was done in a previous pass.
					{
						float out[3];
						gl_drawinfo->GetDynSpriteLight(playermo, nullptr, out);
						gl_RenderState.SetDynLight(out[0], out[1], out[2]);
					}
				}
				SetColor(ll, 0, cmc, rs.alpha, true);
			}
			if (playermo->Sector)
			{
				gl_RenderState.SetAddColor(playermo->Sector->AdditiveColors[sector_t::sprites] | 0xff000000);
			}
			else
			{
				gl_RenderState.SetAddColor(0);
			}

			FVector2 spos = BobWeapon(weap, psp);

			// [BB] In the HUD model step we just render the model and break out. 
			if (hudModelStep)
			{
				gl_RenderHUDModel(psp, spos.X, spos.Y, weapondynlightindex[psp]);
			}
			else
			{
				DrawPSprite(player, psp, spos.X, spos.Y, rs.OverrideShader, !!(rs.RenderStyle.Flags & STYLEF_RedIsAlpha), r_viewpoint.TicFrac);
			}
		}
	}
	gl_RenderState.SetObjectColor(0xffffffff);
	gl_RenderState.SetAddColor(0);
	gl_RenderState.SetDynLight(0, 0, 0);
	gl_RenderState.EnableBrightmap(false);
	level.lightmode = oldlightmode;
}


//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void GLSceneDrawer::DrawTargeterSprites()
{
	AActor * playermo=players[consoleplayer].camera;
	player_t * player=playermo->player;
	
	if(!player || playermo->renderflags&RF_INVISIBLE || !r_drawplayersprites ||
		GLRenderer->mViewActor!=playermo) return;

	gl_RenderState.EnableBrightmap(false);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl_RenderState.AlphaFunc(GL_GEQUAL,gl_mask_sprite_threshold);
	gl_RenderState.BlendEquation(GL_FUNC_ADD);
	gl_RenderState.ResetColor();
	gl_RenderState.SetTextureMode(TM_MODULATE);

	// The Targeter's sprites are always drawn normally.
	for (DPSprite *psp = player->FindPSprite(PSP_TARGETCENTER); psp != nullptr; psp = psp->GetNext())
	{
		if (psp->GetState() != nullptr) DrawPSprite(player, psp, psp->x, psp->y, 0, false, r_viewpoint.TicFrac);
	}
}
