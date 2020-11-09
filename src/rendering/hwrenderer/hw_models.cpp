// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
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
** gl_models.cpp
**
** hardware renderer model handling code
**
**/

#include "filesystem.h"
#include "g_game.h"
#include "doomstat.h"
#include "g_level.h"
#include "r_state.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "i_time.h"
#include "cmdlib.h"
#include "hw_material.h"
#include "hwrenderer/data/buffers.h"
#include "flatvertices.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "hw_renderstate.h"
#include "hwrenderer/scene/hw_portal.h"
#include "hw_models.h"
#include "hw_cvars.h"
#include "hwrenderer/data/hw_vrmodes.h"
#include "gl_renderstate.h"

CVAR(Bool, gl_light_models, true, CVAR_ARCHIVE)
EXTERN_CVAR(Float, gl_weaponOfsY)
EXTERN_CVAR(Float, gl_weaponOfsZ)

VSMatrix FHWModelRenderer::GetViewToWorldMatrix()
{
	VSMatrix objectToWorldMatrix;
	di->VPUniforms.mViewMatrix.inverseMatrix(objectToWorldMatrix);
	return objectToWorldMatrix;
}

void FHWModelRenderer::PrepareRenderHUDModel(FSpriteModelFrame* smf, float ofsX, float ofsY, VSMatrix &objectToWorldMatrix)
{
	auto vrmode = VRMode::GetVRMode(true);
	if (vrmode->mEyeCount > 1)
	{
		//TODO Remove gl_RenderState
		gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);
		// [BB] Render the weapon in worldspace to confirm transforms are all correct
		gl_RenderState.mModelMatrix.loadIdentity();
		// Need to reset the normal matrix too
		di->VPUniforms.mNormalViewMatrix.loadIdentity();

		if (vrmode->GetWeaponTransform(&gl_RenderState.mModelMatrix))
		{
			float scale = 0.01f;
			gl_RenderState.mModelMatrix.scale(scale, scale, scale);
			gl_RenderState.mModelMatrix.translate(0, 5 + gl_weaponOfsZ, 30 + gl_weaponOfsY);
		}
		else
		{
			AActor* playermo = players[consoleplayer].camera;
			DVector3 pos = playermo->InterpolatedPosition(r_viewpoint.TicFrac);
			gl_RenderState.mModelMatrix.translate(pos.X, pos.Z + 40, pos.Y);
			gl_RenderState.mModelMatrix.rotate(-playermo->Angles.Yaw.Degrees - 90, 0, 1, 0);
		}


		// Scaling model (y scale for a sprite means height, i.e. z in the world!).
		gl_RenderState.mModelMatrix.scale(smf->xscale, smf->zscale, smf->yscale);

		// Aplying model offsets (model offsets do not depend on model scalings).
		gl_RenderState.mModelMatrix.translate(smf->xoffset / smf->xscale, smf->zoffset / smf->zscale, smf->yoffset / smf->yscale);

		// [BB] Weapon bob, very similar to the normal Doom weapon bob.
		gl_RenderState.mModelMatrix.rotate(ofsX / 4, 0, 1, 0);
		gl_RenderState.mModelMatrix.rotate((ofsY - WEAPONTOP) / -4., 1, 0, 0);

		// [BB] For some reason the jDoom models need to be rotated.
		gl_RenderState.mModelMatrix.rotate(90.f, 0, 1, 0);

		// Applying angleoffset, pitchoffset, rolloffset.
		gl_RenderState.mModelMatrix.rotate(-smf->angleoffset, 0, 1, 0);
		gl_RenderState.mModelMatrix.rotate(smf->pitchoffset, 0, 0, 1);
		gl_RenderState.mModelMatrix.rotate(-smf->rolloffset, 1, 0, 0);
		gl_RenderState.EnableModelMatrix(true);
		gl_RenderState.EnableModelMatrix(false);
		objectToWorldMatrix = gl_RenderState.mModelMatrix;
	}
	else
	{
		FModelRenderer::PrepareRenderHUDModel(smf, ofsX, ofsY, objectToWorldMatrix);
	}
}

void FHWModelRenderer::BeginDrawModel(FRenderStyle style, FSpriteModelFrame *smf, const VSMatrix &objectToWorldMatrix, bool mirrored)
{
	state.SetDepthFunc(DF_LEqual);
	state.EnableTexture(true);
	// [BB] In case the model should be rendered translucent, do back face culling.
	// This solves a few of the problems caused by the lack of depth sorting.
	// [Nash] Don't do back face culling if explicitly specified in MODELDEF
	// TO-DO: Implement proper depth sorting.
	if (!(style == DefaultRenderStyle()) && !(smf->flags & MDL_DONTCULLBACKFACES))
	{
		state.SetCulling((mirrored ^ portalState.isMirrored()) ? Cull_CCW : Cull_CW);
	}

	state.mModelMatrix = objectToWorldMatrix;
	state.EnableModelMatrix(true);
}

void FHWModelRenderer::EndDrawModel(FRenderStyle style, FSpriteModelFrame *smf)
{
	state.EnableModelMatrix(false);
	state.SetDepthFunc(DF_Less);
	if (!(style == DefaultRenderStyle()) && !(smf->flags & MDL_DONTCULLBACKFACES))
		state.SetCulling(Cull_None);
}

void FHWModelRenderer::BeginDrawHUDModel(FRenderStyle style, const VSMatrix &objectToWorldMatrix, bool mirrored)
{
	state.SetDepthFunc(DF_LEqual);

	// [BB] In case the model should be rendered translucent, do back face culling.
	// This solves a few of the problems caused by the lack of depth sorting.
	// TO-DO: Implement proper depth sorting.
	if (!(style == DefaultRenderStyle()))
	{
		state.SetCulling((mirrored ^ portalState.isMirrored()) ? Cull_CW : Cull_CCW);
	}

	state.mModelMatrix = objectToWorldMatrix;
	state.EnableModelMatrix(true);
}

void FHWModelRenderer::EndDrawHUDModel(FRenderStyle style)
{
	state.EnableModelMatrix(false);

	state.SetDepthFunc(DF_Less);
	if (!(style == DefaultRenderStyle()))
		state.SetCulling(Cull_None);
}

IModelVertexBuffer *FHWModelRenderer::CreateVertexBuffer(bool needindex, bool singleframe)
{
	return new FModelVertexBuffer(needindex, singleframe);
}

void FHWModelRenderer::SetInterpolation(double inter)
{
	state.SetInterpolationFactor((float)inter);
}

void FHWModelRenderer::SetMaterial(FGameTexture *skin, bool clampNoFilter, int translation)
{
	state.SetMaterial(skin, UF_Skin, 0, clampNoFilter ? CLAMP_NOFILTER : CLAMP_NONE, translation, -1);
	state.SetLightIndex(modellightindex);
}

void FHWModelRenderer::DrawArrays(int start, int count)
{
	state.Draw(DT_Triangles, start, count);
}

void FHWModelRenderer::DrawElements(int numIndices, size_t offset)
{
	state.DrawIndexed(DT_Triangles, int(offset / sizeof(unsigned int)), numIndices);
}

//===========================================================================
//
//
//
//===========================================================================

void FHWModelRenderer::SetupFrame(FModel *model, unsigned int frame1, unsigned int frame2, unsigned int size)
{
	auto mdbuff = static_cast<FModelVertexBuffer*>(model->GetVertexBuffer(GetType()));
	state.SetVertexBuffer(mdbuff->vertexBuffer(), frame1, frame2);
	if (mdbuff->indexBuffer()) state.SetIndexBuffer(mdbuff->indexBuffer());
}

