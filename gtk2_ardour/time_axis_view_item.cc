/*
    Copyright (C) 2003 Paul Davis

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

#include <utility>

#include "pbd/error.h"
#include "pbd/stacktrace.h"

#include "ardour/types.h"
#include "ardour/ardour.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/gui_thread.h"

#include "canvas/colors.h"
#include "canvas/container.h"
#include "canvas/rectangle.h"
#include "canvas/debug.h"
#include "canvas/text.h"
#include "canvas/utils.h"

#include "ardour/profile.h"

#include "ardour_ui.h"
/*
 * ardour_ui.h was moved up in the include list
 * due to a conflicting definition of 'Rect' between
 * Apple's MacTypes.h file and GTK
 */

#include "public_editor.h"
#include "time_axis_view_item.h"
#include "time_axis_view.h"
#include "utils.h"
#include "rgb_macros.h"

#include "i18n.h"

using namespace std;
using namespace Editing;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace Gtkmm2ext;

#ifdef _WIN32
    Pango::FontDescription TimeAxisViewItem::NAME_FONT("Arial Bold 10");
    Pango::FontDescription TimeAxisViewItem::FILE_INFO_FONT("Arial Bold 10");
	const double TimeAxisViewItem::NAME_HIGHLIGHT_Y_INDENT = 0.0;
#else
	const double TimeAxisViewItem::NAME_HIGHLIGHT_Y_INDENT = 1.0;
    Pango::FontDescription TimeAxisViewItem::NAME_FONT("Helvetica Bold 10");
    Pango::FontDescription TimeAxisViewItem::FILE_INFO_FONT("Helvetica Bold 10");
#endif


const double TimeAxisViewItem::NAME_HIGHLIGHT_X_OFFSET = 10.0;
const double TimeAxisViewItem::NAME_HIGHLIGHT_Y_OFFSET = 5.0;
const double TimeAxisViewItem::GRAB_HANDLE_TOP = 2.0;
const double TimeAxisViewItem::GRAB_HANDLE_WIDTH = 5.0;
const double TimeAxisViewItem::RIGHT_EDGE_SHIFT = 0.0;

const double TimeAxisViewItem::REGION_TOP_OFFSET = 2.0;
const double TimeAxisViewItem::REGION_BOTTOM_OFFSET = 2.0;

int    TimeAxisViewItem::NAME_HEIGHT;
double TimeAxisViewItem::NAME_HIGHLIGHT_X_INDENT;
double TimeAxisViewItem::NAME_HIGHLIGHT_HEIGHT;
double TimeAxisViewItem::NAME_HIGHLIGHT_THRESH;

void
TimeAxisViewItem::set_constant_heights ()
{
        // GZ: FONT IS DEFINED AT THE BEGINNING OF THE FILE
        //NAME_FONT = get_font_for_style (X_("TimeAxisViewItemName"));
        Gtk::Window win;
        Gtk::Label foo;
        win.add (foo);
        
        int width = 0;
        int height = 0;
        
        Glib::RefPtr<Pango::Layout> layout = foo.create_pango_layout (X_("H")); /* just the ascender */
        layout->set_font_description (NAME_FONT);
        get_pixel_size (layout, width, height);
        
        NAME_HEIGHT = height;
        NAME_HIGHLIGHT_X_INDENT = width;
        
	/* Config->get_show_name_highlight) == true:
           Y_OFFSET is measured from bottom of the time axis view item.
	   Config->get_show_name_highlight) == false: 
           Y_OFFSET is measured from the top of the time axis view item.
	*/
        
        NAME_HIGHLIGHT_HEIGHT = NAME_HEIGHT + NAME_HIGHLIGHT_Y_INDENT*2;
        NAME_HIGHLIGHT_THRESH = NAME_HIGHLIGHT_HEIGHT + 2;
}

/**
 * Construct a new TimeAxisViewItem.
 *
 * @param it_name the unique name of this item
 * @param parent the parent canvas group
 * @param tv the TimeAxisView we are going to be added to
 * @param spu samples per unit
 * @param base_color
 * @param start the start point of this item
 * @param duration the duration of this item
 * @param recording true if this is a recording region view
 * @param automation true if this is an automation region view
 */
TimeAxisViewItem::TimeAxisViewItem(
	const string & it_name, ArdourCanvas::Item& parent, TimeAxisView& tv, double spu, uint32_t base_color,
	framepos_t start, framecnt_t duration, bool recording, bool automation, Visibility vis
	)
	: trackview (tv)
	, frame_position (-1)
	, item_name (it_name)
	, _height (1.0)
	, _recregion (recording)
	, _automation (automation)
	, _dragging (false)
	, _width (0.0)
{
	init (&parent, spu, base_color, start, duration, vis, true, true);
}

TimeAxisViewItem::TimeAxisViewItem (const TimeAxisViewItem& other)
	: trackable (other)
	, Selectable (other)
	, PBD::ScopedConnectionList()
	, trackview (other.trackview)
	, frame_position (-1)
	, item_name (other.item_name)
	, _height (1.0)
	, _recregion (other._recregion)
	, _automation (other._automation)
	, _dragging (other._dragging)
	, _width (0.0)
{
	/* share the other's parent, but still create a new group */

	ArdourCanvas::Item* parent = other.group->parent();
	
	_selected = other._selected;
	
	init (parent, other.samples_per_pixel, other.fill_color, other.frame_position,
	      other.item_duration, other.visibility, other.wide_enough_for_name, other.high_enough_for_name);
}

void
TimeAxisViewItem::init (ArdourCanvas::Item* parent, double fpp, uint32_t base_color, 
			framepos_t start, framepos_t duration, Visibility vis, 
			bool wide, bool high)
{
	group = new ArdourCanvas::Container (parent);
	CANVAS_DEBUG_NAME (group, string_compose ("TAVI group for %1", get_item_name()));
	group->Event.connect (sigc::mem_fun (*this, &TimeAxisViewItem::canvas_group_event));

	fill_color = base_color;
    _route_time_axis_view_color = base_color;
	samples_per_pixel = fpp;
	frame_position = start;
	item_duration = duration;
	name_connected = false;
	position_locked = false;
	max_item_duration = ARDOUR::max_framepos;
	min_item_duration = 0;
	show_vestigial = true;
	visibility = vis;
	_sensitive = true;
	name_text_width = 0;
    ioconfig_label_width = 0;
    sr_label_width = 0;
	last_item_width = 0;
	wide_enough_for_name = wide;
	high_enough_for_name = high;
        rect_visible = true;

	if (duration == 0) {
		warning << "Time Axis Item Duration == 0" << endl;
	}

	vestigial_frame = new ArdourCanvas::Rectangle (group, ArdourCanvas::Rect (0.0, 2.0, 2.0 + REGION_TOP_OFFSET, region_frame_height_from_axis_hight (trackview.current_height() ) ) );
	CANVAS_DEBUG_NAME (vestigial_frame, string_compose ("vestigial frame for %1", get_item_name()));
	vestigial_frame->hide ();
	vestigial_frame->set_outline_color (ARDOUR_UI::config()->get_canvasvar_VestigialFrame());
	vestigial_frame->set_fill_color (ARDOUR_UI::config()->get_canvasvar_VestigialFrame());

	if (visibility & ShowFrame) {
		frame = new ArdourCanvas::Rectangle (group, 
						     ArdourCanvas::Rect (0.0, REGION_TOP_OFFSET,
									 trackview.editor().sample_to_pixel(duration) + RIGHT_EDGE_SHIFT, 
									 region_frame_height_from_axis_hight (trackview.current_height() ) ) );

		CANVAS_DEBUG_NAME (frame, string_compose ("frame for %1", get_item_name()));
		
        if (_recregion) {
            frame->set_outline_what (ArdourCanvas::Rectangle::What (ArdourCanvas::Rectangle::LEFT|
                                                                    ArdourCanvas::Rectangle::BOTTOM|
                                                                    ArdourCanvas::Rectangle::TOP));
        } else {
            frame->set_outline_what (ArdourCanvas::Rectangle::What (ArdourCanvas::Rectangle::LEFT|
                                                                    ArdourCanvas::Rectangle::RIGHT|
                                                                    ArdourCanvas::Rectangle::BOTTOM|
                                                                    ArdourCanvas::Rectangle::TOP));
        }

        ArdourCanvas::Color ouline_color = ARDOUR_UI::config()->get_canvasvar_TimeAxisFrame();
        frame->set_outline_color (ouline_color);

	} else {

		frame = 0;
	}

	{ // configure name highlight rect
        uint32_t opacity = 255*0.85; //85% opacity
        name_highlight_color = RGBA_TO_UINT (0, 0, 0, opacity);
        name_highlight = new ArdourCanvas::Rectangle (group, 
                                        ArdourCanvas::Rect (NAME_HIGHLIGHT_X_OFFSET,
                                                            NAME_HIGHLIGHT_Y_OFFSET,
                                                            NAME_HIGHLIGHT_X_OFFSET + 2*NAME_HIGHLIGHT_X_INDENT,
                                                            NAME_HIGHLIGHT_HEIGHT + NAME_HIGHLIGHT_Y_OFFSET) );
        CANVAS_DEBUG_NAME (name_highlight, string_compose ("name highlight for %1", get_item_name()));
        name_highlight->set_data ("timeaxisviewitem", this);
        name_highlight->set_outline_what (ArdourCanvas::Rectangle::What (0) );
        name_highlight->set_outline_color (RGBA_TO_UINT (0,0,0,255));
	}
        
    { // configure name text
		name_text = new ArdourCanvas::Text (group);
		CANVAS_DEBUG_NAME (name_text, string_compose ("name text for %1", get_item_name()));
                
        name_text->set_position (ArdourCanvas::Duple (NAME_HIGHLIGHT_X_OFFSET + NAME_HIGHLIGHT_X_INDENT, NAME_HIGHLIGHT_Y_OFFSET + NAME_HIGHLIGHT_Y_INDENT) );
        name_text->set_font_description (NAME_FONT);
        name_text->set("");
        name_highlight->hide();
    }
    
    { // configure io config highlight
        uint32_t opacity = 255*0.85; //85% opacity
        ioconfig_highlight_color = RGBA_TO_UINT (0, 0, 0, opacity);
        // alighn with name highlight
        double x0 = name_highlight->x1 ();
        double y0 = name_highlight->y0 ();
        double x1 = x0 + 2*NAME_HIGHLIGHT_X_INDENT;
        double y1 = name_highlight->y1 ();
        ioconfig_highlight = new ArdourCanvas::Rectangle (group,
                                                      ArdourCanvas::Rect (x0, y0, x1, y1) );
        CANVAS_DEBUG_NAME (ioconfig_highlight, string_compose ("io config highlight for %1", get_item_name()));
        ioconfig_highlight->set_data ("timeaxisviewitem", this);
        ioconfig_highlight->set_outline_what (ArdourCanvas::Rectangle::What (0) );
        ioconfig_highlight->set_outline_color (RGBA_TO_UINT (0,0,0,255));
    }
    
    { // configure io config label text
        ioconfig_text = new ArdourCanvas::Text (group);
        CANVAS_DEBUG_NAME (ioconfig_text, string_compose ("ioconfig text for %1", get_item_name()));
        
        ioconfig_text->set_position (ArdourCanvas::Duple (ioconfig_highlight->x0 () /*+ NAME_HIGHLIGHT_X_INDENT*/, ioconfig_highlight->y0 () + NAME_HIGHLIGHT_Y_INDENT) );
        ioconfig_text->set_font_description (FILE_INFO_FONT);
        ioconfig_text->set_color (RGBA_TO_UINT (255,255,255,255));

        ioconfig_text->set("");
        ioconfig_text->hide();
    }
    
    { // configure SR highlight
        uint32_t opacity = 255*0.85; //85% opacity
        sr_highlight_color = RGBA_TO_UINT (0, 0, 0, opacity);
        // alighn with io config highlight
        double x0 = ioconfig_highlight->x1 ();
        double y0 = ioconfig_highlight->y0 ();
        double x1 = x0 + 2*NAME_HIGHLIGHT_X_INDENT;
        double y1 = ioconfig_highlight->y1 ();
        sample_rate_highlight = new ArdourCanvas::Rectangle (group,
                                                      ArdourCanvas::Rect (x0, y0, x1, y1) );
        CANVAS_DEBUG_NAME (sample_rate_highlight, string_compose ("sr highlight for %1",get_item_name()));
        sample_rate_highlight->set_data ("timeaxisviewitem", this);
        sample_rate_highlight->set_outline_what (ArdourCanvas::Rectangle::What (0) );
        sample_rate_highlight->set_outline_color (RGBA_TO_UINT (0,0,0,255));
    }

    { // configure SR label text
        sample_rate_text = new ArdourCanvas::Text (group);
        CANVAS_DEBUG_NAME (sample_rate_text, string_compose ("sr text for %1", get_item_name()));
        
        sample_rate_text->set_position (ArdourCanvas::Duple (sample_rate_highlight->x0 () /*+ NAME_HIGHLIGHT_X_INDENT*/, sample_rate_highlight->y0 () + NAME_HIGHLIGHT_Y_INDENT) );
        sample_rate_text->set_font_description (FILE_INFO_FONT);
        sample_rate_text->set_color(RGBA_TO_UINT (255,255,255,255) );
        
        sample_rate_text->set("");
        sample_rate_text->hide();
    }
    
	/* create our grab handles used for trimming/duration etc */
	if (!_recregion && !_automation) {
		double top   = TimeAxisViewItem::GRAB_HANDLE_TOP;
		double width = TimeAxisViewItem::GRAB_HANDLE_WIDTH;

		frame_handle_start = new ArdourCanvas::Rectangle (group, ArdourCanvas::Rect (0.0, 0.5*trackview.current_height(), width, region_frame_height_from_axis_hight (trackview.current_height() ) ) );
		CANVAS_DEBUG_NAME (frame_handle_start, "TAVI frame handle start");
		frame_handle_start->set_outline (false);
		frame_handle_start->set_fill (false);
		frame_handle_start->Event.connect (sigc::bind (sigc::mem_fun (*this, &TimeAxisViewItem::frame_handle_crossing), frame_handle_start));

		frame_handle_end = new ArdourCanvas::Rectangle (group, ArdourCanvas::Rect (0.0, 0.5*trackview.current_height(), width, region_frame_height_from_axis_hight (trackview.current_height() ) ) );

		CANVAS_DEBUG_NAME (frame_handle_end, "TAVI frame handle end");
		frame_handle_end->set_outline (false);
		frame_handle_end->set_fill (false);
		frame_handle_end->Event.connect (sigc::bind (sigc::mem_fun (*this, &TimeAxisViewItem::frame_handle_crossing), frame_handle_end));
	} else {
		frame_handle_start = frame_handle_end = 0;
	}

	set_color (base_color);

	set_duration (item_duration, this);
	set_position (start, this);

	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&TimeAxisViewItem::parameter_changed, this, _1), gui_context ());
    
    ARDOUR_UI::config()->ParameterChanged.connect_same_thread (*this, boost::bind (&TimeAxisViewItem::parameter_changed, this, _1) );
}

TimeAxisViewItem::~TimeAxisViewItem()
{
	delete group;
}

bool
TimeAxisViewItem::canvas_group_event (GdkEvent* /*ev*/)
{
	return false;
}

void
TimeAxisViewItem::hide_rect ()
{
        rect_visible = false;
        set_frame_color ();

        if (name_highlight) {
            name_highlight->set_outline_what (ArdourCanvas::Rectangle::What (0));
            name_highlight->set_fill_color (UINT_RGBA_CHANGE_A (fill_color, 64));
        }
    
        if (ioconfig_highlight) {
            ioconfig_highlight->set_outline_what (ArdourCanvas::Rectangle::What (0));
            ioconfig_highlight->set_fill_color (UINT_RGBA_CHANGE_A (fill_color, 64));
        }
    
        if (sample_rate_highlight) {
            sample_rate_highlight->set_outline_what (ArdourCanvas::Rectangle::What (0));
            sample_rate_highlight->set_fill_color (UINT_RGBA_CHANGE_A (fill_color, 64));
        }
}

void
TimeAxisViewItem::show_rect ()
{
        rect_visible = true;
        set_frame_color ();

        if (name_highlight) {
            name_highlight->set_outline_what (ArdourCanvas::Rectangle::What (0));
            name_highlight->set_fill_color (name_highlight_color);
        }
    
        if (ioconfig_highlight) {
            ioconfig_highlight->set_outline_what (ArdourCanvas::Rectangle::What (0));
            ioconfig_highlight->set_fill_color (ioconfig_highlight_color);
        }
        
        if (sample_rate_highlight) {
            sample_rate_highlight->set_outline_what (ArdourCanvas::Rectangle::What (0));
            sample_rate_highlight->set_fill_color (sr_highlight_color);
        }
}

/**
 * Set the position of this item on the timeline.
 *
 * @param pos the new position
 * @param src the identity of the object that initiated the change
 * @return true on success
 */

bool
TimeAxisViewItem::set_position(framepos_t pos, void* src, double* delta)
{
	if (position_locked) {
		return false;
	}

	frame_position = pos;

	double new_unit_pos = trackview.editor().sample_to_pixel (pos);

	if (delta) {
		(*delta) = new_unit_pos - group->position().x;
		if (*delta == 0.0) {
			return true;
		}
	} else {
		if (new_unit_pos == group->position().x) {
			return true;
		}
	}

	group->set_x_position (new_unit_pos);
	PositionChanged (frame_position, src); /* EMIT_SIGNAL */

	return true;
}

/** @return position of this item on the timeline */
framepos_t
TimeAxisViewItem::get_position() const
{
	return frame_position;
}

/**
 * Set the duration of this item.
 *
 * @param dur the new duration of this item
 * @param src the identity of the object that initiated the change
 * @return true on success
 */

bool
TimeAxisViewItem::set_duration (framecnt_t dur, void* src)
{
	if ((dur > max_item_duration) || (dur < min_item_duration)) {
		warning << string_compose (
				P_("new duration %1 frame is out of bounds for %2", "new duration of %1 frames is out of bounds for %2", dur),
				get_item_name(), dur)
			<< endmsg;
		return false;
	}

	if (dur == 0) {
		group->hide();
	}

	item_duration = dur;

	double end_pixel = trackview.editor().sample_to_pixel (frame_position + dur);
	double first_pixel = trackview.editor().sample_to_pixel (frame_position);

	reset_width_dependent_items (end_pixel - first_pixel);

	DurationChanged (dur, src); /* EMIT_SIGNAL */
	return true;
}

/** @return duration of this item */
framepos_t
TimeAxisViewItem::get_duration() const
{
	return item_duration;
}

/**
 * Set the maximum duration that this item can have.
 *
 * @param dur the new maximum duration
 * @param src the identity of the object that initiated the change
 */
void
TimeAxisViewItem::set_max_duration(framecnt_t dur, void* src)
{
	max_item_duration = dur;
	MaxDurationChanged(max_item_duration, src); /* EMIT_SIGNAL */
}

/** @return the maximum duration that this item may have */
framecnt_t
TimeAxisViewItem::get_max_duration() const
{
	return max_item_duration;
}

/**
 * Set the minimum duration that this item may have.
 *
 * @param the minimum duration that this item may be set to
 * @param src the identity of the object that initiated the change
 */
void
TimeAxisViewItem::set_min_duration(framecnt_t dur, void* src)
{
	min_item_duration = dur;
	MinDurationChanged(max_item_duration, src); /* EMIT_SIGNAL */
}

/** @return the minimum duration that this item mey have */
framecnt_t
TimeAxisViewItem::get_min_duration() const
{
	return min_item_duration;
}

/**
 * Set whether this item is locked to its current position.
 * Locked items cannot be moved until the item is unlocked again.
 *
 * @param yn true to lock this item to its current position
 * @param src the identity of the object that initiated the change
 */
void
TimeAxisViewItem::set_position_locked(bool yn, void* src)
{
	position_locked = yn;
	set_trim_handle_colors();
	PositionLockChanged (position_locked, src); /* EMIT_SIGNAL */
}

/** @return true if this item is locked to its current position */
bool
TimeAxisViewItem::get_position_locked() const
{
	return position_locked;
}

/**
 * Set whether the maximum duration constraint is active.
 *
 * @param active set true to enforce the max duration constraint
 * @param src the identity of the object that initiated the change
 */
void
TimeAxisViewItem::set_max_duration_active (bool active, void* /*src*/)
{
	max_duration_active = active;
}

/** @return true if the maximum duration constraint is active */
bool
TimeAxisViewItem::get_max_duration_active() const
{
	return max_duration_active;
}

/**
 * Set whether the minimum duration constraint is active.
 *
 * @param active set true to enforce the min duration constraint
 * @param src the identity of the object that initiated the change
 */

void
TimeAxisViewItem::set_min_duration_active (bool active, void* /*src*/)
{
	min_duration_active = active;
}

/** @return true if the maximum duration constraint is active */
bool
TimeAxisViewItem::get_min_duration_active() const
{
	return min_duration_active;
}

/**
 * Set the name of this item.
 *
 * @param new_name the new name of this item
 * @param src the identity of the object that initiated the change
 */

void
TimeAxisViewItem::set_item_name(std::string new_name, void* src)
{
	if (new_name != item_name) {
		std::string temp_name = item_name;
		item_name = new_name;
		NameChanged (item_name, temp_name, src); /* EMIT_SIGNAL */
	}
}

/** @return the name of this item */
std::string
TimeAxisViewItem::get_item_name() const
{
	return item_name;
}

/**
 * Set selection status.
 *
 * @param yn true if this item is currently selected
 */
void
TimeAxisViewItem::set_selected(bool yn)
{
	if (_selected != yn) {
		Selectable::set_selected (yn);
		set_frame_color ();
		set_name_text_color ();
	}
}

/** @return the TimeAxisView that this item is on */
TimeAxisView&
TimeAxisViewItem::get_time_axis_view () const
{
	return trackview;
}

/**
 * Set the displayed item text.
 * This item is the visual text name displayed on the canvas item, this can be different to the name of the item.
 *
 * @param new_name the new name text to display
 */

void
TimeAxisViewItem::set_name_text(const string& new_name)
{
	if (!name_text) {
		return;
	}

	name_text->set (new_name);
    
    name_text->show ();
    name_text_width = name_text->text_width();
    manage_name_highlight();
}

void
TimeAxisViewItem::set_ioconfig_text(const string& new_io_text)
{
    if (!ioconfig_text) {
        return;
    }
    
    ioconfig_text->set (new_io_text);
    ioconfig_text->show(); // show to calculate the width
    ioconfig_label_width = ioconfig_text->text_width();
    manage_ioconfig_highlight();
}

void
TimeAxisViewItem::set_sr_text(const string& new_sr_text)
{
    if (!sample_rate_text) {
        return;
    }
    
    sample_rate_text->set (new_sr_text);
    sample_rate_text->show(); // show to calculate the width
    sr_label_width = sample_rate_text->text_width();
    manage_sr_highlight();
}


/**
 * Set the height of this item.
 *
 * @param h new height
 */
void
TimeAxisViewItem::set_height (double height)
{
    _height = height;

	manage_name_highlight ();

	if (frame) {
		frame->set_y1 (region_frame_height_from_axis_hight (height) );
		if (frame_handle_start) {
            // trim is available just in the bottom half of region
            frame_handle_start->set_y0 (0.5*height-REGION_BOTTOM_OFFSET);
			frame_handle_start->set_y1 (region_frame_height_from_axis_hight(height) );
            frame_handle_end->set_y0 (0.5*height-REGION_BOTTOM_OFFSET);
			frame_handle_end->set_y1 (region_frame_height_from_axis_hight (height) );
		}
	}

	vestigial_frame->set_y1 (region_frame_height_from_axis_hight (height) );

	set_colors ();
}


double TimeAxisViewItem::region_frame_height_from_axis_hight (double axis_hight)
{
    return (axis_hight - REGION_BOTTOM_OFFSET - REGION_TOP_OFFSET);
}

void
TimeAxisViewItem::manage_name_highlight ()
{
    if (!name_highlight) {
		return;
	}

	if (_height < NAME_HIGHLIGHT_THRESH) {
		high_enough_for_name = false;
	} else {
		high_enough_for_name = true;
	}

    double highlite_x1 = name_text_width + 2*NAME_HIGHLIGHT_X_INDENT + NAME_HIGHLIGHT_X_OFFSET;
        if (_width < highlite_x1) {
            highlite_x1 = _width;
        }

        if (highlite_x1 < NAME_HIGHLIGHT_X_OFFSET) {
            wide_enough_for_name = false;
        } else {
            wide_enough_for_name = true;
        }
    
    if (wide_enough_for_name && high_enough_for_name && !name_text->text().empty() ) {
		name_highlight->set (ArdourCanvas::Rect (NAME_HIGHLIGHT_X_OFFSET,
                                                 NAME_HIGHLIGHT_Y_OFFSET,
                                                 highlite_x1,
                                                 NAME_HIGHLIGHT_HEIGHT + NAME_HIGHLIGHT_Y_OFFSET) );
        name_highlight->show();
        name_highlight->raise_to_top();
    } else {
        name_highlight->hide();
    }
    
    manage_name_text ();
    
    manage_ioconfig_highlight ();
    manage_sr_highlight ();

}

void
TimeAxisViewItem::manage_ioconfig_highlight ()
{
    if (!ioconfig_highlight) {
        return;
    }
    
    if (ioconfig_highlight->x0 () >= _width) {
        ioconfig_highlight->hide();
        manage_ioconfig_text ();
        return;
    }
    
    if (_height < NAME_HIGHLIGHT_THRESH) {
        ioconfig_highlight->hide();
        manage_ioconfig_text ();
        return;
    }
    
    if (ioconfig_text->text().empty() ) {
        ioconfig_highlight->hide();
        return;
    }
    
    
#ifdef __APPLE__
    // there is a bug in pango with text width definition on MAC
    // that's why ioconfig_label_width is bigger then is in fact
    double ioconfig_x1 = ioconfig_label_width + /*NAME_HIGHLIGHT_X_INDENT/2 */+ name_highlight->x1 ();
#else
    double ioconfig_x1 = ioconfig_label_width + /*2*NAME_HIGHLIGHT_X_INDENT */+ name_highlight->x1 ();
#endif

    if (_width < ioconfig_x1) {
        ioconfig_x1 = _width;
    }
    
    ioconfig_highlight->set (ArdourCanvas::Rect (name_highlight->x1 (),
                                                 name_highlight->y0 (),
                                                 ioconfig_x1,
                                                 name_highlight->y1 () ) );
        ioconfig_highlight->show();
        ioconfig_highlight->raise_to_top();
    
    manage_ioconfig_text ();
}

void
TimeAxisViewItem::manage_sr_highlight ()
{
    if (!ioconfig_highlight) {
        return;
    }
    
    if (sample_rate_highlight->x0 () > _width) {
        sample_rate_highlight->hide();
        manage_sr_text ();
        return;
    }
    
    if (_height < NAME_HIGHLIGHT_THRESH) {
        sample_rate_highlight->hide();
        manage_sr_text ();
        return;
    }
    
    if (sample_rate_text->text().empty() ) {
        sample_rate_highlight->hide();
        return;
    }
    
#ifdef __APPLE__
    // there is a bug in pango with text width definition on MAC
    // that's why ioconfig_label_width is bigger then is in fact
    double sr_x1 = sr_label_width + NAME_HIGHLIGHT_X_INDENT/2 + ioconfig_highlight->x1 ();
#else
    double sr_x1 = sr_label_width + 2*NAME_HIGHLIGHT_X_INDENT + ioconfig_highlight->x1 ();
#endif

    if (_width < sr_x1) {
        sr_x1 = _width;
    }

    sample_rate_highlight->set (ArdourCanvas::Rect (ioconfig_highlight->x1 (),
                                                 ioconfig_highlight->y0 (),
                                                 sr_x1,
                                                 ioconfig_highlight->y1 () ) );
        sample_rate_highlight->show();
        sample_rate_highlight->raise_to_top();
    
    manage_sr_text ();
}

void
TimeAxisViewItem::restore_color_after_mute ()
{
    fill_color = _route_time_axis_view_color;
    set_colors ();    
}

void
TimeAxisViewItem::set_color (uint32_t base_color)
{
	fill_color = base_color;
	set_colors ();
}

ArdourCanvas::Item*
TimeAxisViewItem::get_canvas_frame()
{
	return frame;
}

ArdourCanvas::Item*
TimeAxisViewItem::get_canvas_group()
{
	return group;
}

ArdourCanvas::Item*
TimeAxisViewItem::get_name_highlight()
{
	return name_highlight;
}

/**
 * Convenience method to set the various canvas item colors
 */
void
TimeAxisViewItem::set_colors()
{
	set_frame_color ();

	if (name_highlight) {
		name_highlight->set_fill_color (name_highlight_color);
	}
    
    if (ioconfig_highlight) {
        ioconfig_highlight->set_fill_color (ioconfig_highlight_color);
    }
    
    if (sample_rate_highlight) {
        sample_rate_highlight->set_fill_color (sr_highlight_color);
    }

	set_name_text_color ();
	set_trim_handle_colors();
}

void
TimeAxisViewItem::update_color()
{
    set_colors ();
}

void
TimeAxisViewItem::set_name_text_color ()
{
	if (!name_text) {
		return;
	}
	
    // GZ FIXME:change in config instead of following
    uint32_t text_color = ArdourCanvas::rgba_to_color (255.0, 255.0, 255.0, 1.0);
	name_text->set_color (text_color);
}

uint32_t
TimeAxisViewItem::fill_opacity () const
{
	if (!rect_visible) {
		/* if the frame/rect is marked as invisible, then the
		 * fill should be transparent. simplest: set
		 
		 * alpha/opacity to zero.
		 */
		return 0;
	}

	if (_dragging) {
		return 130;
	}

	uint32_t col = ARDOUR_UI::config()->get_canvasvar_FrameBase();
	return UINT_RGBA_A (col);
}

uint32_t
TimeAxisViewItem::get_fill_color () const
{
    uint32_t f;
	uint32_t o;

	o = fill_opacity ();

	if (_selected) {

        f = ARDOUR_UI::config()->get_canvasvar_SelectedFrameBase();

		if (o == 0) {
			/* some condition of this item has set fill opacity to
			 * zero, but it has been selected, so use a mid-way
			 * alpha value to make it reasonably visible.
			 */
			o = 130;
		}
		
	} else {

		if (_recregion) {
			f = ARDOUR_UI::config()->get_canvasvar_RecordingRect();
		} else {
            f = fill_color;
            f = UINT_RGBA_CHANGE_A (f, (ARDOUR_UI::config()->get_canvasvar_FrameBase() & 0x000000ff));
		}
	}

	return UINT_RGBA_CHANGE_A (f, o);
}

/**
 * Sets the frame color depending on whether this item is selected
 */
void
TimeAxisViewItem::set_frame_color()
{
	if (!frame) {
		return;
	}

	uint32_t f = get_fill_color ();
	
	if (!rect_visible) {
		f = UINT_RGBA_CHANGE_A (f, 0);
	}
        
    frame->set_fill_color (f);
	set_frame_gradient ();
        
    uint32_t outline_color_rgba; //border color
    if (_selected){
        uint32_t opacity = 255; //100% opacity
        outline_color_rgba = RGBA_TO_UINT (255, 255, 255, opacity);
    }
    else{
        uint32_t opacity = 255 * 0.5; //70% opacity
        outline_color_rgba = RGBA_TO_UINT (50, 50, 50, opacity);
    }
    frame->set_outline_color (outline_color_rgba );
}

void
TimeAxisViewItem::set_frame_gradient ()
{
	ArdourCanvas::Fill::StopList stops;
	double r, g, b, a;
	ArdourCanvas::Color fill_color (get_fill_color() );
    
	/* need to get alpha value */
	ArdourCanvas::color_to_rgba (fill_color, r, g, b, a);
    
    /* set base apacity 95% */
	double lighter = 0.60;
    ArdourCanvas::Color base = ArdourCanvas::rgba_to_color (min(1.0, r+lighter), min(1.0, g+lighter), min(1.0, b+lighter), 0.3);
    /* set middle apacity 80%, add 20% to each color*/
    ArdourCanvas::Color middle  = ArdourCanvas::rgba_to_color (r+0.2, g+0.2, b+0.2, 0.8);
    /* set top apacity 65%, add 50% to each color*/
    ArdourCanvas::Color top  = ArdourCanvas::rgba_to_color (r+0.5, g+0.5, b+0.5, 0.65);
	
    /*set base color starting from the beginning*/
	stops.push_back (std::make_pair (0.0, base));
    /*set middle color starting from on top*/
	stops.push_back (std::make_pair (1.0, base));
	
	frame->set_gradient (stops, true);
}

/**
 * Set the colors of the start and end trim handle depending on object state
 */
void
TimeAxisViewItem::set_trim_handle_colors()
{
#if 1
	/* Leave them transparent for now */
	if (frame_handle_start) {
		frame_handle_start->set_fill_color (0x00000000);
		frame_handle_end->set_fill_color (0x00000000);
	}
#else
	if (frame_handle_start) {
		if (position_locked) {
			frame_handle_start->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TrimHandleLocked());
			frame_handle_end->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TrimHandleLocked());
		} else {
			frame_handle_start->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TrimHandle());
			frame_handle_end->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TrimHandle());
		}
	}
#endif
}

bool
TimeAxisViewItem::frame_handle_crossing (GdkEvent* ev, ArdourCanvas::Rectangle* item)
{
	switch (ev->type) {
	case GDK_LEAVE_NOTIFY:
		/* always hide the handle whenever we leave, no matter what mode */
		item->set_fill (false);
		break;
	case GDK_ENTER_NOTIFY:
		if (trackview.editor().effective_mouse_mode() == Editing::MouseObject &&
		    !trackview.editor().internal_editing()) {
			/* never set this to be visible in internal
			   edit mode. Note, however, that we do need to
			   undo visibility (LEAVE_NOTIFY case above) no
			   matter what the mode is.
			*/
			item->set_fill (true);
		}
		break;
	default:
		break;
	}
	return false;
}

/** @return the frames per pixel */
double
TimeAxisViewItem::get_samples_per_pixel () const
{
	return samples_per_pixel;
}

/** Set the frames per pixel of this item.
 *  This item is used to determine the relative visual size and position of this item
 *  based upon its duration and start value.
 *
 *  @param fpp the new frames per pixel
 */
void
TimeAxisViewItem::set_samples_per_pixel (double fpp)
{
	samples_per_pixel = fpp;
	set_position (this->get_position(), this);

	double end_pixel = trackview.editor().sample_to_pixel (frame_position + get_duration());
	double first_pixel = trackview.editor().sample_to_pixel (frame_position);

	reset_width_dependent_items (end_pixel - first_pixel);
}

void
TimeAxisViewItem::reset_width_dependent_items (double pixel_width)
{
	_width = pixel_width;

	manage_name_highlight ();

	if (pixel_width < 2.0) {

		if (show_vestigial) {
			vestigial_frame->show();
		}

		if (frame) {
			frame->hide();
		}

		if (frame_handle_start) {
			frame_handle_start->hide();
			frame_handle_end->hide();
		}

	} else {
		vestigial_frame->hide();

		if (frame) {
			frame->show();
			/* Note: x0 is always zero - the position is defined by
			 * the position of the group, not the frame.
			 */
			frame->set_x1 (pixel_width + RIGHT_EDGE_SHIFT);
		}

		if (frame_handle_start) {
			if (pixel_width < (3 * TimeAxisViewItem::GRAB_HANDLE_WIDTH)) {
				/*
				 * there's less than GRAB_HANDLE_WIDTH of the region between 
				 * the right-hand end of frame_handle_start and the left-hand
				 * end of frame_handle_end, so disable the handles
				 */

				frame_handle_start->hide();
				frame_handle_end->hide();
			} else {
				frame_handle_start->show();
				frame_handle_end->set_x0 (pixel_width + RIGHT_EDGE_SHIFT - (TimeAxisViewItem::GRAB_HANDLE_WIDTH));
				frame_handle_end->set_x1 (pixel_width + RIGHT_EDGE_SHIFT);
				frame_handle_end->show();
			}
		}
	}
}

void
TimeAxisViewItem::manage_name_text ()
{
	int visible_name_width;

	if (!name_text) {
		return;
	}

	if (!wide_enough_for_name || !high_enough_for_name) {
		name_text->hide ();
		return;
	}
		
	if (name_text->text().empty()) {
		name_text->hide ();
	}

	visible_name_width = name_text_width;

	if (visible_name_width > _width - NAME_HIGHLIGHT_X_OFFSET - NAME_HIGHLIGHT_X_INDENT) {
		visible_name_width = _width - NAME_HIGHLIGHT_X_OFFSET - NAME_HIGHLIGHT_X_INDENT;
	}

	if (visible_name_width < 1) {
		name_text->hide ();
	} else {
		name_text->clamp_width (visible_name_width);
		name_text->show ();
        name_text->raise_to_top ();
	}
}

void
TimeAxisViewItem::manage_ioconfig_text ()
{
    if (!ioconfig_text) {
        return;
    }
    
    if (!ioconfig_highlight->visible() && !_selected ) {
        ioconfig_text->hide ();
        return;
    }
    
    if (ioconfig_text->text().empty()) {
        ioconfig_text->hide ();
    }
    
    int visible_ioconfig_width = ioconfig_label_width;
    
    if (visible_ioconfig_width > _width - ioconfig_highlight->x0() /*- NAME_HIGHLIGHT_X_INDENT*/) {
        visible_ioconfig_width = _width - ioconfig_highlight->x0() /*- NAME_HIGHLIGHT_X_INDENT*/;
    }
    
    if (visible_ioconfig_width < 1) {
        ioconfig_text->hide ();
    } else {
        ioconfig_text->set_position (ArdourCanvas::Duple (ioconfig_highlight->x0 () /*+ NAME_HIGHLIGHT_X_INDENT*/, ioconfig_highlight->y0 () + NAME_HIGHLIGHT_Y_INDENT) );
        ioconfig_text->clamp_width (visible_ioconfig_width);
        ioconfig_text->show ();
        ioconfig_text->raise_to_top ();
    }
}

void
TimeAxisViewItem::manage_sr_text ()
{
    if (!sample_rate_text) {
        return;
    }
    
    if (!sample_rate_highlight->visible() && !_selected) {
        sample_rate_text->hide ();
        return;
    }
    
    if (sample_rate_text->text().empty()) {
        sample_rate_text->hide ();
    }
    
    int visible_sr_width = sr_label_width;
    
    if (visible_sr_width > _width - sample_rate_highlight->x0() /*- NAME_HIGHLIGHT_X_INDENT*/) {
        visible_sr_width = _width - sample_rate_highlight->x0() /*- NAME_HIGHLIGHT_X_INDENT*/;
    }
    
    if (visible_sr_width < 1) {
        sample_rate_text->hide ();
    } else {
        sample_rate_text->set_position (ArdourCanvas::Duple (sample_rate_highlight->x0 () /*+ NAME_HIGHLIGHT_X_INDENT*/, sample_rate_highlight->y0 () + NAME_HIGHLIGHT_Y_INDENT) );
        sample_rate_text->clamp_width (visible_sr_width);
        sample_rate_text->show ();
        sample_rate_text->raise_to_top ();
    }
}

/**
 * Callback used to remove this time axis item during the gtk idle loop.
 * This is used to avoid deleting the obejct while inside the remove_this_item
 * method.
 *
 * @param item the TimeAxisViewItem to remove.
 * @param src the identity of the object that initiated the change.
 */
gint
TimeAxisViewItem::idle_remove_this_item(TimeAxisViewItem* item, void* src)
{
	item->ItemRemoved (item->get_item_name(), src); /* EMIT_SIGNAL */
	delete item;
	item = 0;
	return false;
}

void
TimeAxisViewItem::set_y (double y)
{
	group->set_y_position (y);
}

void
TimeAxisViewItem::parameter_changed (string p)
{
	if (p == "color-regions-using-track-color") {
		set_colors ();
	} else if (p == "timeline-item-gradient-depth") {
		set_frame_gradient ();
	}
}

void
TimeAxisViewItem::drag_start ()
{
	_dragging = true;
	set_frame_color ();
}

void
TimeAxisViewItem::drag_end ()
{
	_dragging = false;
	set_frame_color ();
}
