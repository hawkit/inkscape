#define __OBJECT_PROPERTIES_C__

/**
 * \brief  Fill, stroke, and stroke style dialog
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Frank Felfe <innerspace@iname.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Johan Engelen <goejendaagh@zonnet.nl>
 *
 * Copyright (C) 1999-2006 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <gtk/gtk.h>

#include <glibmm/i18n.h>
#include "helper/window.h"
#include "widgets/sp-widget.h"
#include "widgets/icon.h"
#include "desktop.h"
#include "macros.h"
#include "inkscape.h"
#include "fill-style.h"
#include "stroke-style.h"
#include "dialog-events.h"
#include "verbs.h"
#include "interface.h"
#include "style.h"
#include "inkscape-stock.h"
#include "prefs-utils.h"
#include "svg/css-ostringstream.h"
#include "sp-gaussian-blur.h"
#include "sp-filter.h"
#include "filter-chemistry.h"
#include "desktop-handles.h"
#include "desktop-style.h"
#include "document.h"
#include "document-private.h"
#include <selection.h>
#include <ui/dialog/dialog-manager.h>
#include "ui/widget/filter-effect-chooser.h"
#include "xml/repr.h"
#include "display/sp-canvas.h"

#define MIN_ONSCREEN_DISTANCE 50

static GtkWidget *dlg = NULL;
static win_data wd;

// impossible original values to make sure they are read from prefs
static gint x = -1000, y = -1000, w = 0, h = 0;
static gchar *prefs_path = "dialogs.fillstroke";

static void sp_fillstroke_selection_modified ( Inkscape::Application *inkscape, Inkscape::Selection *selection, guint flags, GtkObject *base );
static void sp_fillstroke_selection_changed ( Inkscape::Application *inkscape, Inkscape::Selection *selection, GtkObject *base );
static void sp_fillstroke_opacity_changed (GtkAdjustment *a, SPWidget *dlg);

using Inkscape::UI::Widget::SimpleFilterModifier;
static void sp_fillstroke_blend_blur_changed (SimpleFilterModifier *);

static void
sp_object_properties_dialog_destroy (GtkObject *object, gpointer data)
{
    sp_signal_disconnect_by_data (INKSCAPE, dlg);
    wd.win = dlg = NULL;
    wd.stop = 0;
}

static gboolean
sp_object_properties_dialog_delete ( GtkObject *object,
                                     GdkEvent *event,
                                     gpointer data )
{

    gtk_window_get_position ((GtkWindow *) dlg, &x, &y);
    gtk_window_get_size ((GtkWindow *) dlg, &w, &h);

    if (x<0) x=0;
    if (y<0) y=0;

    prefs_set_int_attribute (prefs_path, "x", x);
    prefs_set_int_attribute (prefs_path, "y", y);
    prefs_set_int_attribute (prefs_path, "w", w);
    prefs_set_int_attribute (prefs_path, "h", h);

    return FALSE; // which means, go ahead and destroy it

}


void
sp_object_properties_page( GtkWidget *nb,
                           GtkWidget *page,
                           char *label,
                           char *dlg_name,
                           char *label_image )
{
    GtkWidget *hb, *l, *px;

    hb = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (hb);

    px = sp_icon_new( Inkscape::ICON_SIZE_DECORATION, label_image );
    gtk_widget_show (px);
    gtk_box_pack_start (GTK_BOX (hb), px, FALSE, FALSE, 2);

    l = gtk_label_new_with_mnemonic (label);
    gtk_widget_show (l);
    gtk_box_pack_start (GTK_BOX (hb), l, FALSE, FALSE, 0);

    gtk_widget_show (page);
    gtk_notebook_append_page (GTK_NOTEBOOK (nb), page, hb);
    gtk_object_set_data (GTK_OBJECT (dlg), dlg_name, page);
}

void
sp_object_properties_dialog (void)
{
    if (!dlg) {
        gchar title[500];
        sp_ui_dialog_title_string (Inkscape::Verb::get(SP_VERB_DIALOG_FILL_STROKE), title);

        dlg = sp_window_new (title, TRUE);
        if (x == -1000 || y == -1000) {
            x = prefs_get_int_attribute (prefs_path, "x", -1000);
            y = prefs_get_int_attribute (prefs_path, "y", -1000);
        }
        if (w ==0 || h == 0) {
            w = prefs_get_int_attribute (prefs_path, "w", 0);
            h = prefs_get_int_attribute (prefs_path, "h", 0);
        }
        
//        if (x<0) x=0;
//        if (y<0) y=0;

        if (w && h) gtk_window_resize ((GtkWindow *) dlg, w, h);
        if (x >= 0 && y >= 0 && (x < (gdk_screen_width()-MIN_ONSCREEN_DISTANCE)) && (y < (gdk_screen_height()-MIN_ONSCREEN_DISTANCE)))
            gtk_window_move ((GtkWindow *) dlg, x, y);
        else
            gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
        sp_transientize (dlg);
        wd.win = dlg;
        wd.stop = 0;

        g_signal_connect ( G_OBJECT (INKSCAPE), "activate_desktop", G_CALLBACK (sp_transientize_callback), &wd );

        gtk_signal_connect ( GTK_OBJECT (dlg), "event", GTK_SIGNAL_FUNC (sp_dialog_event_handler), dlg );

        gtk_signal_connect ( GTK_OBJECT (dlg), "destroy", G_CALLBACK (sp_object_properties_dialog_destroy), dlg );
        gtk_signal_connect ( GTK_OBJECT (dlg), "delete_event", G_CALLBACK (sp_object_properties_dialog_delete), dlg );
        g_signal_connect ( G_OBJECT (INKSCAPE), "shut_down", G_CALLBACK (sp_object_properties_dialog_delete), dlg );

        g_signal_connect ( G_OBJECT (INKSCAPE), "dialogs_hide", G_CALLBACK (sp_dialog_hide), dlg );
        g_signal_connect ( G_OBJECT (INKSCAPE), "dialogs_unhide", G_CALLBACK (sp_dialog_unhide), dlg );

        GtkWidget *vb = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (vb);
        gtk_container_add (GTK_CONTAINER (dlg), vb);

        GtkWidget *nb = gtk_notebook_new ();
        gtk_widget_show (nb);
        gtk_box_pack_start (GTK_BOX (vb), nb, TRUE, TRUE, 0);
        gtk_object_set_data (GTK_OBJECT (dlg), "notebook", nb);

        /* Fill page */
        {
            GtkWidget *page = sp_fill_style_widget_new ();
            sp_object_properties_page(nb, page, _("_Fill"), "fill",
                                      INKSCAPE_STOCK_PROPERTIES_FILL_PAGE);
        }

        /* Stroke paint page */
        {
            GtkWidget *page = sp_stroke_style_paint_widget_new ();
            sp_object_properties_page(nb, page, _("Stroke _paint"), "stroke-paint",
                                      INKSCAPE_STOCK_PROPERTIES_STROKE_PAINT_PAGE);
        }

        /* Stroke style page */
        {
            GtkWidget *page = sp_stroke_style_line_widget_new ();
            sp_object_properties_page(nb, page, _("Stroke st_yle"), "stroke-line",
                                      INKSCAPE_STOCK_PROPERTIES_STROKE_PAGE);
        }


        /* Filter Effects (gtkmm) */
        GtkWidget *al_fe = gtk_alignment_new(1, 1, 1, 1);
        gtk_alignment_set_padding(GTK_ALIGNMENT(al_fe), 0, 0, 4, 0);
        SimpleFilterModifier *cb_fe = Gtk::manage(new SimpleFilterModifier);
        g_object_set_data(G_OBJECT(dlg), "filter_modifier", cb_fe);
        cb_fe->signal_selection_changed().connect(
            sigc::bind(sigc::ptr_fun(sp_fillstroke_blend_blur_changed), cb_fe));
        cb_fe->signal_blend_blur_changed().connect(
            sigc::bind(sigc::ptr_fun(sp_fillstroke_blend_blur_changed), cb_fe));
        gtk_container_add(GTK_CONTAINER(al_fe), GTK_WIDGET(cb_fe->gobj()));
        
        GtkWidget *b_vb = gtk_vbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (vb), b_vb, FALSE, FALSE, 2);
        gtk_object_set_data (GTK_OBJECT (dlg), "blur", b_vb);
        gtk_box_pack_start (GTK_BOX (b_vb), al_fe, FALSE, FALSE, 0);
        gtk_widget_show_all (b_vb);


        /* Opacity */

        GtkWidget *o_vb = gtk_vbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (vb), o_vb, FALSE, FALSE, 2);
        gtk_object_set_data (GTK_OBJECT (dlg), "master_opacity", o_vb);

        GtkWidget *l_hb = gtk_hbox_new (FALSE, 0);
        GtkWidget *l = gtk_label_new_with_mnemonic (_("Master _opacity, %"));
        gtk_misc_set_alignment (GTK_MISC (l), 0.0, 1.0);
        gtk_box_pack_start (GTK_BOX (l_hb), l, FALSE, FALSE, 4);
        gtk_box_pack_start (GTK_BOX (o_vb), l_hb, FALSE, FALSE, 0);

        GtkWidget *hb = gtk_hbox_new (FALSE, 4);
        gtk_box_pack_start (GTK_BOX (o_vb), hb, FALSE, FALSE, 0);

        GtkObject *a = gtk_adjustment_new (1.0, 0.0, 100.0, 1.0, 1.0, 0.0);
        gtk_object_set_data(GTK_OBJECT(dlg), "master_opacity_adjustment", a);

        GtkWidget *s = gtk_hscale_new (GTK_ADJUSTMENT (a));
        gtk_scale_set_draw_value (GTK_SCALE (s), FALSE);
        gtk_box_pack_start (GTK_BOX (hb), s, TRUE, TRUE, 4);
        gtk_label_set_mnemonic_widget (GTK_LABEL(l), s);

        GtkWidget *sb = gtk_spin_button_new (GTK_ADJUSTMENT (a), 0.01, 1);
        gtk_box_pack_start (GTK_BOX (hb), sb, FALSE, FALSE, 0);

        gtk_signal_connect ( a, "value_changed",
                             GTK_SIGNAL_FUNC (sp_fillstroke_opacity_changed),
                             dlg );

        gtk_widget_show_all (o_vb);

        // these callbacks are only for the master opacity update; the tabs above take care of themselves
        g_signal_connect ( G_OBJECT (INKSCAPE), "change_selection", G_CALLBACK (sp_fillstroke_selection_changed), dlg );
        g_signal_connect ( G_OBJECT (INKSCAPE), "change_subselection", G_CALLBACK (sp_fillstroke_selection_changed), dlg );
        g_signal_connect ( G_OBJECT (INKSCAPE), "modify_selection", G_CALLBACK (sp_fillstroke_selection_modified), dlg );
        g_signal_connect ( G_OBJECT (INKSCAPE), "activate_desktop", G_CALLBACK (sp_fillstroke_selection_changed), dlg );

        sp_fillstroke_selection_changed(INKSCAPE, sp_desktop_selection(SP_ACTIVE_DESKTOP), NULL);

        gtk_widget_show (dlg);

    } else {
        gtk_window_present (GTK_WINDOW (dlg));
    }

} // end of sp_object_properties_dialog()

void sp_object_properties_fill (void)
{
    sp_object_properties_dialog ();
    GtkWidget *nb = (GtkWidget *)gtk_object_get_data (GTK_OBJECT (dlg), "notebook");
    gtk_notebook_set_page (GTK_NOTEBOOK (nb), 0);
}

void sp_object_properties_stroke (void)
{
    sp_object_properties_dialog ();
    GtkWidget *nb = (GtkWidget *)gtk_object_get_data (GTK_OBJECT (dlg), "notebook");
    gtk_notebook_set_page (GTK_NOTEBOOK (nb), 1);
}

void sp_object_properties_stroke_style (void)
{
    sp_object_properties_dialog ();
    GtkWidget *nb = (GtkWidget *)gtk_object_get_data (GTK_OBJECT (dlg), "notebook");
    gtk_notebook_set_page (GTK_NOTEBOOK (nb), 2);
}



static void
sp_fillstroke_selection_modified ( Inkscape::Application *inkscape,
                              Inkscape::Selection *selection,
                              guint flags,
                              GtkObject *base )
{
    sp_fillstroke_selection_changed ( inkscape, selection, base );
}


static void
sp_fillstroke_selection_changed ( Inkscape::Application *inkscape,
                              Inkscape::Selection *selection,
                              GtkObject *base )
{
    if (gtk_object_get_data (GTK_OBJECT (dlg), "blocked"))
        return;
    gtk_object_set_data (GTK_OBJECT (dlg), "blocked", GUINT_TO_POINTER (TRUE));

    GtkWidget *opa = GTK_WIDGET (gtk_object_get_data (GTK_OBJECT (dlg), "master_opacity"));
    GtkAdjustment *a = GTK_ADJUSTMENT(gtk_object_get_data(GTK_OBJECT(dlg), "master_opacity_adjustment"));

    // create temporary style
    SPStyle *query = sp_style_new ();
    // query style from desktop into it. This returns a result flag and fills query with the style of subselection, if any, or selection
    int result = sp_desktop_query_style (SP_ACTIVE_DESKTOP, query, QUERY_STYLE_PROPERTY_MASTEROPACITY);

    switch (result) {
        case QUERY_STYLE_NOTHING:
            gtk_widget_set_sensitive (opa, FALSE);
            break;
        case QUERY_STYLE_SINGLE:
        case QUERY_STYLE_MULTIPLE_AVERAGED: // TODO: treat this slightly differently
        case QUERY_STYLE_MULTIPLE_SAME: 
            gtk_widget_set_sensitive (opa, TRUE);
            gtk_adjustment_set_value(a, 100 * SP_SCALE24_TO_FLOAT(query->opacity.value));
            break;
    }

    SimpleFilterModifier *mod = (SimpleFilterModifier*)g_object_get_data(G_OBJECT(dlg), "filter_modifier");

    //query now for current filter mode and average blurring of selection
    const int blend_result = sp_desktop_query_style (SP_ACTIVE_DESKTOP, query, QUERY_STYLE_PROPERTY_BLEND);
    switch(blend_result) {
        case QUERY_STYLE_NOTHING:
            mod->set_sensitive(false);
            break;
        case QUERY_STYLE_SINGLE:
        case QUERY_STYLE_MULTIPLE_SAME:
            mod->set_blend_mode(query->filter_blend_mode.value);
            mod->set_sensitive(true);
            break;
        case QUERY_STYLE_MULTIPLE_DIFFERENT:
            // TODO: set text
            mod->set_sensitive(false);
            break;
    }

    if(blend_result == QUERY_STYLE_SINGLE || blend_result == QUERY_STYLE_MULTIPLE_SAME) {
        int blur_result = sp_desktop_query_style (SP_ACTIVE_DESKTOP, query, QUERY_STYLE_PROPERTY_BLUR);
        switch (blur_result) {
            case QUERY_STYLE_NOTHING: //no blurring
                mod->set_blur_sensitive(false);
                break;
            case QUERY_STYLE_SINGLE:
            case QUERY_STYLE_MULTIPLE_AVERAGED:
            case QUERY_STYLE_MULTIPLE_SAME: 
                NR::Maybe<NR::Rect> bbox = sp_desktop_selection(SP_ACTIVE_DESKTOP)->bounds();
                if (bbox) {
                    double perimeter = bbox->extent(NR::X) + bbox->extent(NR::Y);
                    mod->set_blur_sensitive(true);
                    //update blur widget value
                    float radius = query->filter_gaussianBlur_deviation.value;
                    float percent = radius * 400 / perimeter; // so that for a square, 100% == half side
                    mod->set_blur_value(percent);
                }
                break;
        }
    }
    
    g_free (query);
    gtk_object_set_data (GTK_OBJECT (dlg), "blocked", GUINT_TO_POINTER (FALSE));
}

static void
sp_fillstroke_opacity_changed (GtkAdjustment *a, SPWidget *base)
{
    if (gtk_object_get_data (GTK_OBJECT (dlg), "blocked"))
        return;

    gtk_object_set_data (GTK_OBJECT (dlg), "blocked", GUINT_TO_POINTER (TRUE));

    // FIXME: fix for GTK breakage, see comment in SelectedStyle::on_opacity_changed; here it results in crash 1580903
    // UPDATE: crash fixed in GTK+ 2.10.7 (bug 374378), remove this as soon as it's reasonably common
    // (though this only fixes the crash, not the multiple change events)
    sp_canvas_force_full_redraw_after_interruptions(sp_desktop_canvas(SP_ACTIVE_DESKTOP), 0);

    SPCSSAttr *css = sp_repr_css_attr_new ();

    Inkscape::CSSOStringStream os;
    os << CLAMP (a->value / 100, 0.0, 1.0);
    sp_repr_css_set_property (css, "opacity", os.str().c_str());

    sp_desktop_set_style (SP_ACTIVE_DESKTOP, css);

    sp_repr_css_attr_unref (css);

    sp_document_maybe_done (sp_desktop_document (SP_ACTIVE_DESKTOP), "fillstroke:opacity", SP_VERB_DIALOG_FILL_STROKE, 
                            _("Change opacity"));

    // resume interruptibility
    sp_canvas_end_forced_full_redraws(sp_desktop_canvas(SP_ACTIVE_DESKTOP));

    gtk_object_set_data (GTK_OBJECT (dlg), "blocked", GUINT_TO_POINTER (FALSE));
}

static void
sp_fillstroke_blend_blur_changed (SimpleFilterModifier *m)
{
    //if dialog is locked, return 
    if (gtk_object_get_data (GTK_OBJECT (dlg), "blocked"))
        return;

     //lock dialog
    gtk_object_set_data (GTK_OBJECT (dlg), "blocked", GUINT_TO_POINTER (TRUE));
  
    //get desktop
    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    if (!desktop) {
        return;
    }

    // FIXME: fix for GTK breakage, see comment in SelectedStyle::on_opacity_changed; here it results in crash 1580903
    sp_canvas_force_full_redraw_after_interruptions(sp_desktop_canvas(desktop), 0);
    
    //get current selection
    Inkscape::Selection *selection = sp_desktop_selection (desktop);

    NR::Maybe<NR::Rect> bbox = selection->bounds();
    if (!bbox) {
        return;
    }
    //get list of selected items
    GSList const *items = selection->itemList();
    //get current document
    SPDocument *document = sp_desktop_document (desktop);

    double perimeter = bbox->extent(NR::X) + bbox->extent(NR::Y);
    const Glib::ustring blendmode = m->get_blend_mode();
    double radius = m->get_blur_value() * perimeter / 400;

    SPFilter *filter = m->get_selected_filter();
    const bool remfilter = (blendmode == "normal" && radius == 0) || (blendmode == "filter" && !filter);
        
    if(blendmode != "filter" || filter) {
        //apply created filter to every selected item
        for (GSList const *i = items; i != NULL; i = i->next) {
            SPItem * item = SP_ITEM(i->data);
            SPStyle *style = SP_OBJECT_STYLE(item);
            g_assert(style != NULL);
            
            if(remfilter) {
                remove_filter (item, false);
            }
            else {
                if(blendmode != "filter")
                    filter = new_filter_simple_from_item(document, item, blendmode.c_str(), radius);
                sp_style_set_property_url (SP_OBJECT(item), "filter", SP_OBJECT(filter), false);
            }
            
            //request update
            SP_OBJECT(item)->requestDisplayUpdate(( SP_OBJECT_MODIFIED_FLAG |
                                                    SP_OBJECT_STYLE_MODIFIED_FLAG ));
        }
    }

    sp_document_maybe_done (sp_desktop_document (SP_ACTIVE_DESKTOP), "fillstroke:blur", SP_VERB_DIALOG_FILL_STROKE,  _("Change blur"));

    // resume interruptibility
    sp_canvas_end_forced_full_redraws(sp_desktop_canvas(desktop));

    gtk_object_set_data (GTK_OBJECT (dlg), "blocked", GUINT_TO_POINTER (FALSE));
}


/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99 :

