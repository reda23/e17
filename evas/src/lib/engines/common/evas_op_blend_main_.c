#include "evas_common.h"

RGBA_Gfx_Func     op_blend_span_funcs[SP_LAST][SM_LAST][SC_LAST][DP_LAST][CPU_LAST];
RGBA_Gfx_Pt_Func  op_blend_pt_funcs[SP_LAST][SM_LAST][SC_LAST][DP_LAST][CPU_LAST];

static void op_blend_init(void);
static void op_blend_shutdown(void);

static RGBA_Gfx_Func op_blend_pixel_span_get(RGBA_Image *src, RGBA_Image *dst, int pixels);
static RGBA_Gfx_Func op_blend_color_span_get(DATA32 col, RGBA_Image *dst, int pixels);
static RGBA_Gfx_Func op_blend_pixel_color_span_get(RGBA_Image *src, DATA32 col, RGBA_Image *dst, int pixels);
static RGBA_Gfx_Func op_blend_mask_color_span_get(DATA32 col, RGBA_Image *dst, int pixels);
static RGBA_Gfx_Func op_blend_pixel_mask_span_get(RGBA_Image *src, RGBA_Image *dst, int pixels);

static RGBA_Gfx_Pt_Func op_blend_pixel_pt_get(Image_Entry_Flags src_flags, RGBA_Image *dst);
static RGBA_Gfx_Pt_Func op_blend_color_pt_get(DATA32 col, RGBA_Image *dst);
static RGBA_Gfx_Pt_Func op_blend_pixel_color_pt_get(Image_Entry_Flags src_flags, DATA32 col, RGBA_Image *dst);
static RGBA_Gfx_Pt_Func op_blend_mask_color_pt_get(DATA32 col, RGBA_Image *dst);
static RGBA_Gfx_Pt_Func op_blend_pixel_mask_pt_get(Image_Entry_Flags src_flags, RGBA_Image *dst);

static RGBA_Gfx_Compositor  _composite_blend = { "blend",
 op_blend_init, op_blend_shutdown,
 op_blend_pixel_span_get, op_blend_color_span_get,
 op_blend_pixel_color_span_get, op_blend_mask_color_span_get,
 op_blend_pixel_mask_span_get,
 op_blend_pixel_pt_get, op_blend_color_pt_get,
 op_blend_pixel_color_pt_get, op_blend_mask_color_pt_get,
 op_blend_pixel_mask_pt_get
 };

RGBA_Gfx_Compositor  *
evas_common_gfx_compositor_blend_get(void)
{
   return &(_composite_blend);
}


RGBA_Gfx_Func     op_blend_rel_span_funcs[SP_LAST][SM_LAST][SC_LAST][DP_LAST][CPU_LAST];
RGBA_Gfx_Pt_Func  op_blend_rel_pt_funcs[SP_LAST][SM_LAST][SC_LAST][DP_LAST][CPU_LAST];

static void op_blend_rel_init(void);
static void op_blend_rel_shutdown(void);

static RGBA_Gfx_Func op_blend_rel_pixel_span_get(RGBA_Image *src, RGBA_Image *dst, int pixels);
static RGBA_Gfx_Func op_blend_rel_color_span_get(DATA32 col, RGBA_Image *dst, int pixels);
static RGBA_Gfx_Func op_blend_rel_pixel_color_span_get(RGBA_Image *src, DATA32 col, RGBA_Image *dst, int pixels);
static RGBA_Gfx_Func op_blend_rel_mask_color_span_get(DATA32 col, RGBA_Image *dst, int pixels);
static RGBA_Gfx_Func op_blend_rel_pixel_mask_span_get(RGBA_Image *src, RGBA_Image *dst, int pixels);

static RGBA_Gfx_Pt_Func op_blend_rel_pixel_pt_get(Image_Entry_Flags src_flags, RGBA_Image *dst);
static RGBA_Gfx_Pt_Func op_blend_rel_color_pt_get(DATA32 col, RGBA_Image *dst);
static RGBA_Gfx_Pt_Func op_blend_rel_pixel_color_pt_get(Image_Entry_Flags src_flags, DATA32 col, RGBA_Image *dst);
static RGBA_Gfx_Pt_Func op_blend_rel_mask_color_pt_get(DATA32 col, RGBA_Image *dst);
static RGBA_Gfx_Pt_Func op_blend_rel_pixel_mask_pt_get(Image_Entry_Flags src_flags, RGBA_Image *dst);

static RGBA_Gfx_Compositor  _composite_blend_rel = { "blend_rel",
 op_blend_rel_init, op_blend_rel_shutdown,
 op_blend_rel_pixel_span_get, op_blend_rel_color_span_get,
 op_blend_rel_pixel_color_span_get, op_blend_rel_mask_color_span_get,
 op_blend_rel_pixel_mask_span_get,
 op_blend_rel_pixel_pt_get, op_blend_rel_color_pt_get,
 op_blend_rel_pixel_color_pt_get, op_blend_rel_mask_color_pt_get,
 op_blend_rel_pixel_mask_pt_get
 };

RGBA_Gfx_Compositor  *
evas_common_gfx_compositor_blend_rel_get(void)
{
   return &(_composite_blend_rel);
}


# include "./evas_op_blend/op_blend_pixel_.c"
# include "./evas_op_blend/op_blend_color_.c"
# include "./evas_op_blend/op_blend_pixel_color_.c"
# include "./evas_op_blend/op_blend_pixel_mask_.c"
# include "./evas_op_blend/op_blend_mask_color_.c"
//# include "./evas_op_blend/op_blend_pixel_mask_color_.c"

# include "./evas_op_blend/op_blend_pixel_i386.c"
# include "./evas_op_blend/op_blend_color_i386.c"
# include "./evas_op_blend/op_blend_pixel_color_i386.c"
# include "./evas_op_blend/op_blend_pixel_mask_i386.c"
# include "./evas_op_blend/op_blend_mask_color_i386.c"
//# include "./evas_op_blend/op_blend_pixel_mask_color_i386.c"

# include "./evas_op_blend/op_blend_pixel_neon.c"
# include "./evas_op_blend/op_blend_color_neon.c"
# include "./evas_op_blend/op_blend_pixel_color_neon.c"
# include "./evas_op_blend/op_blend_pixel_mask_neon.c"
# include "./evas_op_blend/op_blend_mask_color_neon.c"
//# include "./evas_op_blend/op_blend_pixel_mask_color_neon.c"

#ifdef BUILD_SSE3
void evas_common_op_blend_init_sse3(void);
#endif

static void
op_blend_init(void)
{
   memset(op_blend_span_funcs, 0, sizeof(op_blend_span_funcs));
   memset(op_blend_pt_funcs, 0, sizeof(op_blend_pt_funcs));
#ifdef BUILD_SSE3
   evas_common_op_blend_init_sse3();
#endif
#ifdef BUILD_MMX
   init_blend_pixel_span_funcs_mmx();
   init_blend_pixel_color_span_funcs_mmx();
   init_blend_pixel_mask_span_funcs_mmx();
   init_blend_color_span_funcs_mmx();
   init_blend_mask_color_span_funcs_mmx();

   init_blend_pixel_pt_funcs_mmx();
   init_blend_pixel_color_pt_funcs_mmx();
   init_blend_pixel_mask_pt_funcs_mmx();
   init_blend_color_pt_funcs_mmx();
   init_blend_mask_color_pt_funcs_mmx();
#endif
#ifdef BUILD_NEON
   init_blend_pixel_span_funcs_neon();
   init_blend_pixel_color_span_funcs_neon();
   init_blend_pixel_mask_span_funcs_neon();
   init_blend_color_span_funcs_neon();
   init_blend_mask_color_span_funcs_neon();

   init_blend_pixel_pt_funcs_neon();
   init_blend_pixel_color_pt_funcs_neon();
   init_blend_pixel_mask_pt_funcs_neon();
   init_blend_color_pt_funcs_neon();
   init_blend_mask_color_pt_funcs_neon();
#endif
#ifdef BUILD_C
   init_blend_pixel_span_funcs_c();
   init_blend_pixel_color_span_funcs_c();
   init_blend_pixel_mask_span_funcs_c();
   init_blend_color_span_funcs_c();
   init_blend_mask_color_span_funcs_c();

   init_blend_pixel_pt_funcs_c();
   init_blend_pixel_color_pt_funcs_c();
   init_blend_pixel_mask_pt_funcs_c();
   init_blend_color_pt_funcs_c();
   init_blend_mask_color_pt_funcs_c();
#endif
}

static void
op_blend_shutdown(void)
{
}

static RGBA_Gfx_Func
blend_gfx_span_func_cpu(int s, int m, int c, int d)
{
   RGBA_Gfx_Func func = NULL;
   int cpu = CPU_N;
#ifdef BUILD_SSE3
   if (evas_common_cpu_has_feature(CPU_FEATURE_SSE3))
      {
         cpu = CPU_SSE3;
         func = op_blend_span_funcs[s][m][c][d][cpu];
         if(func) return func;
      }
#endif
#ifdef BUILD_MMX
   if (evas_common_cpu_has_feature(CPU_FEATURE_MMX))
     {
	cpu = CPU_MMX;
	func = op_blend_span_funcs[s][m][c][d][cpu];
	if (func) return func;
     }
#endif
#ifdef BUILD_NEON
   if (evas_common_cpu_has_feature(CPU_FEATURE_NEON))
     {
	cpu = CPU_NEON;
	func = op_blend_span_funcs[s][m][c][d][cpu];
	if (func) return func;
     }
#endif
#ifdef BUILD_C
   cpu = CPU_C;
   func = op_blend_span_funcs[s][m][c][d][cpu];
   if (func) return func;
#endif
   return func;
}

static RGBA_Gfx_Func
op_blend_pixel_span_get(RGBA_Image *src, RGBA_Image *dst, int pixels __UNUSED__)
{
   int  s = SP_AN, m = SM_N, c = SC_N, d = DP_AN;

   if (src && src->cache_entry.flags.alpha)
     {
	s = SP;
	if (src->cache_entry.flags.alpha_sparse)
	    s = SP_AS;
     }
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_gfx_span_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Func
op_blend_color_span_get(DATA32 col, RGBA_Image *dst, int pixels __UNUSED__)
{
   int  s = SP_N, m = SM_N, c = SC_AN, d = DP_AN;

   if ((col >> 24) < 255)
	c = SC;
   if (col == ((col >> 24) * 0x01010101))
	c = SC_AA;
   if (col == 0xffffffff)
	c = SC_N;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_gfx_span_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Func
op_blend_pixel_color_span_get(RGBA_Image *src, DATA32 col, RGBA_Image *dst, int pixels __UNUSED__)
{
   int  s = SP_AN, m = SM_N, c = SC_AN, d = DP_AN;

   if (src && src->cache_entry.flags.alpha)
     {
	s = SP;
	if (src->cache_entry.flags.alpha_sparse)
	    s = SP_AS;
     }
   if ((col >> 24) < 255)
	c = SC;
   if (col == ((col >> 24) * 0x01010101))
	c = SC_AA;
   if (col == 0xffffffff)
	c = SC_N;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_gfx_span_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Func
op_blend_mask_color_span_get(DATA32 col, RGBA_Image *dst, int pixels __UNUSED__)
{
   int  s = SP_N, m = SM_AS, c = SC_AN, d = DP_AN;

   if ((col >> 24) < 255)
	c = SC;
   if (col == ((col >> 24) * 0x01010101))
	c = SC_AA;
   if (col == 0xffffffff)
	c = SC_N;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_gfx_span_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Func
op_blend_pixel_mask_span_get(RGBA_Image *src, RGBA_Image *dst, int pixels __UNUSED__)
{
   int  s = SP_AN, m = SM_AS, c = SC_N, d = DP_AN;

   if (src && src->cache_entry.flags.alpha)
     {
	s = SP;
	if (src->cache_entry.flags.alpha_sparse)
	    s = SP_AS;
     }
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_gfx_span_func_cpu(s, m, c, d);
}


static RGBA_Gfx_Pt_Func
blend_gfx_pt_func_cpu(int s, int m, int c, int d)
{
   RGBA_Gfx_Pt_Func func = NULL;
   int cpu = CPU_N;
#ifdef BUILD_SSE3
   if(evas_common_cpu_has_feature(CPU_FEATURE_SSE3))
      {
         cpu = CPU_SSE3;
         func = op_blend_pt_funcs[s][m][c][d][cpu];
         if(func) return func;
      }
#endif
#ifdef BUILD_MMX
   if (evas_common_cpu_has_feature(CPU_FEATURE_MMX))
     {
	cpu = CPU_MMX;
	func = op_blend_pt_funcs[s][m][c][d][cpu];
	if (func) return func;
     }
#endif
#ifdef BUILD_NEON
   if (evas_common_cpu_has_feature(CPU_FEATURE_NEON))
     {
	cpu = CPU_NEON;
	func = op_blend_pt_funcs[s][m][c][d][cpu];
	if (func) return func;
     }
#endif
#ifdef BUILD_C
   cpu = CPU_C;
   func = op_blend_pt_funcs[s][m][c][d][cpu];
   if (func) return func;
#endif
   return func;
}

static RGBA_Gfx_Pt_Func
op_blend_pixel_pt_get(Image_Entry_Flags src_flags, RGBA_Image *dst)
{
   int  s = SP_AN, m = SM_N, c = SC_N, d = DP_AN;

   if (src_flags.alpha)
	s = SP;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_gfx_pt_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Pt_Func
op_blend_color_pt_get(DATA32 col, RGBA_Image *dst)
{
   int  s = SP_N, m = SM_N, c = SC_AN, d = DP_AN;

   if ((col >> 24) < 255)
	c = SC;
   if (col == ((col >> 24) * 0x01010101))
	c = SC_AA;
   if (col == 0xffffffff)
	c = SC_N;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_gfx_pt_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Pt_Func
op_blend_pixel_color_pt_get(Image_Entry_Flags src_flags, DATA32 col, RGBA_Image *dst)
{
   int  s = SP_AN, m = SM_N, c = SC_AN, d = DP_AN;

   if (src_flags.alpha)
	s = SP;
   if ((col >> 24) < 255)
	c = SC;
   if (col == ((col >> 24) * 0x01010101))
	c = SC_AA;
   if (col == 0xffffffff)
	c = SC_N;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_gfx_pt_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Pt_Func
op_blend_mask_color_pt_get(DATA32 col, RGBA_Image *dst)
{
   int  s = SP_N, m = SM_AS, c = SC_AN, d = DP_AN;

   if ((col >> 24) < 255)
	c = SC;
   if (col == ((col >> 24) * 0x01010101))
	c = SC_AA;
   if (col == 0xffffffff)
	c = SC_N;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_gfx_pt_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Pt_Func
op_blend_pixel_mask_pt_get(Image_Entry_Flags src_flags, RGBA_Image *dst)
{
   int  s = SP_AN, m = SM_AS, c = SC_N, d = DP_AN;

   if (src_flags.alpha)
	s = SP;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_gfx_pt_func_cpu(s, m, c, d);
}

void evas_common_op_blend_rel_init_sse3(void);

static void
op_blend_rel_init(void)
{
   memset(op_blend_rel_span_funcs, 0, sizeof(op_blend_rel_span_funcs));
   memset(op_blend_rel_pt_funcs, 0, sizeof(op_blend_rel_pt_funcs));
#ifdef BUILD_SSE3
   evas_common_op_blend_rel_init_sse3();
#endif
#ifdef BUILD_MMX
   init_blend_rel_pixel_span_funcs_mmx();
   init_blend_rel_pixel_color_span_funcs_mmx();
   init_blend_rel_pixel_mask_span_funcs_mmx();
   init_blend_rel_color_span_funcs_mmx();
   init_blend_rel_mask_color_span_funcs_mmx();

   init_blend_rel_pixel_pt_funcs_mmx();
   init_blend_rel_pixel_color_pt_funcs_mmx();
   init_blend_rel_pixel_mask_pt_funcs_mmx();
   init_blend_rel_color_pt_funcs_mmx();
   init_blend_rel_mask_color_pt_funcs_mmx();
#endif
#ifdef BUILD_NEON
   init_blend_rel_pixel_span_funcs_neon();
   init_blend_rel_pixel_color_span_funcs_neon();
   init_blend_rel_pixel_mask_span_funcs_neon();
   init_blend_rel_color_span_funcs_neon();
   init_blend_rel_mask_color_span_funcs_neon();

   init_blend_rel_pixel_pt_funcs_neon();
   init_blend_rel_pixel_color_pt_funcs_neon();
   init_blend_rel_pixel_mask_pt_funcs_neon();
   init_blend_rel_color_pt_funcs_neon();
   init_blend_rel_mask_color_pt_funcs_neon();
#endif
#ifdef BUILD_C
   init_blend_rel_pixel_span_funcs_c();
   init_blend_rel_pixel_color_span_funcs_c();
   init_blend_rel_pixel_mask_span_funcs_c();
   init_blend_rel_color_span_funcs_c();
   init_blend_rel_mask_color_span_funcs_c();

   init_blend_rel_pixel_pt_funcs_c();
   init_blend_rel_pixel_color_pt_funcs_c();
   init_blend_rel_pixel_mask_pt_funcs_c();
   init_blend_rel_color_pt_funcs_c();
   init_blend_rel_mask_color_pt_funcs_c();
#endif
}

static void
op_blend_rel_shutdown(void)
{
}

static RGBA_Gfx_Func
blend_rel_gfx_span_func_cpu(int s, int m, int c, int d)
{
   RGBA_Gfx_Func func = NULL;
   int cpu = CPU_N;
#ifdef BUILD_SSE3
   if (evas_common_cpu_has_feature(CPU_FEATURE_SSE3))
      {
         cpu = CPU_SSE3;
         func = op_blend_rel_span_funcs[s][m][c][d][cpu];
         if(func) return func;
      }
#endif
#ifdef BUILD_MMX
   if (evas_common_cpu_has_feature(CPU_FEATURE_MMX))
     {
	cpu = CPU_MMX;
	func = op_blend_rel_span_funcs[s][m][c][d][cpu];
	if (func) return func;
     }
#endif
#ifdef BUILD_NEON
   if (evas_common_cpu_has_feature(CPU_FEATURE_NEON))
     {
	cpu = CPU_NEON;
	func = op_blend_rel_span_funcs[s][m][c][d][cpu];
	if (func) return func;
     }
#endif
#ifdef BUILD_C
   cpu = CPU_C;
   func = op_blend_rel_span_funcs[s][m][c][d][cpu];
   if (func) return func;
#endif
   return func;
}

static RGBA_Gfx_Func
op_blend_rel_pixel_span_get(RGBA_Image *src, RGBA_Image *dst, int pixels __UNUSED__)
{
   int  s = SP_AN, m = SM_N, c = SC_N, d = DP_AN;

   if (src && src->cache_entry.flags.alpha)
     {
	s = SP;
	if (src->cache_entry.flags.alpha_sparse)
	    s = SP_AS;
     }
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_rel_gfx_span_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Func
op_blend_rel_color_span_get(DATA32 col, RGBA_Image *dst, int pixels __UNUSED__)
{
   int  s = SP_N, m = SM_N, c = SC_AN, d = DP_AN;

   if ((col >> 24) < 255)
	c = SC;
   if (col == ((col >> 24) * 0x01010101))
	c = SC_AA;
   if (col == 0xffffffff)
	c = SC_N;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_rel_gfx_span_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Func
op_blend_rel_pixel_color_span_get(RGBA_Image *src, DATA32 col, RGBA_Image *dst, int pixels __UNUSED__)
{
   int  s = SP_AN, m = SM_N, c = SC_AN, d = DP_AN;

   if (src && src->cache_entry.flags.alpha)
	s = SP;
   if ((col >> 24) < 255)
	c = SC;
   if (col == ((col >> 24) * 0x01010101))
	c = SC_AA;
   if (col == 0xffffffff)
	c = SC_N;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_rel_gfx_span_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Func
op_blend_rel_mask_color_span_get(DATA32 col, RGBA_Image *dst, int pixels __UNUSED__)
{
   int  s = SP_N, m = SM_AS, c = SC_AN, d = DP_AN;

   if ((col >> 24) < 255)
	c = SC;
   if (col == ((col >> 24) * 0x01010101))
	c = SC_AA;
   if (col == 0xffffffff)
	c = SC_N;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_rel_gfx_span_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Func
op_blend_rel_pixel_mask_span_get(RGBA_Image *src, RGBA_Image *dst, int pixels __UNUSED__)
{
   int  s = SP_AN, m = SM_AS, c = SC_N, d = DP_AN;

   if (src && src->cache_entry.flags.alpha)
     {
	s = SP;
	if (src->cache_entry.flags.alpha_sparse)
	    s = SP_AS;
     }
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_rel_gfx_span_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Pt_Func
blend_rel_gfx_pt_func_cpu(int s, int m, int c, int d)
{
   RGBA_Gfx_Pt_Func func = NULL;
   int cpu = CPU_N;
#ifdef BUILD_SSE3
   if (evas_common_cpu_has_feature(CPU_FEATURE_SSE3))
      {
         cpu = CPU_SSE3;
         func = op_blend_rel_pt_funcs[s][m][c][d][cpu];
         if(func) return func;
      }
#endif
#ifdef BUILD_MMX
   if (evas_common_cpu_has_feature(CPU_FEATURE_MMX))
     {
	cpu = CPU_MMX;
	func = op_blend_rel_pt_funcs[s][m][c][d][cpu];
	if (func) return func;
     }
#endif
#ifdef BUILD_NEON
   if (evas_common_cpu_has_feature(CPU_FEATURE_NEON))
     {
	cpu = CPU_NEON;
	func = op_blend_rel_pt_funcs[s][m][c][d][cpu];
	if (func) return func;
     }
#endif
#ifdef BUILD_C
   cpu = CPU_C;
   func = op_blend_rel_pt_funcs[s][m][c][d][cpu];
   if (func) return func;
#endif
   return func;
}

static RGBA_Gfx_Pt_Func
op_blend_rel_pixel_pt_get(Image_Entry_Flags src_flags, RGBA_Image *dst)
{
   int  s = SP_AN, m = SM_N, c = SC_N, d = DP_AN;

   if (src_flags.alpha)
	s = SP;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_rel_gfx_pt_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Pt_Func
op_blend_rel_color_pt_get(DATA32 col, RGBA_Image *dst)
{
   int  s = SP_N, m = SM_N, c = SC_AN, d = DP_AN;

   if ((col >> 24) < 255)
	c = SC;
   if (col == ((col >> 24) * 0x01010101))
	c = SC_AA;
   if (col == 0xffffffff)
	c = SC_N;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_rel_gfx_pt_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Pt_Func
op_blend_rel_pixel_color_pt_get(Image_Entry_Flags src_flags, DATA32 col, RGBA_Image *dst)
{
   int  s = SP_AN, m = SM_N, c = SC_AN, d = DP_AN;

   if (src_flags.alpha)
	s = SP;
   if ((col >> 24) < 255)
	c = SC;
   if (col == ((col >> 24) * 0x01010101))
	c = SC_AA;
   if (col == 0xffffffff)
	c = SC_N;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_rel_gfx_pt_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Pt_Func
op_blend_rel_mask_color_pt_get(DATA32 col, RGBA_Image *dst)
{
   int  s = SP_N, m = SM_AS, c = SC_AN, d = DP_AN;

   if ((col >> 24) < 255)
	c = SC;
   if (col == ((col >> 24) * 0x01010101))
	c = SC_AA;
   if (col == 0xffffffff)
	c = SC_N;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_rel_gfx_pt_func_cpu(s, m, c, d);
}

static RGBA_Gfx_Pt_Func
op_blend_rel_pixel_mask_pt_get(Image_Entry_Flags src_flags, RGBA_Image *dst)
{
   int  s = SP_AN, m = SM_AS, c = SC_N, d = DP_AN;

   if (src_flags.alpha)
	s = SP;
   if (dst && dst->cache_entry.flags.alpha)
	d = DP;
   return blend_rel_gfx_pt_func_cpu(s, m, c, d);
}
