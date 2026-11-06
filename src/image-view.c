/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * image-view.c: zoomable map image view
 */

#include "image-view.h"

#include <graphene.h>

#define IMAGE_ZOOM_STEP 1.25

typedef enum {
    IMAGE_ZOOM_FIT_WIDTH,
    IMAGE_ZOOM_FIT_WINDOW,
    IMAGE_ZOOM_FREE
} GswordImageZoomMode;

struct _GswordImageView {
    GtkWidget parent_instance;

    GdkTexture *texture;
    gchar *source_path;
    gint image_width;
    gint image_height;
    gdouble zoom;
    GswordImageZoomMode zoom_mode;
    gdouble offset_x;
    gdouble offset_y;
    gdouble pointer_x;
    gdouble pointer_y;
    gdouble drag_origin_x;
    gdouble drag_origin_y;
};

G_DEFINE_TYPE(GswordImageView, gsword_image_view, GTK_TYPE_WIDGET)

enum {
    ZOOM_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static gboolean
nearly_equal(gdouble first, gdouble second)
{
    return first > second ? first - second < 1e-4
                          : second - first < 1e-4;
}

static void
emit_zoom_changed(GswordImageView *self)
{
    g_signal_emit(self, signals[ZOOM_CHANGED], 0);
}

static void
clamp_offsets(GswordImageView *self, gint view_width, gint view_height)
{
    gdouble scaled_width = self->image_width * self->zoom;
    gdouble scaled_height = self->image_height * self->zoom;

    if (scaled_width <= view_width)
        self->offset_x = (view_width - scaled_width) / 2.0;
    else
        self->offset_x = CLAMP(self->offset_x,
                               view_width - scaled_width, 0.0);
    if (scaled_height <= view_height)
        self->offset_y = (view_height - scaled_height) / 2.0;
    else
        self->offset_y = CLAMP(self->offset_y,
                               view_height - scaled_height, 0.0);
}

static void
update_geometry(GswordImageView *self, gint view_width, gint view_height)
{
    gdouble previous_zoom;
    gdouble target;
    gdouble scaled_width;
    gdouble scaled_height;
    gboolean changed;

    if (self->texture == NULL || self->image_width <= 0 ||
        self->image_height <= 0 || view_width <= 0 || view_height <= 0) {
        return;
    }
    previous_zoom = self->zoom;
    target = self->zoom;
    switch (self->zoom_mode) {
    case IMAGE_ZOOM_FIT_WIDTH:
        target = (gdouble)view_width / (gdouble)self->image_width;
        break;
    case IMAGE_ZOOM_FIT_WINDOW:
        target = MIN((gdouble)view_width / (gdouble)self->image_width,
                     (gdouble)view_height / (gdouble)self->image_height);
        break;
    case IMAGE_ZOOM_FREE:
    default:
        break;
    }
    self->zoom = CLAMP(target, GSWORD_IMAGE_MIN_ZOOM,
                       GSWORD_IMAGE_MAX_ZOOM);
    changed = !nearly_equal(previous_zoom, self->zoom);

    scaled_width = self->image_width * self->zoom;
    scaled_height = self->image_height * self->zoom;
    if (self->zoom_mode == IMAGE_ZOOM_FIT_WIDTH) {
        self->offset_x = (view_width - scaled_width) / 2.0;
        self->offset_y = scaled_height <= view_height
                       ? (view_height - scaled_height) / 2.0 : 0.0;
    } else if (self->zoom_mode == IMAGE_ZOOM_FIT_WINDOW) {
        self->offset_x = (view_width - scaled_width) / 2.0;
        self->offset_y = (view_height - scaled_height) / 2.0;
    } else {
        clamp_offsets(self, view_width, view_height);
    }
    if (changed)
        emit_zoom_changed(self);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void
gsword_image_view_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
    GswordImageView *self = GSWORD_IMAGE_VIEW(widget);
    graphene_rect_t rect;

    update_geometry(self, gtk_widget_get_width(widget),
                    gtk_widget_get_height(widget));
    if (self->texture == NULL)
        return;
    graphene_rect_init(&rect, self->offset_x, self->offset_y,
                       self->image_width * self->zoom,
                       self->image_height * self->zoom);
    gtk_snapshot_append_texture(snapshot, self->texture, &rect);
}

static void
gsword_image_view_size_allocate(GtkWidget *widget, gint width, gint height,
                                gint baseline)
{
    GswordImageView *self = GSWORD_IMAGE_VIEW(widget);

    GTK_WIDGET_CLASS(gsword_image_view_parent_class)->size_allocate(
        widget, width, height, baseline);
    update_geometry(self, width, height);
}

static void
zoom_to_anchor(GswordImageView *self, gdouble zoom,
               gdouble anchor_x, gdouble anchor_y)
{
    gdouble old_zoom;
    gdouble content_x;
    gdouble content_y;
    gint view_width;
    gint view_height;

    if (self->texture == NULL)
        return;
    view_width = gtk_widget_get_width(GTK_WIDGET(self));
    view_height = gtk_widget_get_height(GTK_WIDGET(self));
    old_zoom = self->zoom > 0.0 ? self->zoom : 1.0;
    anchor_x = view_width > 0 ? CLAMP(anchor_x, 0.0, (gdouble)view_width) : 0.0;
    anchor_y = view_height > 0 ? CLAMP(anchor_y, 0.0, (gdouble)view_height) : 0.0;
    content_x = (anchor_x - self->offset_x) / old_zoom;
    content_y = (anchor_y - self->offset_y) / old_zoom;
    self->zoom = CLAMP(zoom, GSWORD_IMAGE_MIN_ZOOM,
                       GSWORD_IMAGE_MAX_ZOOM);
    self->zoom_mode = IMAGE_ZOOM_FREE;
    self->offset_x = anchor_x - content_x * self->zoom;
    self->offset_y = anchor_y - content_y * self->zoom;
    update_geometry(self, view_width, view_height);
    if (!nearly_equal(old_zoom, self->zoom))
        emit_zoom_changed(self);
}

void
gsword_image_view_fit_width(GswordImageView *self)
{
    g_return_if_fail(GSWORD_IS_IMAGE_VIEW(self));
    self->zoom_mode = IMAGE_ZOOM_FIT_WIDTH;
    update_geometry(self, gtk_widget_get_width(GTK_WIDGET(self)),
                    gtk_widget_get_height(GTK_WIDGET(self)));
}

void
gsword_image_view_fit_window(GswordImageView *self)
{
    g_return_if_fail(GSWORD_IS_IMAGE_VIEW(self));
    self->zoom_mode = IMAGE_ZOOM_FIT_WINDOW;
    update_geometry(self, gtk_widget_get_width(GTK_WIDGET(self)),
                    gtk_widget_get_height(GTK_WIDGET(self)));
}

void
gsword_image_view_actual_size(GswordImageView *self)
{
    gint view_width;
    gint view_height;

    g_return_if_fail(GSWORD_IS_IMAGE_VIEW(self));
    view_width = gtk_widget_get_width(GTK_WIDGET(self));
    view_height = gtk_widget_get_height(GTK_WIDGET(self));
    {
        gdouble old_zoom = self->zoom;

        self->zoom = 1.0;
        self->zoom_mode = IMAGE_ZOOM_FREE;
        self->offset_x = (view_width - self->image_width) / 2.0;
        self->offset_y = (view_height - self->image_height) / 2.0;
        update_geometry(self, view_width, view_height);
        if (!nearly_equal(old_zoom, self->zoom))
            emit_zoom_changed(self);
    }
}

void
gsword_image_view_zoom_in(GswordImageView *self)
{
    gint width;
    gint height;

    g_return_if_fail(GSWORD_IS_IMAGE_VIEW(self));
    width = gtk_widget_get_width(GTK_WIDGET(self));
    height = gtk_widget_get_height(GTK_WIDGET(self));
    zoom_to_anchor(self, self->zoom * IMAGE_ZOOM_STEP,
                   width / 2.0, height / 2.0);
}

void
gsword_image_view_zoom_out(GswordImageView *self)
{
    gint width;
    gint height;

    g_return_if_fail(GSWORD_IS_IMAGE_VIEW(self));
    width = gtk_widget_get_width(GTK_WIDGET(self));
    height = gtk_widget_get_height(GTK_WIDGET(self));
    zoom_to_anchor(self, self->zoom / IMAGE_ZOOM_STEP,
                   width / 2.0, height / 2.0);
}

gdouble
gsword_image_view_get_zoom(GswordImageView *self)
{
    g_return_val_if_fail(GSWORD_IS_IMAGE_VIEW(self), 1.0);
    return self->zoom;
}

GdkTexture *
gsword_image_view_get_texture(GswordImageView *self)
{
    g_return_val_if_fail(GSWORD_IS_IMAGE_VIEW(self), NULL);
    return self->texture;
}

const gchar *
gsword_image_view_get_path(GswordImageView *self)
{
    g_return_val_if_fail(GSWORD_IS_IMAGE_VIEW(self), NULL);
    return self->source_path;
}

gboolean
gsword_image_view_has_image(GswordImageView *self)
{
    g_return_val_if_fail(GSWORD_IS_IMAGE_VIEW(self), FALSE);
    return self->texture != NULL;
}

gboolean
gsword_image_view_load(GswordImageView *self, const gchar *path,
                       GError **error)
{
    GFile *file;
    GdkTexture *texture;

    g_return_val_if_fail(GSWORD_IS_IMAGE_VIEW(self), FALSE);
    g_return_val_if_fail(path != NULL, FALSE);
    file = g_file_new_for_path(path);
    texture = gdk_texture_new_from_file(file, error);
    g_object_unref(file);
    if (texture == NULL)
        return FALSE;

    g_clear_object(&self->texture);
    g_free(self->source_path);
    self->texture = texture;
    self->source_path = g_strdup(path);
    self->image_width = gdk_texture_get_width(texture);
    self->image_height = gdk_texture_get_height(texture);
    self->zoom = 1.0;
    self->zoom_mode = IMAGE_ZOOM_FIT_WIDTH;
    self->offset_x = 0.0;
    self->offset_y = 0.0;
    gtk_widget_queue_resize(GTK_WIDGET(self));
    update_geometry(self, gtk_widget_get_width(GTK_WIDGET(self)),
                    gtk_widget_get_height(GTK_WIDGET(self)));
    emit_zoom_changed(self);
    return TRUE;
}

void
gsword_image_view_clear(GswordImageView *self)
{
    g_return_if_fail(GSWORD_IS_IMAGE_VIEW(self));
    g_clear_object(&self->texture);
    g_clear_pointer(&self->source_path, g_free);
    self->image_width = 0;
    self->image_height = 0;
    self->zoom = 1.0;
    self->zoom_mode = IMAGE_ZOOM_FIT_WIDTH;
    self->offset_x = 0.0;
    self->offset_y = 0.0;
    emit_zoom_changed(self);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

static gboolean
on_scroll(GtkEventControllerScroll *controller, gdouble dx, gdouble dy,
          gpointer user_data)
{
    GswordImageView *self = user_data;
    GdkEvent *event;
    GdkModifierType state = 0;

    if (self->texture == NULL)
        return FALSE;
    event = gtk_event_controller_get_current_event(
        GTK_EVENT_CONTROLLER(controller));
    if (event != NULL)
        state = gdk_event_get_modifier_state(event);
    if ((state & GDK_CONTROL_MASK) != 0) {
        gdouble factor = dy < 0.0 ? IMAGE_ZOOM_STEP
                                  : 1.0 / IMAGE_ZOOM_STEP;

        zoom_to_anchor(self, self->zoom * factor,
                       self->pointer_x, self->pointer_y);
        return TRUE;
    }
    self->offset_x -= dx * 40.0;
    self->offset_y -= dy * 40.0;
    clamp_offsets(self, gtk_widget_get_width(GTK_WIDGET(self)),
                  gtk_widget_get_height(GTK_WIDGET(self)));
    gtk_widget_queue_draw(GTK_WIDGET(self));
    return TRUE;
}

static void
on_motion(GtkEventControllerMotion *controller, gdouble x, gdouble y,
          gpointer user_data)
{
    GswordImageView *self = user_data;

    (void)controller;
    self->pointer_x = x;
    self->pointer_y = y;
}

static void
on_drag_begin(GtkGestureDrag *gesture, gdouble start_x, gdouble start_y,
              gpointer user_data)
{
    GswordImageView *self = user_data;

    (void)gesture;
    (void)start_x;
    (void)start_y;
    self->drag_origin_x = self->offset_x;
    self->drag_origin_y = self->offset_y;
}

static void
on_drag_update(GtkGestureDrag *gesture, gdouble offset_x, gdouble offset_y,
               gpointer user_data)
{
    GswordImageView *self = user_data;

    (void)gesture;
    self->offset_x = self->drag_origin_x + offset_x;
    self->offset_y = self->drag_origin_y + offset_y;
    clamp_offsets(self, gtk_widget_get_width(GTK_WIDGET(self)),
                  gtk_widget_get_height(GTK_WIDGET(self)));
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void
on_click(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y,
         gpointer user_data)
{
    GswordImageView *self = user_data;

    (void)x;
    (void)y;
    if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture)) !=
        GDK_BUTTON_PRIMARY)
        return;
    gtk_widget_grab_focus(GTK_WIDGET(self));
    if (n_press == 2) {
        if (self->zoom_mode == IMAGE_ZOOM_FREE &&
            nearly_equal(self->zoom, 1.0)) {
            gsword_image_view_fit_width(self);
        } else {
            gsword_image_view_actual_size(self);
        }
    }
}

static gboolean
on_key_pressed(GtkEventControllerKey *controller, guint keyval,
               guint keycode, GdkModifierType state, gpointer user_data)
{
    GswordImageView *self = user_data;
    gint width;
    gint height;

    (void)controller;
    (void)keycode;
    if (self->texture == NULL ||
        (state & (GDK_CONTROL_MASK | GDK_ALT_MASK)) != 0)
        return FALSE;
    width = gtk_widget_get_width(GTK_WIDGET(self));
    height = gtk_widget_get_height(GTK_WIDGET(self));
    if (keyval == GDK_KEY_plus || keyval == GDK_KEY_equal ||
        keyval == GDK_KEY_KP_Add) {
        zoom_to_anchor(self, self->zoom * IMAGE_ZOOM_STEP,
                       width / 2.0, height / 2.0);
        return TRUE;
    }
    if (keyval == GDK_KEY_minus || keyval == GDK_KEY_KP_Subtract) {
        zoom_to_anchor(self, self->zoom / IMAGE_ZOOM_STEP,
                       width / 2.0, height / 2.0);
        return TRUE;
    }
    if (keyval == GDK_KEY_0) {
        gsword_image_view_fit_window(self);
        return TRUE;
    }
    if (keyval == GDK_KEY_w || keyval == GDK_KEY_W) {
        gsword_image_view_fit_width(self);
        return TRUE;
    }
    if (keyval == GDK_KEY_1) {
        gsword_image_view_actual_size(self);
        return TRUE;
    }
    if (keyval == GDK_KEY_Left) {
        self->offset_x += 40.0;
        clamp_offsets(self, width, height);
        gtk_widget_queue_draw(GTK_WIDGET(self));
        return TRUE;
    }
    if (keyval == GDK_KEY_Right) {
        self->offset_x -= 40.0;
        clamp_offsets(self, width, height);
        gtk_widget_queue_draw(GTK_WIDGET(self));
        return TRUE;
    }
    if (keyval == GDK_KEY_Up) {
        self->offset_y += 40.0;
        clamp_offsets(self, width, height);
        gtk_widget_queue_draw(GTK_WIDGET(self));
        return TRUE;
    }
    if (keyval == GDK_KEY_Down) {
        self->offset_y -= 40.0;
        clamp_offsets(self, width, height);
        gtk_widget_queue_draw(GTK_WIDGET(self));
        return TRUE;
    }
    return FALSE;
}

static void
gsword_image_view_dispose(GObject *object)
{
    GswordImageView *self = GSWORD_IMAGE_VIEW(object);

    g_clear_object(&self->texture);
    g_clear_pointer(&self->source_path, g_free);
    G_OBJECT_CLASS(gsword_image_view_parent_class)->dispose(object);
}

static void
gsword_image_view_class_init(GswordImageViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->dispose = gsword_image_view_dispose;
    widget_class->snapshot = gsword_image_view_snapshot;
    widget_class->size_allocate = gsword_image_view_size_allocate;
    signals[ZOOM_CHANGED] = g_signal_new(
        "zoom-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gsword_image_view_init(GswordImageView *self)
{
    GtkEventController *controller;

    self->zoom = 1.0;
    self->zoom_mode = IMAGE_ZOOM_FIT_WIDTH;
    gtk_widget_set_hexpand(GTK_WIDGET(self), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(self), TRUE);
    gtk_widget_set_focusable(GTK_WIDGET(self), TRUE);

    controller = gtk_event_controller_scroll_new(
        GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect(controller, "scroll", G_CALLBACK(on_scroll), self);
    gtk_widget_add_controller(GTK_WIDGET(self), controller);

    controller = gtk_event_controller_motion_new();
    g_signal_connect(controller, "motion", G_CALLBACK(on_motion), self);
    gtk_widget_add_controller(GTK_WIDGET(self), controller);

    {
        GtkGesture *drag = gtk_gesture_drag_new();

        g_signal_connect(drag, "drag-begin", G_CALLBACK(on_drag_begin), self);
        g_signal_connect(drag, "drag-update", G_CALLBACK(on_drag_update), self);
        gtk_widget_add_controller(GTK_WIDGET(self),
                                  GTK_EVENT_CONTROLLER(drag));
    }
    {
        GtkGesture *click = gtk_gesture_click_new();

        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click),
                                      GDK_BUTTON_PRIMARY);
        g_signal_connect(click, "pressed", G_CALLBACK(on_click), self);
        gtk_widget_add_controller(GTK_WIDGET(self),
                                  GTK_EVENT_CONTROLLER(click));
    }

    controller = gtk_event_controller_key_new();
    g_signal_connect(controller, "key-pressed", G_CALLBACK(on_key_pressed),
                     self);
    gtk_widget_add_controller(GTK_WIDGET(self), controller);
}

GtkWidget *
gsword_image_view_new(void)
{
    return g_object_new(GSWORD_TYPE_IMAGE_VIEW, NULL);
}
