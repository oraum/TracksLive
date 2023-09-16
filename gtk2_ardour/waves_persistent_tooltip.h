/*
    Copyright (C) 2015 Waves Audio Ltd.
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __tracks_waves_persistent_tooltip_h
#define __tracks_waves_persistent_tooltip_h

#include <sigc++/trackable.h>
#include "gtkmm2ext/gtkmm2ext/visibility.h"

#include "waves_ui.h"

namespace Gtkmm2ext {
    
/** A class which offers a tooltip-like window which can be made to
*  stay open during a drag.
*/
class LIBGTKMM2EXT_API WavesPersistentTooltip : public Gtk::Window, public WavesUI
{
public:
    WavesPersistentTooltip (Gtk::Widget *, std::string xml_file = "waves_persistent_tooltip.xml", bool draggable = false);
    virtual ~WavesPersistentTooltip ();
        
    void set_tip (std::string);
    void set_font (Pango::FontDescription font);
    void set_center_alignment (bool align_to_center);
        
    virtual bool dragging () const;
        
private:
    void init ();
    bool timeout ();
    void show_tooltip ();
    bool enter (GdkEventCrossing *);
    bool leave (GdkEventCrossing *);
    bool press (GdkEventButton *);
    bool release (GdkEventButton *);
    
    /** The widget that we are providing a tooltip for */
    Gtk::Widget* _target;
    /** Our label */
    Gtk::Label& _label;
    /** allow to drag
     */
    bool _draggable;
    /** true if we are `dragging', in the sense that button 1
         is being held over _target.
     */
    bool _maybe_dragging;
    /** Connection to a timeout used to open the tooltip */
    sigc::connection _timeout;
    /** The tip text */
    std::string _tip;
    Pango::FontDescription _font;
    bool _align_to_center;
};
}
#endif