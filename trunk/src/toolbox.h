GtkWidget *gui_hist(RS_BLOB *rs, const gchar *label);
GtkWidget *gui_box(const gchar *title, GtkWidget *in);
void gui_transform_rot90_clicked(GtkWidget *w, RS_BLOB *rs);
void gui_transform_rot180_clicked(GtkWidget *w, RS_BLOB *rs);
void gui_transform_rot270_clicked(GtkWidget *w, RS_BLOB *rs);
void gui_transform_mirror_clicked(GtkWidget *w, RS_BLOB *rs);
void gui_transform_flip_clicked(GtkWidget *w, RS_BLOB *rs);
GtkWidget *gui_transform(RS_BLOB *rs);
GtkWidget *gui_tool_rgb_mixer(RS_BLOB *rs, gint n);
GtkWidget *gui_tool_warmth(RS_BLOB *rs, gint n);
GtkWidget *gui_slider(GtkObject *adj, const gchar *label);
void gui_reset_clicked(GtkWidget *w, RS_BLOB *rs);
GtkWidget *gui_reset(RS_BLOB *rs);
void save_file_clicked(GtkWidget *w, RS_BLOB *rs);
GtkWidget *save_file(RS_BLOB *rs);
GtkWidget *gui_make_scale(RS_BLOB *rs, GCallback cb, double value, double min, double max, double step, double page);
GtkWidget *gui_make_scale_from_adj(RS_BLOB *rs, GCallback cb, GtkObject *adj);
GtkWidget *gui_tool_exposure(RS_BLOB *rs, gint n);
GtkWidget *gui_tool_saturation(RS_BLOB *rs, gint n);
GtkWidget *gui_tool_hue(RS_BLOB *rs, gint n);
GtkWidget *gui_tool_contrast(RS_BLOB *rs, gint n);
GtkWidget *gui_tool_gamma(RS_BLOB *rs, gint n);
GtkWidget *gui_make_tools(RS_BLOB *rs, gint n);
void gui_notebook_callback(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, RS_BLOB *rs);
GtkWidget *make_toolbox(RS_BLOB *rs);
