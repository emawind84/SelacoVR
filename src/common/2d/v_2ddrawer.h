#ifndef __2DDRAWER_H
#define __2DDRAWER_H

#include "buffers.h"
#include "tarray.h"
#include "vectors.h"
#include "textures.h"
#include "renderstyle.h"
#include "dobject.h"
#include "refcounted.h"

struct DrawParms;
struct FColormap;
struct IntRect;

class DShape2DTransform : public DObject
{

DECLARE_CLASS(DShape2DTransform, DObject)
public:
	DShape2DTransform()
	{
		transform.Identity();
	}
	DMatrix3x3 transform;
};

// intermediate struct for shape drawing

enum EClearWhich
{
	C_Verts = 1,
	C_Coords = 2,
	C_Indices = 4,
};

class F2DVertexBuffer;

struct F2DPolygons
{
	TArray<FVector4> vertices;
	TArray<int>  indices;

	unsigned AllocVertices(int num)
	{
		auto vindex = vertices.Reserve(num);
		indices.Push(num);
		return vindex;
	}

};

class DShape2D;
struct DShape2DBufferInfo;

enum class SpecialDrawCommand {
	NotSpecial,

	EnableStencil,
	SetStencil,
	ClearStencil,
};

class F2DDrawer
{
public:
	F2DDrawer() {
		this->transform.Identity();
	}

	enum EDrawType : uint8_t
	{
		DrawTypeTriangles,
		DrawTypeLines,
		DrawTypePoints,
		DrawTypeRotateSprite,
	};

	enum ETextureFlags : uint8_t
	{
		DTF_Wrap = 1,
		DTF_Scissor = 2,
        DTF_Burn = 4,
		DTF_Indexed = 8,
		DTF_ForceFilter = 16
	};


	// This vertex type is hardware independent and needs conversion when put into a buffer.
	struct TwoDVertex
	{
		float x, y, z;
		float u, v;
		PalEntry color0;

		void Set(float xx, float yy, float zz)
		{
			x = xx;
			z = zz;
			y = yy;
			u = 0;
			v = 0;
			color0 = 0;
		}

		void Set(double xx, double yy, double zz, double uu, double vv, PalEntry col)
		{
			x = (float)xx;
			z = (float)zz;
			y = (float)yy;
			u = (float)uu;
			v = (float)vv;
			color0 = col;
		}

	};

	struct RenderCommand
	{
		SpecialDrawCommand isSpecial;

		bool stencilOn;

		int stencilOffs;
		int stencilOp;
		int stencilFlags;

		EDrawType mType;
		int mVertIndex;
		int mVertCount;
		int mIndexIndex;
		int mIndexCount;

		FGameTexture *mTexture;
		FTranslationID mTranslationId;
		PalEntry mSpecialColormap[2];
		int mScissor[4];
		int mDesaturate;
		FRenderStyle mRenderStyle;
		PalEntry mColor1;	// Overlay color
		ETexMode mDrawMode;
		uint8_t mLightLevel;
		uint8_t mFlags;
		//When a render command should run in VR on the whole screen, not just center qued used for 2D rendering. Used e.g. for nightvision
		bool mOutside2D = false;
		float mScreenFade;

		bool useTransform;
		DMatrix3x3 transform;

		RefCountedPtr<DShape2DBufferInfo> shape2DBufInfo;
		int shape2DBufIndex;
		int shape2DIndexCount;
		int shape2DCommandCounter;

		RenderCommand()
		{
			memset((void*)this, 0,  sizeof(*this));
		}

		// If these fields match, two draw commands can be batched.
		bool isCompatible(const RenderCommand &other) const
		{
			if (
				isSpecial != SpecialDrawCommand::NotSpecial ||
				other.isSpecial != SpecialDrawCommand::NotSpecial
			) return false;
			if (shape2DBufInfo != nullptr || other.shape2DBufInfo != nullptr) return false;
			return mTexture == other.mTexture &&
				mType == other.mType &&
				mTranslationId == other.mTranslationId &&
				mSpecialColormap[0].d == other.mSpecialColormap[0].d &&
				mSpecialColormap[1].d == other.mSpecialColormap[1].d &&
				!memcmp(mScissor, other.mScissor, sizeof(mScissor)) &&
				mDesaturate == other.mDesaturate &&
				mRenderStyle == other.mRenderStyle &&
				mDrawMode == other.mDrawMode &&
				mFlags == other.mFlags &&
				mLightLevel == other.mLightLevel &&
				mColor1.d == other.mColor1.d &&
				useTransform == other.useTransform &&
				mOutside2D == other.mOutside2D &&
				mScreenFade == other.mScreenFade &&
				(
					!useTransform ||
					(
						transform[0] == other.transform[0] &&
						transform[1] == other.transform[1] &&
						transform[2] == other.transform[2]
					)
				);
		}
	};

	TArray<int> mIndices;
	TArray<TwoDVertex> mVertices;
	TArray<RenderCommand> mData;
	int Width, Height;
	bool isIn2D = false;
	bool locked = false;	// prevents clearing of the data so it can be reused multiple times (useful for screen fades)
	float screenFade = 1.f;
	DVector2 offset;
	DMatrix3x3 transform;
public:
	int fullscreenautoaspect = 3;
	int cliptop = -1, clipleft = -1, clipwidth = -1, clipheight = -1;

	int AddCommand(RenderCommand *data);
	void AddIndices(int firstvert, int count, ...);
private:
	void AddIndices(int firstvert, TArray<int> &v);
	bool SetStyle(FGameTexture *tex, DrawParms &parms, PalEntry &color0, RenderCommand &quad);
	void SetColorOverlay(PalEntry color, float alpha, PalEntry &vertexcolor, PalEntry &overlaycolor);

public:
	float GetClassicFlatScalarWidth();
	float GetClassicFlatScalarHeight();
	void AddTexture(FGameTexture* img, DrawParms& parms);
	void AddShape(FGameTexture *img, DShape2D *shape, DrawParms &parms);
	void AddPoly(FGameTexture *texture, FVector2 *points, int npoints,
		double originx, double originy, double scalex, double scaley,
		DAngle rotation, const FColormap &colormap, PalEntry flatcolor, double lightlevel, uint32_t *indices, size_t indexcount);
	void AddPoly(FGameTexture* img, FVector4 *vt, size_t vtcount, const unsigned int *ind, size_t idxcount, FTranslationID translation, PalEntry color, FRenderStyle style, const IntRect* clip);
	void FillPolygon(int* rx1, int* ry1, int* xb1, int32_t npoints, int pic, int palette, int shade, int props, const FVector2& xtex, const FVector2& ytex, const FVector2& otex,
		int clipx1, int clipy1, int clipx2, int clipy2);
	void AddFlatFill(int left, int top, int right, int bottom, FGameTexture *src, int local_origin = false, double flatscale = 1.0, PalEntry color = 0xffffffff, ERenderStyle rs = STYLE_Normal);

	void AddColorOnlyQuad(int left, int top, int width, int height, PalEntry color, FRenderStyle *style = nullptr, bool prepend = false, bool outside2D = false);
	void ClearScreen(PalEntry color = 0xff000000);
	void AddDim(PalEntry color, float damount, int x1, int y1, int w, int h);
	void AddClear(int left, int top, int right, int bottom, int palcolor, uint32_t color);


	void AddLine(const DVector2& v1, const DVector2& v2, const IntRect* clip, uint32_t color, uint8_t alpha = 255);
	void AddThickLine(const DVector2& v1, const DVector2& v2, double thickness, uint32_t color, uint8_t alpha = 255);
	void AddPixel(int x1, int y1, uint32_t color);

	void AddEnableStencil(bool on);
	void AddSetStencil(int offs, int op, int flags);
	void AddClearStencil();

	void Clear();
	void Lock() { locked = true; }
	void SetScreenFade(float factor) { screenFade = factor; }
	void Unlock() { locked = false; }
	int GetWidth() const { return Width; }
	int GetHeight() const { return Height; }
	void SetSize(int w, int h) { Width = w; Height = h; }
	void Begin(int w, int h) { isIn2D = true; Width = w; Height = h; }
	void End() { isIn2D = false; }
	bool HasBegun2D() { return isIn2D; }
	void OnFrameDone();

	void ClearClipRect() { clipleft = cliptop = 0; clipwidth = clipheight = -1; }
	void SetClipRect(int x, int y, int w, int h);
	void GetClipRect(int* x, int* y, int* w, int* h);

	DVector2 SetOffset(const DVector2& vec)
	{
		auto v = offset;
		offset = vec;
		return v;
	}

	void SetTransform(const DShape2DTransform& transform)
	{
		this->transform = transform.transform;
	}
	void ClearTransform()
	{
		this->transform.Identity();
	}

	int DrawCount() const
	{
		return mData.Size();
	}

	bool mIsFirstPass = true;
};

// DCanvas is already taken so using FCanvas instead.
class FCanvas : public DObject
{
	DECLARE_CLASS(FCanvas, DObject)
public:
	F2DDrawer Drawer;
	FCanvasTexture* Tex = nullptr;
};

struct DShape2DBufferInfo : RefCountedBase
{
	TArray<F2DVertexBuffer> buffers;
	bool needsVertexUpload = true;
	int bufIndex = -1;
	int lastCommand = -1;
	bool uploadedOnce = false;
};

class DShape2D : public DObject
{

	DECLARE_CLASS(DShape2D,DObject)
public:
	DShape2D()
	: bufferInfo(new DShape2DBufferInfo)
	{
		transform.Identity();
	}

	TArray<int> mIndices;
	TArray<DVector2> mVertices;
	TArray<DVector2> mCoords;

	double minx = 0.0;
	double maxx = 0.0;
	double miny = 0.0;
	double maxy = 0.0;

	DMatrix3x3 transform;

	RefCountedPtr<DShape2DBufferInfo> bufferInfo;

	DrawParms* lastParms = nullptr;

	void OnDestroy() override;
};


//===========================================================================
// 
// Vertex buffer for 2D drawer
//
//===========================================================================

class F2DVertexBuffer
{
	IVertexBuffer *mVertexBuffer;
	IIndexBuffer *mIndexBuffer;


public:

	F2DVertexBuffer();

	~F2DVertexBuffer()
	{
		delete mIndexBuffer;
		delete mVertexBuffer;
	}

	void UploadData(F2DDrawer::TwoDVertex *vertices, int vertcount, int *indices, int indexcount)
	{
		mVertexBuffer->SetData(vertcount * sizeof(*vertices), vertices, BufferUsageType::Stream);
		mIndexBuffer->SetData(indexcount * sizeof(unsigned int), indices, BufferUsageType::Stream);
	}

	std::pair<IVertexBuffer *, IIndexBuffer *> GetBufferObjects() const
	{
		return std::make_pair(mVertexBuffer, mIndexBuffer);
	}
};


#endif
