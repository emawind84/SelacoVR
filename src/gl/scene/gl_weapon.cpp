// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2000-2018 Christoph Oelckers
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

#include "gl_load/gl_system.h"
#include "r_utility.h"
#include "v_video.h"
#include "d_player.h"

#include "gl_load/gl_interface.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/scene/hw_weapon.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/models/gl_models.h"
#include "gl/renderer/gl_quaddrawer.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include <hwrenderer\utility\hw_vrmodes.h>

EXTERN_CVAR(Bool, r_drawplayersprites)
EXTERN_CVAR(Float, transsouls)
EXTERN_CVAR(Int, gl_fuzztype)
EXTERN_CVAR(Bool, r_deathcamera)
EXTERN_CVAR(Int, r_PlayerSprites3DMode)
EXTERN_CVAR(Float, gl_fatItemWidth)

enum PlayerSprites3DMode
{
	CROSSED,
	BACK_ONLY,
	ITEM_ONLY,
	FAT_ITEM,
};
//==========================================================================
//
// R_DrawPSprite
//
//==========================================================================

void FDrawInfo::DrawPSprite(HUDSprite* huds)
{
	if (huds->RenderStyle.BlendOp == STYLEOP_Shadow)
	{
		gl_RenderState.SetColor(0.2f, 0.2f, 0.2f, 0.33f, huds->cm.Desaturation);
	}
	else
	{
		SetColor(huds->lightlevel, 0, huds->cm, huds->alpha, true);
	}
	gl_SetRenderStyle(huds->RenderStyle, false, false);
	gl_RenderState.SetObjectColor(huds->ObjectColor);
	gl_RenderState.SetDynLight(huds->dynrgb[0], huds->dynrgb[1], huds->dynrgb[2]);
	gl_RenderState.EnableBrightmap(!(huds->RenderStyle.Flags & STYLEF_ColorIsFixed));

	auto vrmode = VRMode::GetVRMode(true);

	if (huds->mframe)
	{
		gl_RenderState.AlphaFunc(GL_GEQUAL, 0);
        FGLModelRenderer renderer(this, huds->lightindex);
        renderer.RenderHUDModel(huds->weapon, huds->mx, huds->my);
	}
	else
	{
		float thresh = (huds->tex->tex->GetTranslucency() || huds->OverrideShader != -1) ? 0.f : gl_mask_sprite_threshold;
		gl_RenderState.AlphaFunc(GL_GEQUAL, thresh);
		gl_RenderState.SetMaterial(huds->tex, CLAMP_XY_NOMIP, 0, huds->OverrideShader, !!(huds->RenderStyle.Flags & STYLEF_RedIsAlpha));
		gl_RenderState.Apply();

		if (vrmode->mEyeCount == 1 || (r_PlayerSprites3DMode != ITEM_ONLY && r_PlayerSprites3DMode != FAT_ITEM))
		{
			GLRenderer->mVBO->RenderArray(GL_TRIANGLE_STRIP, huds->mx, 4);
		}


		player_t* player = huds->player;
		DPSprite* psp = huds->weapon;
		bool alphatexture = huds->RenderStyle.Flags & STYLEF_RedIsAlpha;
		float sy;

		//TODO Cleanup code for rendering weapon models from sprites in VR mode
		if (psp->GetID() == PSP_WEAPON && vrmode->RenderPlayerSpritesCrossed())
		{
			if (r_PlayerSprites3DMode == BACK_ONLY)
				return;

			float fU1, fV1;
			float fU2, fV2;

			AWeapon* wi = player->ReadyWeapon;
			if (wi == nullptr)
				return;

			// decide which patch to use
			bool mirror;
			FTextureID lump = sprites[psp->GetSprite()].GetSpriteFrame(psp->GetFrame(), 0, 0., &mirror);
			if (!lump.isValid()) return;

			FMaterial* tex = FMaterial::ValidateTexture(lump, true, false);
			if (!tex) return;

			gl_RenderState.SetMaterial(tex, CLAMP_XY_NOMIP, 0, huds->OverrideShader, alphatexture);

			float vw = (float)viewwidth;
			float vh = (float)viewheight;

			FState* spawn = wi->FindState(NAME_Spawn);

			lump = sprites[spawn->sprite].GetSpriteFrame(0, 0, 0., &mirror);
			if (!lump.isValid()) return;

			tex = FMaterial::ValidateTexture(lump, true, false);
			if (!tex) return;

			gl_RenderState.SetMaterial(tex, CLAMP_XY_NOMIP, 0, huds->OverrideShader, alphatexture);

			float z1 = 0.0f;
			float z2 = (huds->y2 - huds->y1) * MIN(3, tex->GetWidth() / tex->GetHeight());

			if (!(mirror) != !(psp->Flags & PSPF_FLIP))
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

			if (r_PlayerSprites3DMode == FAT_ITEM)
			{
				float x1 = vw / 2 + (huds->x1 - vw / 2) * gl_fatItemWidth;
				float x2 = vw / 2 + (huds->x2 - vw / 2) * gl_fatItemWidth;

				for (float x = x1; x < x2; x += 1)
				{
					FQuadDrawer qd2;
					qd2.Set(0, x, huds->y1, -z1, fU1, fV1);
					qd2.Set(1, x, huds->y2, -z1, fU1, fV2);
					qd2.Set(2, x, huds->y1, -z2, fU2, fV1);
					qd2.Set(3, x, huds->y2, -z2, fU2, fV2);
					qd2.Render(GL_TRIANGLE_STRIP);
				}
			}
			else
			{
				float crossAt;
				if (r_PlayerSprites3DMode == ITEM_ONLY)
				{
					crossAt = 0.0f;
					sy = 0.0f;
				}
				else
				{
					sy = huds->y2 - huds->y1;
					crossAt = sy * 0.25f;
				}

				float y1 = huds->y1 - crossAt;
				float y2 = huds->y2 - crossAt;

				FQuadDrawer qd2;
				qd2.Set(0, vw / 2 - crossAt, y1, -z1, fU1, fV1);
				qd2.Set(1, vw / 2 + sy / 2, y2, -z1, fU1, fV2);
				qd2.Set(2, vw / 2 - crossAt, y1, -z2, fU2, fV1);
				qd2.Set(3, vw / 2 + sy / 2, y2, -z2, fU2, fV2);
				qd2.Render(GL_TRIANGLE_STRIP);

				FQuadDrawer qd3;
				qd3.Set(0, vw / 2 + crossAt, y1, -z1, fU1, fV1);
				qd3.Set(1, vw / 2 - sy / 2, y2, -z1, fU1, fV2);
				qd3.Set(2, vw / 2 + crossAt, y1, -z2, fU2, fV1);
				qd3.Set(3, vw / 2 - sy / 2, y2, -z2, fU2, fV2);
				qd3.Render(GL_TRIANGLE_STRIP);
			}
		}
	}

	gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);
	gl_RenderState.SetObjectColor(0xffffffff);
	gl_RenderState.SetDynLight(0, 0, 0);
	gl_RenderState.EnableBrightmap(false);
}

//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void FDrawInfo::DrawPlayerSprites(bool hudModelStep)
{
	auto vrmode = VRMode::GetVRMode(true);
	vrmode->AdjustPlayerSprites(this);

	int oldlightmode = level.lightmode;
	if (!hudModelStep && level.lightmode == 8) level.lightmode = 2;	// Software lighting cannot handle 2D content so revert to lightmode 2 for that.
	for (auto& hudsprite : hudsprites)
	{
		if ((!!hudsprite.mframe) == hudModelStep)
			DrawPSprite(&hudsprite);
	}
	
	vrmode->DrawControllerModels(this);

	gl_RenderState.SetObjectColor(0xffffffff);
	gl_RenderState.SetDynLight(0, 0, 0);
	gl_RenderState.EnableBrightmap(false);

	level.lightmode = oldlightmode;
	if (!hudModelStep)
	{
		vrmode->UnAdjustPlayerSprites();
	}
}

void FDrawInfo::AddHUDSprite(HUDSprite* huds)
{
	hudsprites.Push(*huds);
}
