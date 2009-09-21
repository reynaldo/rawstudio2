/*
 * Copyright (C) 2006-2009 Anders Brander <anders@brander.dk> and
 * Anders Kvist <akv@lnxbx.dk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* Plugin tmpl version 4 */

#include <rawstudio.h>
#include <math.h>
#if defined (__SSE2__)
#include <emmintrin.h>
#endif /* __SSE2__ */


#define RS_TYPE_RESAMPLE (rs_resample_type)
#define RS_RESAMPLE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_RESAMPLE, RSResample))
#define RS_RESAMPLE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_RESAMPLE, RSResampleClass))
#define RS_IS_RESAMPLE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_RESAMPLE))

typedef struct _RSResample RSResample;
typedef struct _RSResampleClass RSResampleClass;

struct _RSResample {
	RSFilter parent;

	gint target_width;
	gint target_height;
	gint new_width;
	gint new_height;
	gfloat scale;
	gboolean bounding_box;
};

struct _RSResampleClass {
	RSFilterClass parent_class;
};

typedef struct {
	RS_IMAGE16 *input;			/* Input Image to Resampler */
	RS_IMAGE16 *output;			/* Output Image from Resampler */
	guint old_size;				/* Old dimension in the direction of the resampler*/
	guint new_size;				/* New size in the direction of the resampler */
	guint dest_offset_other;	/* Where in the unchanged direction should we begin writing? */
	guint dest_end_other;		/* Where in the unchanged direction should we stop writing? */
	guint (*resample_support)();
	gdouble (*resample_func)(gdouble);
	GThread *threadid;
	gboolean use_compatible;	/* Use compatible resampler if pixelsize != 4 */
	gboolean use_fast;		/* Use nearest neighbour resampler, also compatible*/
} ResampleInfo;

RS_DEFINE_FILTER(rs_resample, RSResample)

enum {
	PROP_0,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_BOUNDING_BOX,
	PROP_SCALE
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void previous_changed(RSFilter *filter, RSFilter *parent, RSFilterChangedMask mask);
static RSFilterChangedMask recalculate_dimensions(RSResample *resample);
static RSFilterResponse *get_image(RSFilter *filter, const RSFilterParam *param);
static gint get_width(RSFilter *filter);
static gint get_height(RSFilter *filter);
static void ResizeH(ResampleInfo *info);
static void ResizeV(ResampleInfo *info);
static void ResizeV_SSE2(ResampleInfo *info);
static void ResizeH_compatible(ResampleInfo *info);
static void ResizeV_compatible(ResampleInfo *info);
static void ResizeH_fast(ResampleInfo *info);
static void ResizeV_fast(ResampleInfo *info);

static RSFilterClass *rs_resample_parent_class = NULL;
inline guint clampbits(gint x, guint n) { guint32 _y_temp; if( (_y_temp=x>>n) ) x = ~_y_temp >> (32-n); return x;}

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_resample_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_resample_class_init(RSResampleClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_resample_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_WIDTH, g_param_spec_int(
			"width", "width", "The width of the scaled image",
			6, 65535, 100, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_HEIGHT, g_param_spec_int(
			"height", "height", "The height of the scaled image",
			6, 65535, 100, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_BOUNDING_BOX, g_param_spec_boolean(
			"bounding-box", "bounding-box", "Use width/height as a bounding box",
			FALSE, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_SCALE, g_param_spec_float(
			"scale", "scale", "The expected scaling factor in bounding box mode",
			0.0, 100.0, 1.0, G_PARAM_READABLE)
	);

	filter_class->name = "Resample filter";
	filter_class->get_image = get_image;
	filter_class->get_width = get_width;
	filter_class->get_height = get_height;
	filter_class->previous_changed = previous_changed;
}

static void
rs_resample_init(RSResample *resample)
{
	resample->target_width = -1;
	resample->target_height = -1;
	resample->new_width = -1;
	resample->new_height = -1;
	resample->bounding_box = FALSE;
	resample->scale = 1.0;
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSResample *resample = RS_RESAMPLE(object);

	switch (property_id)
	{
		case PROP_WIDTH:
			g_value_set_int(value, resample->target_width);
			break;
		case PROP_HEIGHT:
			g_value_set_int(value, resample->target_height);
			break;
		case PROP_BOUNDING_BOX:
			g_value_set_boolean(value, resample->bounding_box);
			break;
		case PROP_SCALE:
			g_value_set_float(value, resample->scale);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSResample *resample = RS_RESAMPLE(object);
	RSFilterChangedMask mask = 0;

	switch (property_id)
	{
		case PROP_WIDTH:
			if (g_value_get_int(value) != resample->target_width)
			{
				resample->target_width = g_value_get_int(value);
				mask |= recalculate_dimensions(resample);
			}
			break;
		case PROP_HEIGHT:
			if (g_value_get_int(value) != resample->target_height)
			{
				resample->target_height = g_value_get_int(value);
				mask |= recalculate_dimensions(resample);
			}
			break;
		case PROP_BOUNDING_BOX:
			if (g_value_get_boolean(value) != resample->bounding_box)
			{
				resample->bounding_box = g_value_get_boolean(value);
				mask |= recalculate_dimensions(resample);
			}
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}

	if (mask)
		rs_filter_changed(RS_FILTER(object), mask);
}

static void
previous_changed(RSFilter *filter, RSFilter *parent, RSFilterChangedMask mask)
{
	if (mask & RS_FILTER_CHANGED_DIMENSION)
		mask |= recalculate_dimensions(RS_RESAMPLE(filter));

	rs_filter_changed(filter, mask);
}

static RSFilterChangedMask
recalculate_dimensions(RSResample *resample)
{
	RSFilterChangedMask mask = 0;
	gint new_width, new_height;

	if (resample->bounding_box && RS_FILTER(resample)->previous)
	{
		const gint previous_width = new_width = rs_filter_get_width(RS_FILTER(resample)->previous);
		const gint previous_height = new_height = rs_filter_get_height(RS_FILTER(resample)->previous);
		rs_constrain_to_bounding_box(resample->target_width, resample->target_height, &new_width, &new_height);
		resample->scale = ((((gfloat) new_width)/ previous_width) + (((gfloat) new_height)/ previous_height))/2.0;
	}
	else
	{
		new_width = resample->target_width;
		new_height = resample->target_height;
		resample->scale = 1.0;
	}

	if ((new_width != resample->new_width) || (new_height != resample->new_height))
	{
		resample->new_width = new_width;
		resample->new_height = new_height;
		mask |= RS_FILTER_CHANGED_DIMENSION;
	}

	return mask;
}

/* This function is (probably) only faster on x86-64 */

#if defined (__x86_64__)
#define RESAMPLE_V_USE_SSE2 1
#else
#define RESAMPLE_V_USE_SSE2 0
#endif

gpointer
start_thread_resampler(gpointer _thread_info)
{
	ResampleInfo* t = _thread_info;

	if (t->input->w == t->output->w)
	{
		if (t->use_fast)
			ResizeV_fast(t);
		else if (!RESAMPLE_V_USE_SSE2 && t->use_compatible)
			ResizeV_compatible(t);
		else if (RESAMPLE_V_USE_SSE2)
			ResizeV_SSE2(t);
		else
			ResizeV(t);
	} else 	{
		if (t->use_fast)
			ResizeH_fast(t);
		else if (t->use_compatible)
			ResizeH_compatible(t);
		else
			ResizeH(t);
	}
	g_thread_exit(NULL);

	return NULL; /* Make the compiler shut up - we'll never return */
}

static RSFilterResponse *
get_image(RSFilter *filter, const RSFilterParam *param)
{
	gboolean use_fast = FALSE;
	RSResample *resample = RS_RESAMPLE(filter);
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *afterVertical;
	RS_IMAGE16 *input;
	RS_IMAGE16 *output = NULL;
	gint input_width = rs_filter_get_width(filter->previous);
	gint input_height = rs_filter_get_height(filter->previous);

	/* Remove ROI, it doesn't make sense across resampler */
	if (rs_filter_param_get_roi(param))
	{
		RSFilterParam *new_param = rs_filter_param_clone(param);
		rs_filter_param_set_roi(new_param, NULL);
		previous_response = rs_filter_get_image(filter->previous, new_param);
		g_object_unref(new_param);
	}
	else
		previous_response = rs_filter_get_image(filter->previous, param);

	/* Return the input, if the new size is uninitialized */
	if ((resample->new_width == -1) || (resample->new_height == -1))
		return previous_response;

	/* Simply return the input, if we don't scale */
	if ((input_width == resample->new_width) && (input_height == resample->new_height))
		return previous_response;

	input = rs_filter_response_get_image(previous_response);

	if (!RS_IS_IMAGE16(input))
		return previous_response;

	response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);

	/* Use compatible (and slow) version if input isn't 3 channels and pixelsize 4 */
	gboolean use_compatible = ( ! ( input->pixelsize == 4 && input->channels == 3));

	if (rs_filter_param_get_quick(param))
	{
		use_fast = TRUE;
		rs_filter_response_set_quick(response);
	}

	guint threads = rs_get_number_of_processor_cores();

	ResampleInfo* h_resample = g_new(ResampleInfo,  threads);
	ResampleInfo* v_resample = g_new(ResampleInfo,  threads);

	/* Create intermediate and output images*/
	afterVertical = rs_image16_new(input_width, resample->new_height, input->channels, input->pixelsize);

	// Only even count
	guint output_x_per_thread = ((input_width + threads * 2 - 1 ) / threads ) & 0xfffe;
	guint output_x_offset = 0;

	GTimer *gt = g_timer_new();

	guint i;
	for (i = 0; i < threads; i++)
	{
		/* Set info for Vertical resampler */
		ResampleInfo *v = &v_resample[i];
		v->input = input;
		v->output  = afterVertical;
		v->old_size = input_height;
		v->new_size = resample->new_height;
		v->dest_offset_other = output_x_offset;
		v->dest_end_other  = MIN(output_x_offset + output_x_per_thread, input_width);
		v->use_compatible = use_compatible;
		v->use_fast = use_fast;

		/* Start it up */
		v->threadid = g_thread_create(start_thread_resampler, v, TRUE, NULL);

		/* Update offset */
		output_x_offset = v->dest_end_other;
	}

	/* Wait for vertical threads to finish */
	for(i = 0; i < threads; i++)
		g_thread_join(v_resample[i].threadid);

	/* input no longer needed */
	g_object_unref(input);

	/* create output */
	output = rs_image16_new(resample->new_width,  resample->new_height, input->channels, input->pixelsize);

	guint input_y_offset = 0;
	guint input_y_per_thread = (resample->new_height+threads-1) / threads;

	gt = g_timer_new();

	for (i = 0; i < threads; i++)
	{
		/* Set info for Horizontal resampler */
		ResampleInfo *h = &h_resample[i];
		h->input = afterVertical;
		h->output  = output;
		h->old_size = input_width;
		h->new_size = resample->new_width;
		h->dest_offset_other = input_y_offset;
		h->dest_end_other  = MIN(input_y_offset+input_y_per_thread, resample->new_height);
		h->use_compatible = use_compatible;
		h->use_fast = use_fast;

		/* Start it up */
		h->threadid = g_thread_create(start_thread_resampler, h, TRUE, NULL);

		/* Update offset */
		input_y_offset = h->dest_end_other;

	}

	/* Wait for horizontal threads to finish */
	for(i = 0; i < threads; i++)
		g_thread_join(h_resample[i].threadid);

	/* Clean up */
	g_free(h_resample);
	g_free(v_resample);
	g_object_unref(afterVertical);

	rs_filter_response_set_image(response, output);
	g_object_unref(output);
	return response;
}

static gint
get_width(RSFilter *filter)
{
	RSResample *resample = RS_RESAMPLE(filter);

	if (resample->new_width == -1)
		return rs_filter_get_width(filter->previous);
	else
		return resample->new_width;
}

static gint
get_height(RSFilter *filter)
{
	RSResample *resample = RS_RESAMPLE(filter);

	if (resample->new_height == -1)
		return rs_filter_get_height(filter->previous);
	else
		return resample->new_height;
}

static guint
lanczos_taps()
{
	return 3;
}

static gdouble
sinc(gdouble value)
{
	if (value != 0.0)
	{
		value *= M_PI;
		return sin(value) / value;
	}
	else
		return 1.0;
}

static gdouble
lanczos_weight(gdouble value)
{
	value = fabs(value);
	if (value < lanczos_taps())
	{
		return (sinc(value) * sinc(value / lanczos_taps()));
	}
	else
		return 0.0;
}

const static gint FPScale = 16384; /* fixed point scaler */
const static gint FPScaleShift = 14; /* fixed point scaler */

static void
ResizeH(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;

	gdouble pos_step = ((gdouble) old_size) / ((gdouble)new_size);
	gdouble filter_step = MIN(1.0 / pos_step, 1.0);
	gdouble filter_support = (gdouble) lanczos_taps() / filter_step;
	gint fir_filter_size = (gint) (ceil(filter_support*2));

	if (old_size <= fir_filter_size)
		return ResizeH_fast(info);

	gint *weights = g_new(gint, new_size * fir_filter_size);
	gint *offsets = g_new(gint, new_size);

	gdouble pos = 0.0;
	gint i,j,k;

	for (i=0; i<new_size; ++i)
	{
		gint end_pos = (gint) (pos + filter_support);

		if (end_pos > old_size-1)
			end_pos = old_size-1;

		gint start_pos = end_pos - fir_filter_size + 1;

		if (start_pos < 0)
			start_pos = 0;

		offsets[i] = start_pos * 4;

		/* the following code ensures that the coefficients add to exactly FPScale */
		gdouble total = 0.0;

		/* Ensure that we have a valid position */
		gdouble ok_pos = MAX(0.0,MIN(old_size-1,pos));

		for (j=0; j<fir_filter_size; ++j)
		{
			/* Accumulate all coefficients */
			total += lanczos_weight((start_pos+j - ok_pos) * filter_step);
		}

		g_assert(total > 0.0f);

		gdouble total2 = 0.0;

		for (k=0; k<fir_filter_size; ++k)
		{
			gdouble total3 = total2 + lanczos_weight((start_pos+k - ok_pos) * filter_step) / total;
			weights[i*fir_filter_size+k] = (gint) (total3*FPScale+0.5) - (gint) (total2*FPScale+0.5);
			total2 = total3;
		}
		pos += pos_step;
	}

	g_assert(input->pixelsize == 4);
	g_assert(input->channels == 3);

	guint y,x;
	for (y = info->dest_offset_other; y < info->dest_end_other ; y++)
	{
		gushort *in_line = GET_PIXEL(input, 0, y);
		gushort *out = GET_PIXEL(output, 0, y);
		gint *wg = weights;

		for (x = 0; x < new_size; x++)
		{
			guint i;
			gushort *in = &in_line[offsets[x]];
			gint acc1 = 0;
			gint acc2 = 0;
			gint acc3 = 0;

			for (i = 0; i <fir_filter_size; i++)
			{
				gint w = *wg++;
				acc1 += in[i*4]*w;
				acc2 += in[i*4+1]*w;
				acc3 += in[i*4+2]*w;
			}
			out[x*4] = clampbits((acc1 + (FPScale/2))>>FPScaleShift, 16);
			out[x*4+1] = clampbits((acc2 + (FPScale/2))>>FPScaleShift, 16);
			out[x*4+2] = clampbits((acc3 + (FPScale/2))>>FPScaleShift, 16);
		}
	}

	g_free(weights);
	g_free(offsets);

}

static void
ResizeV(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;
	const guint start_x = info->dest_offset_other;
	const guint end_x = info->dest_end_other;

	gdouble pos_step = ((gdouble) old_size) / ((gdouble)new_size);
	gdouble filter_step = MIN(1.0 / pos_step, 1.0);
	gdouble filter_support = (gdouble) lanczos_taps() / filter_step;
	gint fir_filter_size = (gint) (ceil(filter_support*2));

	if (old_size <= fir_filter_size)
		return ResizeV_fast(info);

	gint *weights = g_new(gint, new_size * fir_filter_size);
	gint *offsets = g_new(gint, new_size);

	gdouble pos = 0.0;

	gint i,j,k;

	for (i=0; i<new_size; ++i)
	{
		gint end_pos = (gint) (pos + filter_support);
		if (end_pos > old_size-1)
			end_pos = old_size-1;

		gint start_pos = end_pos - fir_filter_size + 1;

		if (start_pos < 0)
			start_pos = 0;

		offsets[i] = start_pos;

		/* The following code ensures that the coefficients add to exactly FPScale */
		gdouble total = 0.0;

		/* Ensure that we have a valid position */
		gdouble ok_pos = MAX(0.0,MIN(old_size-1,pos));

		for (j=0; j<fir_filter_size; ++j)
		{
			/* Accumulate all coefficients */
			total += lanczos_weight((start_pos+j - ok_pos) * filter_step);
		}

		g_assert(total > 0.0f);

		gdouble total2 = 0.0;

		for (k=0; k<fir_filter_size; ++k)
		{
			gdouble total3 = total2 + lanczos_weight((start_pos+k - ok_pos) * filter_step) / total;
			weights[i*fir_filter_size+k] = (gint) (total3*FPScale+0.5) - (gint) (total2*FPScale+0.5);
			total2 = total3;
		}
		pos += pos_step;
	}

	g_assert(input->pixelsize == 4);
	g_assert(input->channels == 3);

	guint y,x;
    gint *wg = weights;

	for (y = 0; y < new_size ; y++)
	{
		gushort *in = GET_PIXEL(input, start_x, offsets[y]);
		gushort *out = GET_PIXEL(output, 0, y);
		for (x = start_x; x < end_x; x++)
		{
			gint acc1 = 0;
			gint acc2 = 0;
			gint acc3 = 0;
			for (i = 0; i < fir_filter_size; i++)
			{
				acc1 += in[i*input->rowstride]* wg[i];
				acc2 += in[i*input->rowstride+1] * wg[i];
				acc3 += in[i*input->rowstride+2] * wg[i];
			}
			out[x*4] = clampbits((acc1 + (FPScale/2))>>FPScaleShift, 16);
			out[x*4+1] = clampbits((acc2 + (FPScale/2))>>FPScaleShift, 16);
			out[x*4+2] = clampbits((acc3 + (FPScale/2))>>FPScaleShift, 16);
			in+=4;
		}
		wg+=fir_filter_size;
	}

	g_free(weights);
	g_free(offsets);

}

/* Special Vertical SSE2 resampler, that has massive parallism,
 * but on the other hand has to convert all data to float before
 * processing it, because there is no 32 * 32 bit multiply in SSE2.
 * This makes it very precise, and faster on a Core2 and later Intel
 * processors in 64 bit mode.
 * An important restriction is that "info->dest_offset_other", must result
 * in a 16 byte aligned memory pointer.
 */

#if defined (__x86_64__)

static void
ResizeV_SSE2(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;
	const guint start_x = info->dest_offset_other * input->pixelsize;
	const guint end_x = info->dest_end_other * input->pixelsize;

	gdouble pos_step = ((gdouble) old_size) / ((gdouble)new_size);
	gdouble filter_step = MIN(1.0 / pos_step, 1.0);
	gdouble filter_support = (gdouble) lanczos_taps() / filter_step;
	gint fir_filter_size = (gint) (ceil(filter_support*2));

	if (old_size <= fir_filter_size)
		return ResizeV_fast(info);

	gfloat *weights = g_new(gfloat, new_size * fir_filter_size);
	gint *offsets = g_new(gint, new_size);

	gdouble pos = 0.0;

	gint i,j,k;

	for (i=0; i<new_size; ++i)
	{
		gint end_pos = (gint) (pos + filter_support);
		if (end_pos > old_size-1)
			end_pos = old_size-1;

		gint start_pos = end_pos - fir_filter_size + 1;

		if (start_pos < 0)
			start_pos = 0;

		offsets[i] = start_pos;

		/* The following code ensures that the coefficients add to exactly FPScale */
		gdouble total = 0.0;

		/* Ensure that we have a valid position */
		gdouble ok_pos = MAX(0.0,MIN(old_size-1,pos));

		for (j=0; j<fir_filter_size; ++j)
		{
			/* Accumulate all coefficients */
			total += lanczos_weight((start_pos+j - ok_pos) * filter_step);
		}

		g_assert(total > 0.0f);

		gdouble total2 = 0.0;

		for (k=0; k<fir_filter_size; ++k)
		{
			gdouble total3 = total2 + lanczos_weight((start_pos+k - ok_pos) * filter_step) / total;
			weights[i*fir_filter_size+k] = total3 - total2;
			total2 = total3;
		}
		pos += pos_step;
	}

	guint y,x;
	gfloat *wg = weights;

	/* 24 pixels = 48 bytes/loop */
	gint end_x_sse = (end_x/24)*24;

	for (y = 0; y < new_size ; y++)
	{
		gushort *in = GET_PIXEL(input, start_x / input->pixelsize, offsets[y]);
		gushort *out = GET_PIXEL(output, 0, y);
		__m128i zero;
		zero = _mm_xor_si128(zero, zero);
		for (x = start_x; x <= (end_x_sse-24); x+=24)
		{
			/* Accumulators, set to 0 */
			__m128 acc1, acc2,  acc3, acc1_h, acc2_h, acc3_h;
			acc1 = acc2 = acc3 = acc1_h = acc2_h = acc3_h = (__m128)zero;

			for (i = 0; i < fir_filter_size; i++) {
				/* Load weight */
				__m128 w = _mm_set_ps(wg[i],wg[i],wg[i],wg[i]);
				/* Load source */
				__m128i src1i, src2i, src3i;
				__m128i* in_sse =  (__m128i*)&in[i*input->rowstride];
				src1i = _mm_load_si128(in_sse);
				src2i = _mm_load_si128(in_sse+1);
				src3i = _mm_load_si128(in_sse+2);
				/* Unpack to dwords */
				__m128i src1i_high, src2i_high, src3i_high;
				src1i_high = _mm_unpackhi_epi16(src1i, zero);
				src2i_high = _mm_unpackhi_epi16(src2i, zero);
				src3i_high = _mm_unpackhi_epi16(src3i, zero);
				src1i = _mm_unpacklo_epi16(src1i, zero);
				src2i = _mm_unpacklo_epi16(src2i, zero);
				src3i = _mm_unpacklo_epi16(src3i, zero);

				/* Convert to float */
				__m128 src1, src2, src3;
				__m128 src1_high, src2_high, src3_high;
				src1_high = _mm_cvtepi32_ps(src1i_high);
				src2_high = _mm_cvtepi32_ps(src2i_high);
				src3_high = _mm_cvtepi32_ps(src3i_high);
				src1 = _mm_cvtepi32_ps(src1i);
				src2 = _mm_cvtepi32_ps(src2i);
				src3 = _mm_cvtepi32_ps(src3i);

				/* Multiply by weight */
				src1_high = _mm_mul_ps(src1_high, w);
				src2_high = _mm_mul_ps(src2_high, w);
				src3_high = _mm_mul_ps(src3_high, w);
				src1 = _mm_mul_ps(src1, w);
				src2 = _mm_mul_ps(src2, w);
				src3 = _mm_mul_ps(src3, w);

				/* Accumulate */
				acc1_h = _mm_add_ps(acc1_h, src1_high);
				acc2_h = _mm_add_ps(acc2_h, src2_high);
				acc3_h = _mm_add_ps(acc3_h, src3_high);
				acc1 = _mm_add_ps(acc1, src1);
				acc2 = _mm_add_ps(acc2, src2);
				acc3 = _mm_add_ps(acc3, src3);
			}
			__m128i subtract = _mm_set_epi32(32768, 32768, 32768, 32768);

			/* Convert to integer */
			__m128i acc1i, acc2i,  acc3i, acc1_hi, acc2_hi, acc3_hi;
			acc1i = _mm_cvtps_epi32(acc1);
			acc2i = _mm_cvtps_epi32(acc2);
			acc3i = _mm_cvtps_epi32(acc3);
			acc1_hi = _mm_cvtps_epi32(acc1_h);
			acc2_hi = _mm_cvtps_epi32(acc2_h);
			acc3_hi = _mm_cvtps_epi32(acc3_h);

			/* Subtract 32768 to avoid signed saturation */
			acc1i = _mm_sub_epi32(acc1i, subtract);
			acc2i = _mm_sub_epi32(acc2i, subtract);
			acc3i = _mm_sub_epi32(acc3i, subtract);
			acc1_hi = _mm_sub_epi32(acc1_hi, subtract);
			acc2_hi = _mm_sub_epi32(acc2_hi, subtract);
			acc3_hi = _mm_sub_epi32(acc3_hi, subtract);

			__m128i signxor = _mm_set_epi32(0x80008000, 0x80008000, 0x80008000, 0x80008000);

			/* Pack to signed shorts */
			acc1i = _mm_packs_epi32(acc1i, acc1_hi);
			acc2i = _mm_packs_epi32(acc2i, acc2_hi);
			acc3i = _mm_packs_epi32(acc3i, acc3_hi);

			/* Shift sign to unsinged shorts */
			acc1i = _mm_xor_si128(acc1i,signxor);
			acc2i = _mm_xor_si128(acc2i,signxor);
			acc3i = _mm_xor_si128(acc3i,signxor);

			/* Store result */
			__m128i* sse_dst = (__m128i*)&out[x];
			_mm_store_si128(sse_dst, acc1i);
			_mm_store_si128(sse_dst+1, acc2i);
			_mm_store_si128(sse_dst+2, acc3i);
			in+=24;
		}
		/* Process remaining pixels */
		for (; x < end_x; x++)
		{
			gfloat acc1 = 0;
			for (i = 0; i < fir_filter_size; i++)
			{
				acc1 += (gfloat)in[i*input->rowstride]* wg[i];
			}
			out[x] = (gushort)(acc1);
			in++;
		}
		wg+=fir_filter_size;
	}
	g_free(weights);
	g_free(offsets);
}

#else // not defined (__x86_64__)

static void
ResizeV_SSE2(ResampleInfo *info)
{
	g_assert(FALSE);
}

#endif // not defined (__x86_64__)

static void
ResizeH_compatible(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;

	gint pixelsize = input->pixelsize;
	gint ch = input->channels;

	gdouble pos_step = ((gdouble) old_size) / ((gdouble)new_size);
	gdouble filter_step = MIN(1.0 / pos_step, 1.0);
	gdouble filter_support = (gdouble) lanczos_taps() / filter_step;
	gint fir_filter_size = (gint) (ceil(filter_support*2));

	if (old_size <= fir_filter_size)
		return ResizeH_fast(info);

	gint *weights = g_new(gint, new_size * fir_filter_size);
	gint *offsets = g_new(gint, new_size);

	gdouble pos = 0.0;
	gint i,j,k;

	for (i=0; i<new_size; ++i)
	{
		gint end_pos = (gint) (pos + filter_support);

		if (end_pos > old_size-1)
			end_pos = old_size-1;

		gint start_pos = end_pos - fir_filter_size + 1;

		if (start_pos < 0)
			start_pos = 0;

		offsets[i] = start_pos * pixelsize;

		/* the following code ensures that the coefficients add to exactly FPScale */
		gdouble total = 0.0;

		/* Ensure that we have a valid position */
		gdouble ok_pos = MAX(0.0,MIN(old_size-1,pos));

		for (j=0; j<fir_filter_size; ++j)
		{
			/* Accumulate all coefficients */
			total += lanczos_weight((start_pos+j - ok_pos) * filter_step);
		}

		g_assert(total > 0.0f);

		gdouble total2 = 0.0;

		for (k=0; k<fir_filter_size; ++k)
		{
			gdouble total3 = total2 + lanczos_weight((start_pos+k - ok_pos) * filter_step) / total;
			weights[i*fir_filter_size+k] = (gint) (total3*FPScale+0.5) - (gint) (total2*FPScale+0.5);
			total2 = total3;
		}
		pos += pos_step;
	}


	guint y,x,c;
	for (y = info->dest_offset_other; y < info->dest_end_other ; y++)
	{
		gint *wg = weights;
		gushort *in_line = GET_PIXEL(input, 0, y);
		gushort *out = GET_PIXEL(output, 0, y);

		for (x = 0; x < new_size; x++)
		{
			guint i;
			gushort *in = &in_line[offsets[x]];
			for (c = 0 ; c < ch; c++)
			{
				gint acc = 0;

				for (i = 0; i <fir_filter_size; i++)
				{
					acc += in[i*pixelsize+c]*wg[i];
				}
				out[x*pixelsize+c] = clampbits((acc + (FPScale/2))>>FPScaleShift, 16);
			}
			wg += fir_filter_size;
		}
	}

	g_free(weights);
	g_free(offsets);

}

static void
ResizeV_compatible(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;
	const guint start_x = info->dest_offset_other;
	const guint end_x = info->dest_end_other;

	gint pixelsize = input->pixelsize;
	gint ch = input->channels;

	gdouble pos_step = ((gdouble) old_size) / ((gdouble)new_size);
	gdouble filter_step = MIN(1.0 / pos_step, 1.0);
	gdouble filter_support = (gdouble) lanczos_taps() / filter_step;
	gint fir_filter_size = (gint) (ceil(filter_support*2));

	if (old_size <= fir_filter_size)
		return ResizeV_fast(info);

	gint *weights = g_new(gint, new_size * fir_filter_size);
	gint *offsets = g_new(gint, new_size);

	gdouble pos = 0.0;

	gint i,j,k;

	for (i=0; i<new_size; ++i)
	{
		gint end_pos = (gint) (pos + filter_support);
		if (end_pos > old_size-1)
			end_pos = old_size-1;

		gint start_pos = end_pos - fir_filter_size + 1;

		if (start_pos < 0)
			start_pos = 0;

		offsets[i] = start_pos;

		/* The following code ensures that the coefficients add to exactly FPScale */
		gdouble total = 0.0;

		/* Ensure that we have a valid position */
		gdouble ok_pos = MAX(0.0,MIN(old_size-1,pos));

		for (j=0; j<fir_filter_size; ++j)
		{
			/* Accumulate all coefficients */
			total += lanczos_weight((start_pos+j - ok_pos) * filter_step);
		}

		g_assert(total > 0.0f);

		gdouble total2 = 0.0;

		for (k=0; k<fir_filter_size; ++k)
		{
			gdouble total3 = total2 + lanczos_weight((start_pos+k - ok_pos) * filter_step) / total;
			weights[i*fir_filter_size+k] = (gint) (total3*FPScale+0.5) - (gint) (total2*FPScale+0.5);
			total2 = total3;
		}
		pos += pos_step;
	}

	guint y,x,c;
	gint *wg = weights;

	for (y = 0; y < new_size ; y++)
	{
		gushort *out = GET_PIXEL(output, 0, y);
		for (x = start_x; x < end_x; x++)
		{
			gushort *in = GET_PIXEL(input, x, offsets[y]);
			for (c = 0; c < ch; c++)
			{
				gint acc = 0;
				for (i = 0; i < fir_filter_size; i++)
				{
					acc += in[i*input->rowstride]* wg[i];
				}
				out[x*pixelsize+c] = clampbits((acc + (FPScale/2))>>FPScaleShift, 16);
				in++;
			}
		}
		wg+=fir_filter_size;
	}

	g_free(weights);
	g_free(offsets);
}

static void
ResizeV_fast(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;
	const guint start_x = info->dest_offset_other;
	const guint end_x = info->dest_end_other;

	gint pixelsize = input->pixelsize;
	gint ch = input->channels;

	gdouble pos_step = ((gdouble) old_size) / ((gdouble)new_size);

	gint pos = 0;
	gint delta = (gint)(pos_step * 65536.0);

	guint y,x,c;

	for (y = 0; y < new_size ; y++)
	{
		gushort *out = GET_PIXEL(output, 0, y);
		for (x = start_x; x < end_x; x++)
		{
			gushort *in = GET_PIXEL(input, x, pos>>16);
			for (c = 0; c < ch; c++)
			{
				out[x*pixelsize+c] = in[c];
			}
		}
		pos += delta;
	}
}



static void
ResizeH_fast(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;

	gint pixelsize = input->pixelsize;
	gint ch = input->channels;

	gdouble pos_step = ((gdouble) old_size) / ((gdouble)new_size);

	gint pos;
	gint delta = (gint)(pos_step * 65536.0);

	guint y,x,c;
	for (y = info->dest_offset_other; y < info->dest_end_other ; y++)
	{
		gushort *in_line = GET_PIXEL(input, 0, y);
		gushort *out = GET_PIXEL(output, 0, y);
		pos = 0;

		for (x = 0; x < new_size; x++)
		{
			for (c = 0 ; c < ch; c++)
			{
				out[x*pixelsize+c] = in_line[(pos>>16)*pixelsize+c];
			}
			pos += delta;
		}
	}
}

