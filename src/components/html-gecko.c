/*
 *  Copyright (C) 2010 Ji YongGang <jungleji@gmail.com>
 *
 *  ChmSee is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.

 *  ChmSee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with ChmSee; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include <string.h>
#include <gtkmozembed.h>

#include "ihtml.h"
#include "html-gecko.h"
#include "marshal.h"
#include "utils.h"
#include "gecko-utils.h"

typedef struct _CsHtmlGeckoPrivate CsHtmlGeckoPrivate;

struct _CsHtmlGeckoPrivate {
        GtkMozEmbed *gecko;
        gchar       *render_name;
};

static void cs_html_gecko_class_init(CsHtmlGeckoClass *);
static void cs_html_gecko_init(CsHtmlGecko *);
static void cs_html_gecko_finalize(GObject *);
static void cs_html_gecko_interface_init(CsIhtmlInterface *);

static void gecko_title_cb(GtkMozEmbed *, CsHtmlGecko *);
static void gecko_location_cb(GtkMozEmbed *, CsHtmlGecko *);
static gboolean gecko_open_uri_cb(GtkMozEmbed *, const gchar *, CsHtmlGecko *);
static gboolean gecko_mouse_click_cb(GtkMozEmbed *, gpointer, CsHtmlGecko *);
static gboolean gecko_link_message_cb(GtkMozEmbed *, CsHtmlGecko *);
static void gecko_child_add_cb(GtkMozEmbed *, GtkWidget *, CsHtmlGecko *);
static void gecko_child_remove_cb(GtkMozEmbed *, GtkWidget *, CsHtmlGecko *);
static void gecko_child_grab_focus_cb(GtkWidget *, CsHtmlGecko *);

static void         cs_html_gecko_clear(CsHtmlGecko *);
static void         cs_html_gecko_open_uri(CsHtmlGecko *, const gchar *);

static GtkWidget   *cs_html_gecko_get_widget(CsHtmlGecko *);
static gboolean     cs_html_gecko_can_go_forward(CsHtmlGecko *);
static gboolean     cs_html_gecko_can_go_back(CsHtmlGecko *);
static void         cs_html_gecko_go_forward(CsHtmlGecko *);
static void         cs_html_gecko_go_back(CsHtmlGecko *);
static gchar       *cs_html_gecko_get_title(CsHtmlGecko *);
static gchar       *cs_html_gecko_get_location(CsHtmlGecko *);
static void         cs_html_gecko_copy_selection(CsHtmlGecko *);
static void         cs_html_gecko_select_all(CsHtmlGecko *);
static void         cs_html_gecko_increase_size(CsHtmlGecko *);
static void         cs_html_gecko_reset_size(CsHtmlGecko *);
static void         cs_html_gecko_decrease_size(CsHtmlGecko *);

/* Has the value of the URL under the mouse pointer, otherwise NULL */
static gchar *current_url = NULL;

#define CS_HTML_GECKO_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), CS_TYPE_HTML_GECKO, CsHtmlGeckoPrivate))

/* GObject functions */

/* G_DEFINE_TYPE (CsHtmlGecko, cs_html_gecko, G_TYPE_OBJECT); */
G_DEFINE_TYPE_WITH_CODE (CsHtmlGecko, cs_html_gecko, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CS_TYPE_IHTML, cs_html_gecko_interface_init));

static void
cs_html_gecko_class_init(CsHtmlGeckoClass *klass)
{
        G_OBJECT_CLASS (klass)->finalize = cs_html_gecko_finalize;
        g_type_class_add_private(klass, sizeof(CsHtmlGeckoPrivate));
}

static void
cs_html_gecko_init(CsHtmlGecko *html)
{
        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (html);
        priv->gecko = GTK_MOZ_EMBED(gtk_moz_embed_new());
        priv->render_name = g_strdup("Mozilla Gecko");

        g_signal_connect(G_OBJECT (priv->gecko),
                         "title",
                         G_CALLBACK (gecko_title_cb),
                         html);
        g_signal_connect(G_OBJECT (priv->gecko), 
                         "location",
                         G_CALLBACK (gecko_location_cb),
                         html);
        g_signal_connect(G_OBJECT (priv->gecko), 
                         "open-uri",
                         G_CALLBACK (gecko_open_uri_cb),
                         html);
        g_signal_connect(G_OBJECT (priv->gecko),
                         "dom_mouse_click",
                         G_CALLBACK (gecko_mouse_click_cb),
                         html);
        g_signal_connect(G_OBJECT (priv->gecko), 
                         "link_message",
                         G_CALLBACK (gecko_link_message_cb),
                         html);
        g_signal_connect(G_OBJECT (priv->gecko), 
                         "add",
                         G_CALLBACK (gecko_child_add_cb),
                         html);
        g_signal_connect(G_OBJECT (priv->gecko), 
                         "remove",
                         G_CALLBACK (gecko_child_remove_cb),
                         html);
}

static void
cs_html_gecko_finalize(GObject *object)
{
        CsHtmlGecko *self = CS_HTML_GECKO (object);
        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (self);

        g_free(priv->render_name);
        G_OBJECT_CLASS (cs_html_gecko_parent_class)->finalize(object);
}

static void
cs_html_gecko_interface_init(CsIhtmlInterface *iface)
{
        g_debug("CS_HTML_GECKO >>> interface init");
        iface->get_widget = (GtkWidget *(*) (CsIhtml *html)) cs_html_gecko_get_widget;

        iface->open_uri = (void (*) (CsIhtml *html, const gchar* uri)) cs_html_gecko_open_uri;

        iface->get_title    = (const gchar *(*) (CsIhtml *html)) cs_html_gecko_get_title;
        iface->get_location = (const gchar *(*) (CsIhtml *html)) cs_html_gecko_get_location;

        iface->can_go_back    = (gboolean (*) (CsIhtml *html)) cs_html_gecko_can_go_back;
        iface->can_go_forward = (gboolean (*) (CsIhtml *html)) cs_html_gecko_can_go_forward;
        iface->go_back        = (void (*) (CsIhtml *html)) cs_html_gecko_go_back;
        iface->go_forward     = (void (*) (CsIhtml *html)) cs_html_gecko_go_forward;

        iface->copy_selection = (void (*) (CsIhtml *html)) cs_html_gecko_copy_selection;
        iface->select_all     = (void (*) (CsIhtml *html)) cs_html_gecko_select_all;

        iface->increase_size = (void (*) (CsIhtml *html)) cs_html_gecko_increase_size;
        iface->decrease_size = (void (*) (CsIhtml *html)) cs_html_gecko_decrease_size;
        iface->reset_size    = (void (*) (CsIhtml *html)) cs_html_gecko_reset_size;

        iface->clear    = (void (*) (CsIhtml *html)) cs_html_gecko_clear;
}

/* Callbacks */

static void
gecko_title_cb(GtkMozEmbed *embed, CsHtmlGecko *html)
{
        char *new_title;

        new_title = gtk_moz_embed_get_title(embed);
        cs_ihtml_title_changed(CS_IHTML (html), new_title);
        /* g_signal_emit(html, signals[TITLE_CHANGED], 0, new_title); */
        g_free(new_title);
}

static void
gecko_location_cb(GtkMozEmbed *embed, CsHtmlGecko *html)
{
        char *location;

        location = gtk_moz_embed_get_location(embed);
        cs_ihtml_location_changed(CS_IHTML (html), location);
        /* g_signal_emit(html, signals[LOCATION_CHANGED], 0, location); */
        g_free(location);
}

static gboolean
gecko_open_uri_cb(GtkMozEmbed *embed, const gchar *uri, CsHtmlGecko *html)
{
        gboolean ret_val;

        ret_val = TRUE;

        cs_ihtml_uri_opened(CS_IHTML (html), uri);
        /* g_signal_emit(html, signals[OPEN_URI], 0, uri, &ret_val); */

        /* Reset current url */
        if (current_url != NULL) {
                g_free(current_url);
                current_url = NULL;

                cs_ihtml_link_message(CS_IHTML (html), "");
                /* g_signal_emit(html, signals[LINK_MESSAGE], 0, ""); */
        }

        return ret_val;
}

static gboolean
gecko_mouse_click_cb(GtkMozEmbed *widget, gpointer dom_event, CsHtmlGecko *html)
{
        gint button;
        gint mask;
        
        button = gecko_utils_get_mouse_event_button(dom_event);
        mask = gecko_utils_get_mouse_event_modifiers(dom_event);

        if (button == 2 || (button == 1 && mask & GDK_CONTROL_MASK)) {
                if (current_url) {
                        cs_ihtml_open_new_tab(CS_IHTML (html), current_url);
                        /* g_signal_emit(html, signals[OPEN_NEW_TAB], 0, current_url); */

                        return TRUE;
                }
        } else if (button == 3) {
                if (current_url)
                        cs_ihtml_context_link(CS_IHTML (html), current_url);
                        /* g_signal_emit(html, signals[CONTEXT_LINK], 0, current_url); */
                else
                        cs_ihtml_context_normal(CS_IHTML (html));
                        /* g_signal_emit(html, signals[CONTEXT_NORMAL], 0); */

                return TRUE;
        }

        return FALSE;
}

static gboolean
gecko_link_message_cb(GtkMozEmbed *widget, CsHtmlGecko *html)
{
        g_free(current_url);

        current_url = gtk_moz_embed_get_link_message(widget);
        cs_ihtml_link_message(CS_IHTML (html), current_url);
        /* g_signal_emit(html, signals[LINK_MESSAGE], 0, current_url); */

        if (current_url[0] == '\0') {
                g_free(current_url);
                current_url = NULL;
        }

        return FALSE;
}

static void
gecko_child_add_cb(GtkMozEmbed *embed, GtkWidget *child, CsHtmlGecko *html)
{
        g_signal_connect(G_OBJECT (child), 
                         "grab-focus",
                         G_CALLBACK (gecko_child_grab_focus_cb),
                         html);
}

static void
gecko_child_remove_cb(GtkMozEmbed *embed, GtkWidget *child, CsHtmlGecko *html)
{
        g_signal_handlers_disconnect_by_func(child, gecko_child_grab_focus_cb, html);
}

static void
gecko_child_grab_focus_cb(GtkWidget *widget, CsHtmlGecko *html)
{
        GdkEvent *event;

        event = gtk_get_current_event();

        if (event == NULL)
                g_signal_stop_emission_by_name(widget, "grab-focus");
        else
                gdk_event_free(event);
}


/* Interface functions */
static void
cs_html_gecko_clear(CsHtmlGecko *html)
{
        static const char *data = "<html><body bgcolor=\"white\"></body></html>";
        
        g_return_if_fail(IS_CS_HTML_GECKO (html));
        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (html);

        gtk_moz_embed_render_data(priv->gecko, data, strlen(data), "file:///", "text/html");
}

static void
cs_html_gecko_open_uri(CsHtmlGecko *html, const gchar *str_uri)
{
        gchar *full_uri;
        
        g_return_if_fail(IS_CS_HTML_GECKO (html));
        g_return_if_fail(str_uri != NULL);

        if (str_uri[0] == '/')
                full_uri = g_strdup_printf("file://%s", str_uri);
        else
                full_uri = g_strdup(str_uri);
        
        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (html);
        gtk_moz_embed_load_url(priv->gecko, full_uri);
        g_free(full_uri);
}

static GtkWidget *
cs_html_gecko_get_widget(CsHtmlGecko *html)
{
        g_return_val_if_fail(IS_CS_HTML_GECKO (html), NULL);

        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (html);
        return GTK_WIDGET (priv->gecko);
}

static gboolean
cs_html_gecko_can_go_forward(CsHtmlGecko *html)
{
        g_return_val_if_fail(IS_CS_HTML_GECKO (html), FALSE);
        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (html);

        return gtk_moz_embed_can_go_forward(priv->gecko);
}

static gboolean
cs_html_gecko_can_go_back(CsHtmlGecko *html)
{
        g_return_val_if_fail(IS_CS_HTML_GECKO (html), FALSE);
        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (html);

        return gtk_moz_embed_can_go_back(priv->gecko);
}

static void
cs_html_gecko_go_forward(CsHtmlGecko *html)
{
        g_return_if_fail(IS_CS_HTML_GECKO (html));
        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (html);

        gtk_moz_embed_go_forward(priv->gecko);
}

static void
cs_html_gecko_go_back(CsHtmlGecko *html)
{
        g_return_if_fail(IS_CS_HTML_GECKO (html));
        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (html);

        gtk_moz_embed_go_back(priv->gecko);
}

static gchar *
cs_html_gecko_get_title(CsHtmlGecko *html)
{
        g_return_val_if_fail(IS_CS_HTML_GECKO (html), NULL);
        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (html);

        return gtk_moz_embed_get_title(priv->gecko);
}

static gchar *
cs_html_gecko_get_location(CsHtmlGecko *html)
{
        g_return_val_if_fail(IS_CS_HTML_GECKO (html), NULL);
        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (html);

        return gtk_moz_embed_get_location(priv->gecko);
}

static void
cs_html_gecko_copy_selection(CsHtmlGecko *html)
{
        g_return_if_fail(IS_CS_HTML_GECKO (html));
        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (html);

        gecko_utils_copy_selection(priv->gecko);
}

static void
cs_html_gecko_select_all(CsHtmlGecko *html)
{
        g_return_if_fail(IS_CS_HTML_GECKO (html));
        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (html);

        gecko_utils_select_all(priv->gecko);
}

static void
cs_html_gecko_increase_size(CsHtmlGecko *html)
{
        gfloat zoom;

        g_return_if_fail(IS_CS_HTML_GECKO (html));
        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (html);

        zoom = gecko_utils_get_zoom(priv->gecko);
        zoom *= 1.2;

        gecko_utils_set_zoom(priv->gecko, zoom);
}

static void
cs_html_gecko_reset_size(CsHtmlGecko *html)
{
        g_return_if_fail(IS_CS_HTML_GECKO (html));
        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (html);

        gecko_utils_set_zoom(priv->gecko, 1.0);
}

static void
cs_html_gecko_decrease_size(CsHtmlGecko *html)
{
        gfloat zoom;

        g_return_if_fail(IS_CS_HTML_GECKO (html));
        CsHtmlGeckoPrivate *priv = CS_HTML_GECKO_GET_PRIVATE (html);

        zoom = gecko_utils_get_zoom(priv->gecko);
        zoom /= 1.2;

        gecko_utils_set_zoom(priv->gecko, zoom);
}

/* External functions */

CsHtmlGecko *
cs_html_gecko_new(void)
{
        CsHtmlGecko *html;

        html = g_object_new(CS_TYPE_HTML_GECKO, NULL);

        return html;
}

gboolean 
cs_html_gecko_init_system(void)
{
        return gecko_utils_init();
}

void
cs_html_gecko_shutdown_system()
{
        gecko_utils_shutdown();
}

void
cs_html_gecko_set_default_lang(gint lang)
{
        gecko_utils_set_default_lang(lang);
}

void
cs_html_gecko_set_variable_font(const gchar *font_name)
{
        g_debug("CS_HTML_GECKO >>> set variable font %s", font_name);
        gecko_utils_set_font(GECKO_PREF_FONT_VARIABLE, font_name);
}

void
cs_html_gecko_set_fixed_font(const gchar *font_name)
{
        g_debug("CS_HTML_GECKO >>> set fixed font %s", font_name);
        gecko_utils_set_font(GECKO_PREF_FONT_FIXED, font_name);
}
