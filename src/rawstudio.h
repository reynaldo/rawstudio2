/*
 * Copyright (C) 2006 Anders Brander <anders@brander.dk> and 
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
#ifndef RS_RAWSTUDIO_H
#define RS_RAWSTUDIO_H

#include <gtk/gtk.h>
#include <glib.h>
#include <lcms.h>
#include "dcraw_api.h"
#include "rs-batch.h"
#include "matrix.h"
#include "rs-arch.h"

#define PITCH(width) ((((width)+31)/32)*32)

#define SWAP( a, b ) a ^= b ^= a ^= b

#define DOTDIR ".rawstudio"
#define HISTOGRAM_DATASET_WIDTH (250)

#define ORIENTATION_RESET(orientation) orientation = 0
#define ORIENTATION_90(orientation) orientation = (orientation&4) | ((orientation+1)&3)
#define ORIENTATION_180(orientation) orientation = (orientation^2)
#define ORIENTATION_270(orientation) orientation = (orientation&4) | ((orientation+3)&3)
#define ORIENTATION_FLIP(orientation) orientation = (orientation^4)
#define ORIENTATION_MIRROR(orientation) orientation = ((orientation&4)^4) | ((orientation+2)&3)

#define GETVAL(adjustment) \
	gtk_adjustment_get_value((GtkAdjustment *) adjustment)
#define SETVAL(adjustment, value) \
	gtk_adjustment_set_value((GtkAdjustment *) adjustment, value)

enum {
	STATE_NORMAL,
	STATE_CROP,
	STATE_CROP_MOVE_N,
	STATE_CROP_MOVE_E,
	STATE_CROP_MOVE_S,
	STATE_CROP_MOVE_W,
	STATE_CROP_MOVE_NW,
	STATE_CROP_MOVE_NE,
	STATE_CROP_MOVE_SE,
	STATE_CROP_MOVE_SW,
};

enum {
	MASK_EXPOSURE = 1,
	MASK_SATURATION = 2,
	MASK_HUE = 4,
	MASK_CONTRAST = 8,
	MASK_WARMTH = 16,
	MASK_TINT = 32,
	MASK_ALL = 63
};

#define MASK_WB (MASK_WARMTH|MASK_TINT)

enum {
	MAKE_UNKNOWN = 0,
	MAKE_CANON,
	MAKE_NIKON,
	MAKE_MINOLTA,
};

enum {
	MASK_OVER = 128,
	MASK_UNDER = 64,
};

enum {
	RS_CMS_PROFILE_IN,
	RS_CMS_PROFILE_DISPLAY,
	RS_CMS_PROFILE_EXPORT
};

#if __GNUC__ >= 3
#define likely(x) __builtin_expect (!!(x), 1)
#define unlikely(x) __builtin_expect (!!(x), 0)
#define align(x) __attribute__ ((aligned (x)))
#define __deprecated __attribute__ ((deprecated))
#else
#define likely(x) (x)
#define unlikely(x) (x)
#define align(x)
#define __deprecated
#endif

typedef struct {
	guint w;
	guint h;
	gint rowstride;
	guint channels;
	guint pixelsize; /* the size of a pixel in CHARS */
	guint orientation;
	guchar *pixels;
	guint *pixels4;
	GdkImage *image;
} RS_IMAGE8;

typedef struct _rs_image16 {
	guint w;
	guint h;
	gint pitch;
	gint rowstride;
	guint channels;
	guint pixelsize; /* the size of a pixel in SHORTS */
	guint orientation;
	gushort *pixels;
	struct _rs_image16 *parent; /* use with care! */
} RS_IMAGE16;

typedef struct {
	gint x1;
	gint y1;
	gint x2;
	gint y2;
} RS_RECT;

typedef struct {
	GtkObject *exposure;
	GtkObject *saturation;
	GtkObject *hue;
	GtkObject *contrast;
	GtkObject *warmth;
	GtkObject *tint;
} RS_SETTINGS;

typedef struct {
	gdouble exposure;
	gdouble saturation;
	gdouble hue;
	gdouble contrast;
	gdouble warmth;
	gdouble tint;
} RS_SETTINGS_DOUBLE;

typedef struct _metadata {
	gint make;
	gushort orientation;
	gfloat aperture;
	gushort iso;
	gfloat shutterspeed;
	guint thumbnail_start;
	guint thumbnail_length;
	guint preview_start;
	guint preview_length;
	gdouble cam_mul[4];
	gdouble contrast;
	gdouble saturation;
	gdouble sharpness;
	gdouble color_tone;
	gshort focallength;
} RS_METADATA;

typedef struct _photo {
	gboolean active;
	gchar *filename;
	RS_IMAGE16 *input;
	RS_IMAGE16 *scaled;
	RS_IMAGE8 *preview;
	RS_IMAGE8 *mask;
	RS_SETTINGS_DOUBLE *settings[3];
	gint current_setting;
	gint priority;
	guint orientation;
	RS_METADATA *metadata;
	RS_MATRIX4Int mati;
	RS_MATRIX4 mat;
	gfloat pre_mul[4];
	RS_RECT *crop;
} RS_PHOTO;

typedef struct {
	gboolean in_use;
	RS_PHOTO *photo;
	RS_RECT roi;
	RS_RECT roi_scaled;
	RS_SETTINGS_DOUBLE *settings_buffer;
	RS_SETTINGS *settings[3];
	gint current_setting;
	GtkObject *scale;
	gboolean mark_roi;
	gdouble preview_scale;
	gboolean zoom_to_fit;
	RS_RECT *preview_exposed;
	RS_IMAGE16 *histogram_dataset;
	guint histogram_table[3][256];
	GtkImage *histogram_image;
	GtkWidget *preview_drawingarea;
	gint preview_width;
	gint preview_height;
	gboolean preview_idle_render;
	gboolean preview_done;
	GdkPixmap *preview_backing;
	GdkPixmap *preview_backing_crop;
	gint preview_idle_render_lastrow;
	gboolean show_exposure_overlay;
	GArray *batch_queue;
	RS_QUEUE *queue;
	void *loadProfile;
	void *displayProfile;
	void *exportProfile;
	gchar *exportProfileFilename;
	gint cms_intent;
	gboolean cms_enabled;
} RS_BLOB;

enum {
	FILETYPE_RAW,
	FILETYPE_JPEG,
	FILETYPE_PNG,
	FILETYPE_TIFF8,
	FILETYPE_TIFF16,
};

typedef struct _rs_filetype {
	gchar *id;
	gint filetype;
	const gchar *ext;
	gchar *description;
	RS_PHOTO *(*load)(const gchar *);
	GdkPixbuf *(*thumb)(const gchar *);
	void (*load_meta)(const gchar *, RS_METADATA *);
	gboolean (*save)(RS_PHOTO *photo, const gchar *filename, gint filetype, const gchar *profile_filename);
	struct _rs_filetype *next;
} RS_FILETYPE;

void rs_local_cachedir(gboolean new_value);
void rs_load_gdk(gboolean new_value);
void update_preview(RS_BLOB *rs, gboolean update_table, gboolean update_scale);
void update_preview_region(RS_BLOB *rs, RS_RECT *region);
gboolean rs_run_batch_idle(RS_QUEUE *queue);
void rs_reset(RS_BLOB *rs);
void rs_settings_to_rs_settings_double(RS_SETTINGS *rs_settings, RS_SETTINGS_DOUBLE *rs_settings_double);
void rs_settings_double_to_rs_settings(RS_SETTINGS_DOUBLE *rs_settings_double, RS_SETTINGS *rs_settings);
void rs_settings_reset(RS_SETTINGS *rss, guint mask);
RS_PHOTO *rs_photo_new();
void rs_photo_free(RS_PHOTO *photo);
gboolean rs_photo_save(RS_PHOTO *photo, const gchar *filename, gint filetype,
	const gchar *profile_filename);
RS_METADATA *rs_metadata_new();
void rs_metadata_free(RS_METADATA *metadata);
void rs_metadata_normalize_wb(RS_METADATA *meta);
RS_BLOB *rs_new();
void rs_free(RS_BLOB *rs);
void rs_zoom_to_fit(RS_BLOB *rs);
void rs_photo_close(RS_PHOTO *photo);
RS_FILETYPE *rs_filetype_get(const gchar *filename, gboolean load);
gchar *rs_dotdir_get(const gchar *filename);
gchar *rs_thumb_get_name(const gchar *src);
void rs_set_wb_auto(RS_BLOB *rs);
void rs_set_wb_from_pixels(RS_BLOB *rs, gint x, gint y);
void rs_set_wb_from_color(RS_BLOB *rs, gdouble r, gdouble g, gdouble b);
void rs_set_wb_from_mul(RS_BLOB *rs, gdouble *mul);
void rs_set_wb(RS_BLOB *rs, gfloat warmth, gfloat tint);
void rs_render_pixel_to_srgb(RS_BLOB *rs, gint x, gint y, guchar *dest);
void rs_apply_settings_from_double(RS_SETTINGS *rss, RS_SETTINGS_DOUBLE *rsd, gint mask);
void rs_rect_scale(RS_RECT *in, RS_RECT *out, gdouble scale);
void rs_rect_union(RS_RECT *a, RS_RECT *b, RS_RECT *destination);
void rs_roi_orientation(RS_BLOB *rs);
void rs_crop_start(RS_BLOB *rs);
void rs_crop_end(RS_BLOB *rs, gboolean accept);
void rs_crop_uncrop(RS_BLOB *rs);
void rs_state_reset(RS_BLOB *rs);
gchar *rs_get_profile(gint type);
gboolean rs_cms_is_profile_valid(const gchar *path);
void rs_cms_prepare_transforms(RS_BLOB *rs);
gboolean rs_shutdown(GtkWidget *dummy1, GdkEvent *dummy2, RS_BLOB *rs);
#if !GLIB_CHECK_VERSION(2,8,0)
int g_mkdir_with_parents (const gchar *pathname, int mode);
#endif

extern void
(*rs_photo_open_dcraw_apply_black_and_shift)(dcraw_data *raw, RS_PHOTO *photo) __rs_optimized;

extern void
rs_photo_open_dcraw_apply_black_and_shift_c(dcraw_data *raw, RS_PHOTO *photo);

#if defined (__i386__) || (__x86_64__)
extern void
rs_photo_open_dcraw_apply_black_and_shift_mmx(dcraw_data *raw, RS_PHOTO *photo);
#endif

#endif /* RS_RAWSTUDIO_H */
