/**
 * Glyph selector dialog.
 */

/* Authors:
 *   Jon A. Cruz
 *
 * Copyright (C) 2010 Jon A. Cruz
 * Released under GNU GPL, read the file 'COPYING' for more information
 */
#ifndef SEEN_DIALOGS_GLYPHS_H
#define SEEN_DIALOGS_GLYPHS_H

#include <gtkmm/treemodel.h>
#include "ui/widget/panel.h"
#include "ui/dialog/desktop-tracker.h"


namespace Gtk {
class ComboBoxText;
class Entry;
class IconView;
class Label;
class ListStore;
}

class SPFontSelector;
class font_instance;


namespace Inkscape {
namespace UI {

class PreviewHolder;

namespace Dialog {

class GlyphColumns;

/**
 * A panel that displays character glyphs.
 */

class GlyphsPanel : public Inkscape::UI::Widget::Panel
{
public:
    GlyphsPanel();
    virtual ~GlyphsPanel();

    static GlyphsPanel& getInstance();

    virtual void setDesktop(SPDesktop *desktop);

protected:

private:
    GlyphsPanel(GlyphsPanel const &); // no copy
    GlyphsPanel &operator=(GlyphsPanel const &); // no assign

    static GlyphColumns *getColumns();

    static void fontChangeCB(SPFontSelector *fontsel, font_instance *font, GlyphsPanel *self);

    void rebuild();

    void glyphActivated(Gtk::TreeModel::Path const & path);
    void glyphSelectionChanged();
    void setTargetDesktop(SPDesktop *desktop);


    Glib::RefPtr<Gtk::ListStore> store;
    Gtk::IconView *iconView;
    Gtk::Entry *entry;
    Gtk::Label *label;
#if GLIB_CHECK_VERSION(2,14,0)
    Gtk::ComboBoxText *scriptCombo;
#endif //GLIB_CHECK_VERSION(2,14,0)
    SPFontSelector *fsel;
    SPDesktop *targetDesktop;
    DesktopTracker deskTrack;
    sigc::connection iconActiveConn;
    sigc::connection iconSelectConn;
    sigc::connection scriptSelectConn;
    sigc::connection desktopChangeConn;
};


} // namespace Dialogs
} // namespace UI
} // namespace Inkscape

#endif // SEEN_DIALOGS_GLYPHS_H
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
