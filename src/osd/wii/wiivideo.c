//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================

#include "osdcore.h"
#include "wiimisc.h"
#include <ogcsys.h>
#include <malloc.h>
#include <unistd.h>
#include "render.h"
#include "mame.h"


typedef struct _gx_tex gx_tex;
struct _gx_tex
{
	u32 size;
	u8 format;
	gx_tex *next;
	void *addr;
	void *data;
};

static GXRModeObj *vmode;
static GXTexObj texObj;
static GXTexObj blankTex;

static unsigned char *xfb[2];
static int currfb;
static unsigned char *gp_fifo;

static int screen_width;
static int screen_height;
static int hofs;
static int vofs;

static gx_tex *firstTex = NULL;
static gx_tex *lastTex = NULL;
static gx_tex *firstScreenTex = NULL;
static gx_tex *lastScreenTex = NULL;

static const render_primitive_list *currlist = NULL;
static lwp_t vidthread = LWP_THREAD_NULL;
static BOOL wii_stopping = FALSE;

static unsigned char blanktex[2*16] __attribute__((aligned(32))) = {
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

#define DEFAULT_FIFO_SIZE	(256*1024)

int wii_render_width()
{
	return screen_width;
}

int wii_render_height()
{
	return screen_height;
}

void wii_init_video()
{
	u32 xfbHeight;
	f32 yscale;
	Mtx44 perspective;
	Mtx GXmodelView2D;
	GXColor background = {0, 0, 0, 0xff};

	currfb = 0;

	VIDEO_Init();
	VIDEO_SetBlack(true);
	vmode = VIDEO_GetPreferredMode(NULL);

	switch (vmode->viTVMode >> 2)
	{
		case VI_PAL:
			vmode = &TVPal574IntDfScale;
			vmode->xfbHeight = 480;
			vmode->viYOrigin = (VI_MAX_HEIGHT_PAL - 480)/2;
			vmode->viHeight = 480;
			break;

		case VI_NTSC:
			break;

		default:
			break;
	}

	VIDEO_Configure(vmode);

	xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
	xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));

	VIDEO_ClearFrameBuffer(vmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(vmode, xfb[1], COLOR_BLACK);

	VIDEO_SetNextFramebuffer(xfb[currfb]);

	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (vmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
	else while (VIDEO_GetNextField()) VIDEO_WaitVSync();

	gp_fifo = memalign(32, DEFAULT_FIFO_SIZE);
	memset(gp_fifo, 0, DEFAULT_FIFO_SIZE);
	GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);

	GX_SetCopyClear(background, 0x00ffffff);
 
	// other gx setup
	GX_SetViewport(0,0,vmode->fbWidth,vmode->efbHeight,0,1);
	yscale = GX_GetYScaleFactor(vmode->efbHeight,vmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0,0,vmode->fbWidth,vmode->efbHeight);
	GX_SetDispCopySrc(0,0,vmode->fbWidth,vmode->efbHeight);
	GX_SetDispCopyDst(vmode->fbWidth,xfbHeight);
	GX_SetCopyFilter(vmode->aa,vmode->sample_pattern,GX_TRUE,vmode->vfilter);
	GX_SetFieldMode(vmode->field_rendering,((vmode->viHeight==2*vmode->xfbHeight)?GX_ENABLE:GX_DISABLE));

	if (vmode->aa)
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	else
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	GX_SetCullMode(GX_CULL_NONE);
	GX_CopyDisp(xfb[currfb],GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetAlphaUpdate(GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);

	guOrtho(perspective,0,479,0,wii_screen_width()-1,0,300);
	GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

	guMtxIdentity(GXmodelView2D);
	guMtxTransApply (GXmodelView2D, GXmodelView2D, 0.0F, 0.0F, -5.0F);
	GX_LoadPosMtxImm(GXmodelView2D,GX_PNMTX0);

	GX_SetViewport(0,0,vmode->fbWidth,vmode->efbHeight,0,1);
	GX_InvVtxCache();
	GX_ClearVtxDesc();
	GX_InvalidateTexAll();

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

	VIDEO_SetBlack(false);

	GX_InitTexObj(&blankTex, blanktex, 1, 1, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
}

void wii_init_dimensions()
{
	screen_width = wii_screen_width();
	screen_height = 480;
	hofs = (screen_width - screen_width * options_get_float(mame_options(), "safearea")) / 2;
	vofs = (screen_height - screen_height * options_get_float(mame_options(), "safearea")) / 2;
	screen_width -= hofs * 2;
	screen_height -= vofs * 2;
}

/* adapted from rendersw.c, might not work because as far as I can tell, only 
   laserdisc uses YCbCr textures, and we don't support that be default */

static u32 yuy_rgb = 0;

inline u8 clamp16_shift8(u32 x)
{
	return (((s32) x < 0) ? 0 : (x > 65535 ? 255: x >> 8));
}

inline u16 GXGetRGBA5551_YUY16(u32 *src, u32 x, u8 i)
{
	if (!(i & 1))
	{
		u32 ycc = src[x];
		u8 y = ycc;
		u8 cb = ycc >> 8;
		u8 cr = ycc >> 16;
		u32 r, g, b, common;

		common = 298 * y - 56992;
		r = (common +            409 * cr);
		g = (common - 100 * cb - 208 * cr + 91776);
		b = (common + 516 * cb - 13696);

		yuy_rgb = MAKE_RGB(clamp16_shift8(r), clamp16_shift8(g), clamp16_shift8(b)) | 0xFF;

		return (yuy_rgb >> 16) & 0x0000FFFF;
	}
	else
	{
		return yuy_rgb & 0x0000FFFF;
	}
}

/* heavily adapted from Wii64 */

inline u16 GXGetRGBA5551_RGB5A3(u16 *src, u32 x)
{
	u16 c = src[x];
	if ((c&1) != 0)		c = 0x8000|(((c>>11)&0x1F)<<10)|(((c>>6)&0x1F)<<5)|((c>>1)&0x1F);   //opaque texel
	else				c = 0x0000|(((c>>12)&0xF)<<8)|(((c>>7)&0xF)<<4)|((c>>2)&0xF);   //transparent texel
	return (u32) c;
}

inline u16 GXGetRGBA8888_RGBA8(u32 *src, u32 x, u8 i)
{
	u32 c = src[x];
	u32 color = (i & 1) ? /* GGBB */ c & 0x0000FFFF : /* AARR */ (c >> 16) & 0x0000FFFF;
	return (u16) color;
}

inline u16 GXGetRGBA5551_PALETTE16(u16 *src, u32 x, int i, const rgb_t *palette)
{
	u16 c = src[x];
	u32 rgb = palette[c];
	if (i == TEXFORMAT_PALETTE16) return rgb_to_rgb15(rgb) | (1 << 15);
	else return (u32)(((RGB_RED(rgb) >> 4) << 8) | ((RGB_GREEN(rgb) >> 4) << 4) | ((RGB_BLUE(rgb) >> 4) << 0) | ((RGB_ALPHA(rgb) >> 5) << 12));
}

static gx_tex *create_texture(render_primitive *prim)
{
	int j, k, l, x, y, tx, ty, bpp;
	int flag = PRIMFLAG_GET_TEXFORMAT(prim->flags);
	int rawwidth = prim->texture.width;
	int rawheight = prim->texture.height;
	int width = ((rawwidth + 3) & (~3));
	int height = ((rawheight + 3) & (~3));
	u8 *data = prim->texture.base;
	u8 *src;
	u16 *fixed;
	gx_tex *newTex = malloc(sizeof(*newTex));

	memset(newTex, 0, sizeof(*newTex));

	j = 0;

	switch(flag)
	{
	case TEXFORMAT_ARGB32:
	case TEXFORMAT_RGB32:
		newTex->format = GX_TF_RGBA8;
		bpp = 4;
		fixed = (newTex->data) ? newTex->data : memalign(32, height * width * bpp);
		for (y = 0; y < height; y+=4)
		{
			for (x = 0; x < width; x+=4)
			{
				for (k = 0; k < 4; k++)
				{
					ty = y + k;
					src = &data[bpp * prim->texture.rowpixels * ty];
					for (l = 0; l < 4; l++)
					{
						tx = x + l;
						if (ty >= rawheight || tx >= rawwidth)
						{
							fixed[j] = 0x0000;
							fixed[j+16] = 0x0000;
						}
						else
						{
							fixed[j] =    GXGetRGBA8888_RGBA8((u32*) src, tx, 0);
							fixed[j+16] = GXGetRGBA8888_RGBA8((u32*) src, tx, 1);
						}
						j++;
					}
				}
				j += 16;
			}
		}
		break;
	case TEXFORMAT_PALETTE16:
	case TEXFORMAT_PALETTEA16:
		newTex->format = GX_TF_RGB5A3;
		bpp = 2;
		fixed = (newTex->data) ? newTex->data : memalign(32, height * width * bpp);
		for (y = 0; y < height; y+=4)
		{
			for (x = 0; x < width; x+=4)
			{
				for (k = 0; k < 4; k++)
				{
					ty = y + k;
					src = &data[bpp * prim->texture.rowpixels * ty];
					for (l = 0; l < 4; l++)
					{
						tx = x + l;
						if (ty >= rawheight || tx >= rawwidth)
							fixed[j++] = 0x0000;
						else
							fixed[j++] = GXGetRGBA5551_PALETTE16((u16*) src, tx, flag, prim->texture.palette);
					}
				}
			}
		}
		break;
	case TEXFORMAT_RGB15:
		newTex->format = GX_TF_RGB5A3;
		bpp = 2;
		fixed = (newTex->data) ? newTex->data : memalign(32, height * width * bpp);
		for (y = 0; y < height; y+=4)
		{
			for (x = 0; x < width; x+=4)
			{
				for (k = 0; k < 4; k++)
				{
					ty = y + k;
					src = &data[bpp * prim->texture.rowpixels * ty];
					for (l = 0; l < 4; l++)
					{
						tx = x + l;
						if (ty >= rawheight || tx >= rawwidth)
							fixed[j++] = 0x0000;
						else
							fixed[j++] = GXGetRGBA5551_RGB5A3((u16*) src, tx);
					}
				}
			}
		}
		break;
	case TEXFORMAT_YUY16:
		newTex->format = GX_TF_RGBA8;
		bpp = 4;
		fixed = (newTex->data) ? newTex->data : memalign(32, height * width * bpp);
		for (y = 0; y < height; y+=4)
		{
			for (x = 0; x < width; x+=4)
			{
				for (k = 0; k < 4; k++)
				{
					ty = y + k;
					src = &data[bpp * prim->texture.rowpixels * ty];
					for (l = 0; l < 4; l++)
					{
						tx = x + l;
						if (ty >= rawheight || tx >= rawwidth)
						{
							fixed[j] = 0x0000;
							fixed[j+16] = 0x0000;
						}
						else
						{
							fixed[j] =    GXGetRGBA5551_YUY16((u32*) src, tx, 0);
							fixed[j+16] = GXGetRGBA5551_YUY16((u32*) src, tx, 1);
						}
						j++;
					}
				}
				j += 16;
			}
		}
		break;
	default:
		return NULL;
	}

	newTex->size = height * width * bpp;
	newTex->data = fixed;
	newTex->addr = &(*data);

	if (PRIMFLAG_GET_SCREENTEX(prim->flags))
	{
		if (firstScreenTex == NULL)
			firstScreenTex = newTex;
		else
			lastScreenTex->next = newTex;

		lastScreenTex = newTex;
	}
	else
	{
		if (firstTex == NULL)
			firstTex = newTex;
		else
			lastTex->next = newTex;

		lastTex = newTex;
	}

	return newTex;
}

static gx_tex *get_texture(render_primitive *prim)
{
	gx_tex *t = firstTex;
	
	if (PRIMFLAG_GET_SCREENTEX(prim->flags))
		return create_texture(prim);

	while (t != NULL)
		if (t->addr == prim->texture.base)
			return t;
		else
			t = t->next;

	return create_texture(prim);
}

static void prep_texture(render_primitive *prim)
{
	gx_tex *newTex = get_texture(prim);

	if (newTex == NULL)
		return;

	DCFlushRange(newTex->data, newTex->size);
	GX_InitTexObj(&texObj, newTex->data, prim->texture.width, prim->texture.height, newTex->format, GX_CLAMP, GX_CLAMP, GX_FALSE);

	//if (PRIMFLAG_GET_SCREENTEX(prim->flags))
		GX_InitTexObjLOD(&texObj, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_DISABLE, GX_DISABLE, GX_ANISO_1);

	GX_LoadTexObj(&texObj, GX_TEXMAP0);
}

static void clearTexs()
{
	gx_tex *t = firstTex;
	gx_tex *n;
	
	while (t != NULL)
	{
		n = t->next;
		free(t->data);
		free(t);
		t = n;
	}
	
	firstTex = NULL;
	lastTex = NULL;
}

static void clearScreenTexs()
{
	gx_tex *t = firstScreenTex;
	gx_tex *n;
	
	while (t != NULL)
	{
		n = t->next;
		free(t->data);
		free(t);
		t = n;
	}
	
	firstScreenTex = NULL;
	lastScreenTex = NULL;
}

static void *wii_video_thread()
{
	while (!wii_stopping)
	{
		render_primitive *prim;

		if (currlist->lock == 0)
		{
			usleep(10);
			continue;
		}

		osd_lock_acquire(currlist->lock);

		for (prim = currlist->head; prim != NULL; prim = prim->next)
		{
			u8 r, g, b, a;
			r = (u8)(255.0f * prim->color.r);
			g = (u8)(255.0f * prim->color.g);
			b = (u8)(255.0f * prim->color.b);
			a = (u8)(255.0f * prim->color.a);

			switch (PRIMFLAG_GET_BLENDMODE(prim->flags))
			{
				case BLENDMODE_NONE:
					GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
					break;
				case BLENDMODE_ALPHA:
					GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
					break;
				case BLENDMODE_RGB_MULTIPLY:
					GX_SetBlendMode(GX_BM_SUBTRACT, GX_BL_SRCCLR, GX_BL_ZERO, GX_LO_CLEAR);
					break;
				case BLENDMODE_ADD:
					GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
					break;
			}

			switch (prim->type)
			{
				case RENDER_PRIMITIVE_LINE:
					GX_LoadTexObj(&blankTex, GX_TEXMAP0);
					GX_SetLineWidth((u8)(prim->width * 16.0f), GX_TO_ZERO);
					GX_Begin(GX_LINES, GX_VTXFMT0, 2);
						GX_Position2f32(prim->bounds.x0+hofs, prim->bounds.y0+vofs);
						GX_Color4u8(r, g, b, a);
						GX_TexCoord2f32(0, 0);
						GX_Position2f32(prim->bounds.x1+hofs, prim->bounds.y1+vofs);
						GX_Color4u8(r, g, b, a);
						GX_TexCoord2f32(0, 0);
					GX_End();
					break;
				case RENDER_PRIMITIVE_QUAD:
					if (prim->texture.base != NULL)
					{
						prep_texture(prim);
						GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
							GX_Position2f32(prim->bounds.x0+hofs, prim->bounds.y0+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(prim->texcoords.tl.u, prim->texcoords.tl.v);
							GX_Position2f32(prim->bounds.x0+hofs, prim->bounds.y1+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(prim->texcoords.bl.u, prim->texcoords.bl.v);
							GX_Position2f32(prim->bounds.x1+hofs, prim->bounds.y1+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(prim->texcoords.br.u, prim->texcoords.br.v);
							GX_Position2f32(prim->bounds.x1+hofs, prim->bounds.y0+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(prim->texcoords.tr.u, prim->texcoords.tr.v);
						GX_End();
					}
					else
					{
						GX_LoadTexObj(&blankTex, GX_TEXMAP0);
						GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
							GX_Position2f32(prim->bounds.x0+hofs, prim->bounds.y0+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(0, 0);
							GX_Position2f32(prim->bounds.x0+hofs, prim->bounds.y1+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(0, 0);
							GX_Position2f32(prim->bounds.x1+hofs, prim->bounds.y1+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(0, 0);
							GX_Position2f32(prim->bounds.x1+hofs, prim->bounds.y0+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(0, 0);
						GX_End();
					}
					break;
			}
		}

		osd_lock_release(currlist->lock);

		currfb ^= 1;

		GX_DrawDone();
		GX_CopyDisp(xfb[currfb],GX_TRUE);
		VIDEO_SetNextFramebuffer(xfb[currfb]);
		VIDEO_Flush();
		VIDEO_WaitVSync();

		clearScreenTexs();
	}
	
	return (void *)0;
}

void wii_video_render(const render_primitive_list *primlist)
{
	currlist = primlist;

	if (vidthread == LWP_THREAD_NULL)
		LWP_CreateThread(&vidthread, wii_video_thread, NULL, NULL, 0, 67);
}

void wii_shutdown_video()
{
	void *status;

	wii_stopping = TRUE;
	LWP_JoinThread(vidthread, &status);
	clearTexs();

	GX_AbortFrame();
	GX_Flush();

	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	VIDEO_SetNextFramebuffer(xfb[currfb^1]);
	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();
}
