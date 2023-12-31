/*
    Copyright (C) 2001-2006 Paul Davis

    This program is free software; you can r>edistribute it and/or modify
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

#include <cmath>
#include <cassert>
#include <algorithm>
#include <vector>

#include <boost/scoped_array.hpp>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include "ardour/playlist.h"
#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/engine_state_controller.h"

#include "pbd/memento_command.h"
#include "pbd/stacktrace.h"

#include "evoral/Curve.hpp"

#include "canvas/colors.h"
#include "canvas/rectangle.h"
#include "canvas/polygon.h"
#include "canvas/poly_line.h"
#include "canvas/line.h"
#include "canvas/text.h"
#include "canvas/xfade_curve.h"
#include "canvas/debug.h"
#include "canvas/utils.h"

#include "streamview.h"
#include "audio_region_view.h"
#include "audio_time_axis.h"
#include "public_editor.h"
#include "audio_streamview.h"
#include "region_gain_line.h"
#include "control_point.h"
#include "ghostregion.h"
#include "audio_time_axis.h"
#include "rgb_macros.h"
#include "gui_thread.h"
#include "ardour_ui.h"

#include "i18n.h"

#define MUTED_ALPHA 32 // NP: do not show waveform if region is muted

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace ArdourCanvas;

static const int32_t sync_mark_width = 9;
static double const handle_size = 10; /* height of fade handles */

AudioRegionView::AudioRegionView (ArdourCanvas::Container *parent, RouteTimeAxisView &tv, boost::shared_ptr<AudioRegion> r, double spu,
				  uint32_t basic_color)
	: RegionView (parent, tv, r, spu, basic_color)
	, sync_mark(0)
	, fade_in_handle(0)
	, fade_out_handle(0)
	, start_xfade_curve (0)
	, start_xfade_rect (0)
	, _start_xfade_visible (false)
	, end_xfade_curve (0)
	, end_xfade_rect (0)
	, _end_xfade_visible (false)
	, _amplitude_above_axis(1.0)
	, trim_fade_in_drag_active(false)
	, trim_fade_out_drag_active(false)
{
	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&AudioRegionView::parameter_changed, this, _1), gui_context());
}

AudioRegionView::AudioRegionView (ArdourCanvas::Container *parent, RouteTimeAxisView &tv, boost::shared_ptr<AudioRegion> r, double spu,
				  uint32_t basic_color, bool recording, TimeAxisViewItem::Visibility visibility)
	: RegionView (parent, tv, r, spu, basic_color, recording, visibility)
	, sync_mark(0)
	, fade_in_handle(0)
	, fade_out_handle(0)
	, start_xfade_curve (0)
	, start_xfade_rect (0)
	, _start_xfade_visible (false)
	, end_xfade_curve (0)
	, end_xfade_rect (0)
	, _end_xfade_visible (false)
	, _amplitude_above_axis(1.0)
	, trim_fade_in_drag_active(false)
	, trim_fade_out_drag_active(false)
{
	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&AudioRegionView::parameter_changed, this, _1), gui_context());
}

AudioRegionView::AudioRegionView (const AudioRegionView& other, boost::shared_ptr<AudioRegion> other_region)
	: RegionView (other, boost::shared_ptr<Region> (other_region))
	, fade_in_handle(0)
	, fade_out_handle(0)
	, start_xfade_curve (0)
	, start_xfade_rect (0)
	, _start_xfade_visible (false)
	, end_xfade_curve (0)
	, end_xfade_rect (0)
	, _end_xfade_visible (false)
	, _amplitude_above_axis (other._amplitude_above_axis)
	, trim_fade_in_drag_active(false)
	, trim_fade_out_drag_active(false)
{
	init (true);

	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&AudioRegionView::parameter_changed, this, _1), gui_context());
}

void
AudioRegionView::init (bool wfd)
{
	// FIXME: Some redundancy here with RegionView::init.  Need to figure out
	// where order is important and where it isn't...

	RegionView::init (wfd);

	_amplitude_above_axis = 1.0;

	create_waves ();

	if (!_recregion) {
		fade_in_handle = new ArdourCanvas::Rectangle (group);
		CANVAS_DEBUG_NAME (fade_in_handle, string_compose ("fade in handle for %1", region()->name()));
		fade_in_handle->set_outline_color (ArdourCanvas::rgba_to_color (0, 0, 0, 1.0));
		fade_in_handle->set_fill_color (ARDOUR_UI::config()->get_canvasvar_InactiveFadeHandle());
		fade_in_handle->set_data ("regionview", this);
		fade_in_handle->hide ();

		fade_out_handle = new ArdourCanvas::Rectangle (group);
		CANVAS_DEBUG_NAME (fade_out_handle, string_compose ("fade out handle for %1", region()->name()));
		fade_out_handle->set_outline_color (ArdourCanvas::rgba_to_color (0, 0, 0, 1.0));
		fade_out_handle->set_fill_color (ARDOUR_UI::config()->get_canvasvar_InactiveFadeHandle());
		fade_out_handle->set_data ("regionview", this);
		fade_out_handle->hide ();

		CANVAS_DEBUG_NAME (fade_in_handle, string_compose ("fade in trim handle for %1", region()->name()));

		CANVAS_DEBUG_NAME (fade_out_handle, string_compose ("fade out trim handle for %1", region()->name()));
	}

	setup_fade_handle_positions ();

	if (!trackview.session()->config.get_show_region_fades()) {
		set_fade_visibility (false);
	}

	const string line_name = _region->name() + ":gain";

	if (!Profile->get_sae() && !Profile->get_trx() ) {
		gain_line.reset (new AudioRegionGainLine (line_name, *this, *group, audio_region()->envelope()));
	}
	
	update_envelope_visibility ();
	
    if (gain_line) {
        gain_line->reset ();
    }

	set_height (trackview.current_height());

	region_muted ();
	region_sync_changed ();

	region_resized (ARDOUR::bounds_change);

	for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		(*i)->set_duration (_region->length() / samples_per_pixel);
	}

	region_locked ();
	envelope_active_changed ();
	fade_in_active_changed ();
	fade_out_active_changed ();

	reset_width_dependent_items (_pixel_width);

	if (fade_in_handle) {
		fade_in_handle->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_fade_in_handle_event), fade_in_handle, this, false));
	}

	if (fade_out_handle) {
		fade_out_handle->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_fade_out_handle_event), fade_out_handle, this, false));
	}

	set_colors ();

	setup_waveform_visibility ();

	if (frame_handle_start) {
		frame_handle_start->raise_to_top ();
	}
	if (frame_handle_end) {
		frame_handle_end->raise_to_top ();
	}

	/* XXX sync mark drag? */
    
    update_ioconfig_label ();
    update_sample_rate_label ();
    
    EngineStateController::instance()->EngineRunning.connect (*this, invalidator (*this), boost::bind (&AudioRegionView::update_sample_rate_label, this), gui_context() );
    
    EngineStateController::instance()->SampleRateChanged.connect (*this, invalidator (*this), boost::bind (&AudioRegionView::update_sample_rate_label, this), gui_context() );
}

AudioRegionView::~AudioRegionView ()
{
	in_destructor = true;

	RegionViewGoingAway (this); /* EMIT_SIGNAL */

	for (vector<ScopedConnection*>::iterator i = _data_ready_connections.begin(); i != _data_ready_connections.end(); ++i) {
		delete *i;
	}

	for (list<std::pair<framepos_t, ArdourCanvas::Line*> >::iterator i = feature_lines.begin(); i != feature_lines.end(); ++i) {
		delete ((*i).second);
	}

	/* all waveviews etc will be destroyed when the group is destroyed */
}

boost::shared_ptr<ARDOUR::AudioRegion>
AudioRegionView::audio_region() const
{
	// "Guaranteed" to succeed...
	return boost::dynamic_pointer_cast<AudioRegion>(_region);
}

void
AudioRegionView::region_changed (const PropertyChange& what_changed)
{
	ENSURE_GUI_THREAD (*this, &AudioRegionView::region_changed, what_changed);

	RegionView::region_changed (what_changed);

	if (what_changed.contains (ARDOUR::Properties::scale_amplitude)) {
		region_scale_amplitude_changed ();
	}
	if (what_changed.contains (ARDOUR::Properties::fade_in)) {
		fade_in_changed ();
	}
	if (what_changed.contains (ARDOUR::Properties::fade_out)) {
		fade_out_changed ();
	}
	if (what_changed.contains (ARDOUR::Properties::fade_in_active)) {
		fade_in_active_changed ();
	}
	if (what_changed.contains (ARDOUR::Properties::fade_out_active)) {
		fade_out_active_changed ();
	}
	if (what_changed.contains (ARDOUR::Properties::envelope_active)) {
		envelope_active_changed ();
	}
	if (what_changed.contains (ARDOUR::Properties::valid_transients)) {
		transients_changed ();
	}
}

void
AudioRegionView::fade_in_changed ()
{
	reset_fade_in_shape ();
}

void
AudioRegionView::fade_out_changed ()
{
	reset_fade_out_shape ();
}

void
AudioRegionView::fade_in_active_changed ()
{
	if (start_xfade_rect) {
		if (audio_region()->fade_in_active()) {
			start_xfade_rect->set_fill (false);
		} else {
			start_xfade_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_InactiveCrossfade());
			start_xfade_rect->set_fill (true);
		}
	}
}

void
AudioRegionView::fade_out_active_changed ()
{
	if (end_xfade_rect) {
		if (audio_region()->fade_out_active()) {
			end_xfade_rect->set_fill (false);
		} else {	
			end_xfade_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_InactiveCrossfade());
			end_xfade_rect->set_fill (true);
		}
	}
}


void
AudioRegionView::region_scale_amplitude_changed ()
{
	for (uint32_t n = 0; n < waves.size(); ++n) {
		waves[n]->gain_changed ();
	}
}

void
AudioRegionView::region_renamed ()
{
	std::string str = RegionView::make_name ();

	set_item_name (str, this);
	set_name_text (str);
}

void
AudioRegionView::update_sample_rate_label ()
{
    boost::shared_ptr<AudioRegion> aregion = audio_region();
    
    if (!aregion) {
        set_sr_text("");
        return;
    }
    
    framecnt_t region_sr = aregion->audio_source()->sample_rate();
    framecnt_t system_sr = EngineStateController::instance()->get_current_sample_rate();
    
    if (region_sr != system_sr) {
        std::ostringstream ss;
        ss << (float)region_sr/1000.0;
        std::string sample_rate_str(ss.str() );
        set_sr_text(sample_rate_str);
    } else {
        set_sr_text("");
    }
}

void
AudioRegionView::update_ioconfig_label ()
{
    boost::shared_ptr<AudioRegion> aregion = audio_region();
    
    if (!aregion) {
        set_sr_text("");
        return;
    }
    
    uint32_t chan_count = aregion->get_related_audio_file_channel_count();
    std::string label;
    
    switch (chan_count ) {
        case 0:
            label = "File Not Found!";
            break;
        case 1:
            label = "M";
            break;
        case 2:
            label = "ST";
            break;
        default:
            std::ostringstream ss;
            ss << chan_count;
            label = ss.str();
            break;
    }
    
    set_ioconfig_text (label);
}

void
AudioRegionView::region_resized (const PropertyChange& what_changed)
{
	AudioGhostRegion* agr;

	RegionView::region_resized(what_changed);
	PropertyChange interesting_stuff;

	interesting_stuff.add (ARDOUR::Properties::start);
	interesting_stuff.add (ARDOUR::Properties::length);

	if (what_changed.contains (interesting_stuff)) {
		
		for (uint32_t n = 0; n < waves.size(); ++n) {
			waves[n]->region_resized ();
		}

		for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
			if ((agr = dynamic_cast<AudioGhostRegion*>(*i)) != 0) {

				for (vector<WaveView*>::iterator w = agr->waves.begin(); w != agr->waves.end(); ++w) {
					(*w)->region_resized ();
				}
			}
		}

		/* hide transient lines that extend beyond the region end */

		list<std::pair<framepos_t, ArdourCanvas::Line*> >::iterator l;

		for (l = feature_lines.begin(); l != feature_lines.end(); ++l) {
			if (l->first > _region->length() - 1) {
				l->second->hide();
			} else {
				l->second->show();
			}
		}
	}
}

void
AudioRegionView::reset_width_dependent_items (double pixel_width)
{
	RegionView::reset_width_dependent_items(pixel_width);
	assert(_pixel_width == pixel_width);

	if (pixel_width <= 20.0 || _height < 5.0 || !trackview.session()->config.get_show_region_fades()) {
		if (fade_in_handle)       { fade_in_handle->hide(); }
		if (fade_out_handle)      { fade_out_handle->hide(); }
		if (start_xfade_rect)     { start_xfade_rect->set_outline (false); }
		if (end_xfade_rect)       { end_xfade_rect->set_outline (false); }
	}

	AnalysisFeatureList analysis_features = _region->transients();
	AnalysisFeatureList::const_iterator i;

	list<std::pair<framepos_t, ArdourCanvas::Line*> >::iterator l;

	for (i = analysis_features.begin(), l = feature_lines.begin(); i != analysis_features.end() && l != feature_lines.end(); ++i, ++l) {

		float x_pos = trackview.editor().sample_to_pixel (*i);

		(*l).second->set (ArdourCanvas::Duple (x_pos, 2.0),
				  ArdourCanvas::Duple (x_pos, _height - 1));

		(*l).first = *i;

		(*l).second->set (ArdourCanvas::Duple (x_pos, 2.0),
				  ArdourCanvas::Duple (x_pos, _height - 1));
	}

	reset_fade_shapes ();
}

void
AudioRegionView::region_muted ()
{
	RegionView::region_muted();
	set_waveform_colors ();
}

void
AudioRegionView::setup_fade_handle_positions()
{
	/* position of fade handle offset from the top of the region view */
	double const handle_pos = 0.0;

	if (fade_in_handle) {
		fade_in_handle->set_y0 (handle_pos);
		fade_in_handle->set_y1 (handle_pos + handle_size);
	}

	if (fade_out_handle) {
		fade_out_handle->set_y0 (handle_pos);
		fade_out_handle->set_y1 (handle_pos + handle_size);
	}

}

void
AudioRegionView::setup_waveform_height_and_position (WaveView *wave, uint32_t n, uint32_t wcnt)
{
    /* the spacing between the time axis view edge and region (REGION_BOTTOMG_OFFSET and REGION_TOP_OFFSET) need to be
     factored into the wave height.
     Then we need to account for the fact that the edge of the region has a frame which
     consumes 1 extral pixel top and bottom, and we have to subtract another pixel because ... *handwaving*
     2 extra pixels for each additional channel for indend between waveforms
     */
    const gdouble region_geometry_correction = 3.0;
    const gdouble waveform_indend_correction = (wcnt-1) * 2;
    gdouble ht = (_height - (REGION_BOTTOM_OFFSET + REGION_TOP_OFFSET) - region_geometry_correction - waveform_indend_correction)/(double) wcnt;
    
    /* the coordinate space starts in the upper left, so use the TOP_OFFSET, and then add 1 to account for the
     frame around the region.
     add two pixels gap between waveforms on multi channel regions
     */
    const gdouble waveform_indend = n * 2;
    gdouble yoff = REGION_TOP_OFFSET + 1.0 + (n * ht) + waveform_indend;
    
    wave->set_height (ht);
    wave->set_y_position (yoff);
}

void
AudioRegionView::set_height (gdouble height)
{
	RegionView::set_height (height);

	uint32_t wcnt = waves.size();

	for (uint32_t n = 0; n < wcnt; ++n) {
        setup_waveform_height_and_position (waves[n], n, wcnt);
	}

	if (gain_line) {

		if ((height/wcnt) < NAME_HIGHLIGHT_THRESH) {
			gain_line->hide ();
		} else {
			update_envelope_visibility ();
		}

		gain_line->set_height ((uint32_t) rint (height) - 2);
	}

	reset_fade_shapes ();

	/* Update hights for any active feature lines */
	list<std::pair<framepos_t, ArdourCanvas::Line*> >::iterator l;

	for (l = feature_lines.begin(); l != feature_lines.end(); ++l) {

		float pos_x = trackview.editor().sample_to_pixel((*l).first);

		if (height >= NAME_HIGHLIGHT_THRESH) {
			(*l).second->set (ArdourCanvas::Duple (pos_x, 2.0),
					  ArdourCanvas::Duple (pos_x, _height - 1));
		} else {
			(*l).second->set (ArdourCanvas::Duple (pos_x, 2.0),
					  ArdourCanvas::Duple (pos_x, _height - 1));
		}
	}

	if (name_text) {
		name_text->raise_to_top();
	}

	setup_fade_handle_positions();
}

void
AudioRegionView::reset_fade_shapes ()
{
	if (!trim_fade_in_drag_active) { reset_fade_in_shape (); }
	if (!trim_fade_out_drag_active) { reset_fade_out_shape (); }
}

void
AudioRegionView::reset_fade_in_shape ()
{
	reset_fade_in_shape_width (audio_region(), (framecnt_t) audio_region()->fade_in()->back()->when);
}

void
AudioRegionView::reset_fade_in_shape_width (boost::shared_ptr<AudioRegion> ar, framecnt_t width, bool drag_active)
{
	trim_fade_in_drag_active = drag_active;
	if (fade_in_handle == 0) {
		return;
	}

	/* smallest size for a fade is 64 frames */

	width = std::max ((framecnt_t) 64, width);

	/* round here to prevent little visual glitches with sub-pixel placement */
	double const pwidth = rint (width / samples_per_pixel);
	double const handle_left = pwidth;

	/* Put the fade in handle so that its left side is at the end-of-fade line */
	fade_in_handle->set_x0 (handle_left);
	fade_in_handle->set_x1 (handle_left + handle_size);

	if (fade_in_handle->visible()) {
		//see comment for drag_start
		entered(false);
	}

	if (pwidth < 5) {
		hide_start_xfade();
		return;
	}

	if (!trackview.session()->config.get_show_region_fades()) {
		hide_start_xfade ();
		return;
	}
	
	double effective_height;

    effective_height = TimeAxisViewItem::region_frame_height_from_axis_hight (_height) - 2.0;

	/* points *MUST* be in anti-clockwise order */

	Points points;
	Points::size_type pi;
	boost::shared_ptr<const Evoral::ControlList> list (audio_region()->fade_in());
	Evoral::ControlList::const_iterator x;
	double length = list->length();

	points.assign (list->size(), Duple());

	for (x = list->begin(), pi = 0; x != list->end(); ++x, ++pi) {
		points[pi].x = 1.0 + (pwidth * ((*x)->when/length));
		points[pi].y = effective_height - ((*x)->value * effective_height) + TimeAxisViewItem::REGION_TOP_OFFSET + 1.0;
	}

	/* draw the line */

	redraw_start_xfade_to (ar, width, points, effective_height, handle_left);

	/* ensure trim handle stays on top */
	if (frame_handle_start) {
		frame_handle_start->raise_to_top();
	}
}

void
AudioRegionView::reset_fade_out_shape ()
{
	reset_fade_out_shape_width (audio_region(), (framecnt_t) audio_region()->fade_out()->back()->when);
}

void
AudioRegionView::reset_fade_out_shape_width (boost::shared_ptr<AudioRegion> ar, framecnt_t width, bool drag_active)
{
	trim_fade_out_drag_active = drag_active;
	if (fade_out_handle == 0) {
		return;
	}

	/* smallest size for a fade is 64 frames */

	width = std::max ((framecnt_t) 64, width);

	double const pwidth = rint(trackview.editor().sample_to_pixel (width));

	/* the right edge should be right on the region frame is the pixel
	 * width is zero. Hence the additional + 1.0 at the end.
	 */

	double const handle_right = rint(trackview.editor().sample_to_pixel (_region->length()) + TimeAxisViewItem::RIGHT_EDGE_SHIFT - pwidth);

	/* Put the fade out handle so that its right side is at the end-of-fade line;
	 */
	fade_out_handle->set_x0 (1 + handle_right - handle_size);
	fade_out_handle->set_x1 (1 + handle_right);

	if (fade_out_handle->visible()) {
		//see comment for drag_start
		entered(false);
	}
	/* don't show shape if its too small */

	if (pwidth < 5) {
		hide_end_xfade();
		return;
	}

	if (!trackview.session()->config.get_show_region_fades()) {
		hide_end_xfade();
		return;
	}

	double effective_height;

    effective_height = TimeAxisViewItem::region_frame_height_from_axis_hight (_height) - 2.0;
    
	/* points *MUST* be in anti-clockwise order */
	
	Points points;
	Points::size_type pi;
	boost::shared_ptr<const Evoral::ControlList> list (audio_region()->fade_out());
	Evoral::ControlList::const_iterator x;
	double length = list->length();

	points.assign (list->size(), Duple());

	for (x = list->begin(), pi = 0; x != list->end(); ++x, ++pi) {
		points[pi].x = _pixel_width - pwidth + (pwidth * ((*x)->when/length));
		points[pi].y = effective_height - ((*x)->value * effective_height) + TimeAxisViewItem::REGION_TOP_OFFSET + 1.0;
	}

	/* draw the line */

	redraw_end_xfade_to (ar, width, points, effective_height, handle_right, pwidth);

	/* ensure trim handle stays on top */
	if (frame_handle_end) {
		frame_handle_end->raise_to_top();
	}
}

framepos_t
AudioRegionView::get_fade_in_shape_width ()
{
	return audio_region()->fade_in()->back()->when;
}

framepos_t
AudioRegionView::get_fade_out_shape_width ()
{
	return audio_region()->fade_out()->back()->when;
}


void
AudioRegionView::redraw_start_xfade ()
{
	boost::shared_ptr<AudioRegion> ar (audio_region());

	if (!ar->fade_in() || ar->fade_in()->empty()) {
		return;
	}

	show_start_xfade();
	reset_fade_in_shape_width (ar, ar->fade_in()->back()->when);
}

void
AudioRegionView::redraw_start_xfade_to (boost::shared_ptr<AudioRegion> ar, framecnt_t /*width*/, Points& points, double effective_height,
					double rect_width)
{
	if (points.size() < 2) {
		return;
	}

	if (!start_xfade_curve) {
		start_xfade_curve = new ArdourCanvas::XFadeCurve (group, ArdourCanvas::XFadeCurve::Start);
		CANVAS_DEBUG_NAME (start_xfade_curve, string_compose ("xfade start out line for %1", region()->name()));
		start_xfade_curve->set_fill_color (ARDOUR_UI::config()->get_canvasvar_ActiveCrossfade());
		start_xfade_curve->set_outline_color (ARDOUR_UI::config()->get_canvasvar_CrossfadeLine());
		start_xfade_curve->set_ignore_events (true);
	}
	if (!start_xfade_rect) {
		start_xfade_rect = new ArdourCanvas::Rectangle (group);
		CANVAS_DEBUG_NAME (start_xfade_rect, string_compose ("xfade start rect for %1", region()->name()));
		start_xfade_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_CrossfadeLine());
		start_xfade_rect->set_fill (false);
		start_xfade_rect->set_outline (false);
		start_xfade_rect->set_outline_what (ArdourCanvas::Rectangle::What (ArdourCanvas::Rectangle::RIGHT));
		start_xfade_rect->set_outline_width (0.5);
		start_xfade_rect->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_start_xfade_event), start_xfade_rect, this));
		start_xfade_rect->set_data ("regionview", this);
	}

	start_xfade_rect->set (ArdourCanvas::Rect (0.0, TimeAxisViewItem::REGION_TOP_OFFSET, rect_width, effective_height + TimeAxisViewItem::REGION_TOP_OFFSET));

	/* fade out line */

	boost::shared_ptr<AutomationList> inverse = ar->inverse_fade_in ();
	Points ipoints;
	Points::size_type npoints;

	if (!inverse) {

		/* there is no explicit inverse fade in curve, so take the
		 * regular fade in curve given to use as "points" (already a
		 * set of coordinates), and convert to the inverse shape.
		 */

		npoints = points.size();
		ipoints.assign (npoints, Duple());

		for (Points::size_type i = 0, pci = 0; i < npoints; ++i, ++pci) {
			ArdourCanvas::Duple &p (ipoints[pci]);
			/* leave x-axis alone but invert with respect to y-axis */
			p.y = effective_height - points[pci].y + TimeAxisViewItem::REGION_TOP_OFFSET + 1;
		}

	} else {

		/* there is an explicit inverse fade in curve. Grab the points
		   and convert them into coordinates for the inverse fade in
		   line.
		*/

		npoints = inverse->size();
		ipoints.assign (npoints, Duple());
		
		Evoral::ControlList::const_iterator x;
		Points::size_type pi;
		double length = inverse->length();

		for (x = inverse->begin(), pi = 0; x != inverse->end(); ++x, ++pi) {
			ArdourCanvas::Duple& p (ipoints[pi]);
			p.x = 1.0 + (rect_width * ((*x)->when/length));
			p.y = effective_height - ((*x)->value * effective_height) + TimeAxisViewItem::REGION_TOP_OFFSET + 1;
		}
	}

	start_xfade_curve->set_inout (points, ipoints);

	show_start_xfade();
}

void
AudioRegionView::redraw_end_xfade ()
{
	boost::shared_ptr<AudioRegion> ar (audio_region());

	if (!ar->fade_out() || ar->fade_out()->empty()) {
		return;
	}

	show_end_xfade();
	
	reset_fade_out_shape_width (ar, ar->fade_out()->back()->when);
}

void
AudioRegionView::redraw_end_xfade_to (boost::shared_ptr<AudioRegion> ar, framecnt_t width, Points& points, double effective_height,
				      double rect_edge, double rect_width)
{
	if (points.size() < 2) {
		return;
	}

	if (!end_xfade_curve) {
		end_xfade_curve = new ArdourCanvas::XFadeCurve (group, ArdourCanvas::XFadeCurve::End);
		CANVAS_DEBUG_NAME (end_xfade_curve, string_compose ("xfade end out line for %1", region()->name()));
		end_xfade_curve->set_fill_color (ARDOUR_UI::config()->get_canvasvar_ActiveCrossfade());
		end_xfade_curve->set_outline_color (ARDOUR_UI::config()->get_canvasvar_CrossfadeLine());
		end_xfade_curve->set_ignore_events (true);
	}

	if (!end_xfade_rect) {
		end_xfade_rect = new ArdourCanvas::Rectangle (group);
		CANVAS_DEBUG_NAME (end_xfade_rect, string_compose ("xfade end rect for %1", region()->name()));
		end_xfade_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_CrossfadeLine());
		end_xfade_rect->set_fill (false);
		end_xfade_rect->set_outline (false);
		end_xfade_rect->set_outline_what (ArdourCanvas::Rectangle::What (ArdourCanvas::Rectangle::LEFT));
		end_xfade_rect->set_outline_width (0.5);
		end_xfade_rect->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_end_xfade_event), end_xfade_rect, this));
		end_xfade_rect->set_data ("regionview", this);
	}

	end_xfade_rect->set (ArdourCanvas::Rect (rect_edge, TimeAxisViewItem::REGION_TOP_OFFSET, rect_edge + rect_width + TimeAxisViewItem::RIGHT_EDGE_SHIFT, effective_height + TimeAxisViewItem::REGION_TOP_OFFSET));

	/* fade in line */

	boost::shared_ptr<AutomationList> inverse = ar->inverse_fade_out ();
	Points ipoints;
	Points::size_type npoints;

	if (!inverse) {

		/* there is no explicit inverse fade out curve, so take the
		 * regular fade out curve given to use as "points" (already a
		 * set of coordinates), and convert to the inverse shape.
		 */

		npoints = points.size();
		ipoints.assign (npoints, Duple());

		Points::size_type pci;

		for (pci = 0; pci < npoints; ++pci) {
			ArdourCanvas::Duple &p (ipoints[pci]);
			p.y = effective_height - points[pci].y + TimeAxisViewItem::REGION_TOP_OFFSET + 1;
		}

	} else {

		/* there is an explicit inverse fade out curve. Grab the points
		   and convert them into coordinates for the inverse fade out
		   line.
		*/

		npoints = inverse->size();
		ipoints.assign (npoints, Duple());
		
		const double rend = trackview.editor().sample_to_pixel (_region->length() - width);
		
		Evoral::ControlList::const_iterator x;
		Points::size_type i;
		Points::size_type pi;
		double length = inverse->length();

		for (x = inverse->begin(), i = 0, pi = 0; x != inverse->end(); ++x, ++pi, ++i) {
			ArdourCanvas::Duple& p (ipoints[pi]);
			p.x = 1.0 + (rect_width * ((*x)->when/length)) + rend;
			p.y = effective_height - ((*x)->value * effective_height) + TimeAxisViewItem::REGION_TOP_OFFSET + 1;
		}
	}

	end_xfade_curve->set_inout (ipoints, points);

	show_end_xfade();
}

void
AudioRegionView::hide_xfades ()
{
	hide_start_xfade ();
	hide_end_xfade ();
}

void
AudioRegionView::hide_start_xfade ()
{
	if (start_xfade_curve) {
		start_xfade_curve->hide();
	}
	if (start_xfade_rect) {
		start_xfade_rect->hide ();
	}

	_start_xfade_visible = false;
}

void
AudioRegionView::hide_end_xfade ()
{
	if (end_xfade_curve) {
		end_xfade_curve->hide();
	}
	if (end_xfade_rect) {
		end_xfade_rect->hide ();
	}

	_end_xfade_visible = false;
}

void
AudioRegionView::show_start_xfade ()
{
	if (start_xfade_curve) {
		start_xfade_curve->show();
	}
	if (start_xfade_rect) {
		start_xfade_rect->show ();
	}

	_start_xfade_visible = true;
}

void
AudioRegionView::show_end_xfade ()
{
	if (end_xfade_curve) {
		end_xfade_curve->show();
	}
	if (end_xfade_rect) {
		end_xfade_rect->show ();
	}

	_end_xfade_visible = true;
}

void
AudioRegionView::set_samples_per_pixel (gdouble fpp)
{
	RegionView::set_samples_per_pixel (fpp);

	if (ARDOUR_UI::config()->get_show_waveforms ()) {
		for (uint32_t n = 0; n < waves.size(); ++n) {
			waves[n]->set_samples_per_pixel (fpp);
		}
	}

	if (gain_line) {
		gain_line->reset ();
	}

	reset_fade_shapes ();
}

void
AudioRegionView::set_amplitude_above_axis (gdouble a)
{
    if (fabs(_amplitude_above_axis - a) > 0.01) {
        
        _amplitude_above_axis = a;
        for (uint32_t n=0; n < waves.size(); ++n) {
            waves[n]->set_amplitude_above_axis (a);
        }
    }
}

void
AudioRegionView::set_colors ()
{
	RegionView::set_colors();

	if (gain_line) {
		gain_line->set_line_color (audio_region()->envelope_active() ? 
					   ARDOUR_UI::config()->get_canvasvar_GainLine() : 
					   ARDOUR_UI::config()->get_canvasvar_GainLineInactive());
	}

	set_waveform_colors ();

	if (start_xfade_curve) {
		start_xfade_curve->set_fill_color (ARDOUR_UI::config()->get_canvasvar_ActiveCrossfade());
		start_xfade_curve->set_outline_color (ARDOUR_UI::config()->get_canvasvar_CrossfadeLine());
	}
	if (end_xfade_curve) {
		end_xfade_curve->set_fill_color (ARDOUR_UI::config()->get_canvasvar_ActiveCrossfade());
		end_xfade_curve->set_outline_color (ARDOUR_UI::config()->get_canvasvar_CrossfadeLine());
	}

	if (start_xfade_rect) {
		start_xfade_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_CrossfadeLine());
	}
	if (end_xfade_rect) {
		end_xfade_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_CrossfadeLine());
	}
}

void
AudioRegionView::setup_waveform_visibility ()
{
	if (ARDOUR_UI::config()->get_show_waveforms ()) {
		for (uint32_t n = 0; n < waves.size(); ++n) {
			/* make sure the zoom level is correct, since we don't update
			   this when waveforms are hidden.
			*/
			// CAIROCANVAS
			// waves[n]->set_samples_per_pixel (_samples_per_pixel);
			waves[n]->show();
		}
	} else {
		for (uint32_t n = 0; n < waves.size(); ++n) {
			waves[n]->hide();
		}
	}
}

void
AudioRegionView::temporarily_hide_envelope ()
{
	if (gain_line) {
		gain_line->hide ();
	}
}

void
AudioRegionView::unhide_envelope ()
{
	update_envelope_visibility ();
}

void
AudioRegionView::update_envelope_visibility ()
{
	if (!gain_line) {
		return;
	}

	if (ARDOUR_UI::config()->get_show_region_gain() || trackview.editor().current_mouse_mode() == Editing::MouseGain) { // 
		gain_line->add_visibility (AutomationLine::Line);
	} else {
		gain_line->hide ();
	}
}

void
AudioRegionView::create_waves ()
{
	// cerr << "AudioRegionView::create_waves() called on " << this << endl;//DEBUG
	RouteTimeAxisView& atv (*(dynamic_cast<RouteTimeAxisView*>(&trackview))); // ick

	if (!atv.track()) {
		return;
	}

	ChanCount nchans = atv.track()->n_channels();

	// cerr << "creating waves for " << _region->name() << " with wfd = " << wait_for_data
	//		<< " and channels = " << nchans.n_audio() << endl;

	/* in tmp_waves, set up null pointers for each channel so the vector is allocated */
	for (uint32_t n = 0; n < nchans.n_audio(); ++n) {
		tmp_waves.push_back (0);
	}

	for (vector<ScopedConnection*>::iterator i = _data_ready_connections.begin(); i != _data_ready_connections.end(); ++i) {
		delete *i;
	}

	_data_ready_connections.clear ();

	for (uint32_t i = 0; i < nchans.n_audio(); ++i) {
		_data_ready_connections.push_back (0);
	}

	for (uint32_t n = 0; n < nchans.n_audio(); ++n) {

		if (n >= audio_region()->n_channels()) {
			break;
		}

		// cerr << "\tchannel " << n << endl;

		if (wait_for_data) {
			if (audio_region()->audio_source(n)->peaks_ready (boost::bind (&AudioRegionView::peaks_ready_handler, this, n), &_data_ready_connections[n], gui_context())) {
				// cerr << "\tData is ready\n";
				create_one_wave (n, true);
			} else {
				// cerr << "\tdata is not ready\n";
				// we'll get a PeaksReady signal from the source in the future
				// and will call create_one_wave(n) then.
			}

		} else {
			// cerr << "\tdon't delay, display today!\n";
			create_one_wave (n, true);
		}

	}
}

void
AudioRegionView::create_one_wave (uint32_t which, bool /*direct*/)
{
	//cerr << "AudioRegionView::create_one_wave() called which: " << which << " this: " << this << endl;//DEBUG
	RouteTimeAxisView& atv (*(dynamic_cast<RouteTimeAxisView*>(&trackview))); // ick
	uint32_t nchans = atv.track()->n_channels().n_audio();
	uint32_t n;
	uint32_t nwaves = std::min (nchans, audio_region()->n_channels());
	
	WaveView *wave = new WaveView (group, audio_region ());
	CANVAS_DEBUG_NAME (wave, string_compose ("wave view for chn %1 of %2", which, get_item_name()));

	wave->set_channel (which);
    setup_waveform_height_and_position (wave, which, nchans);
	wave->set_samples_per_pixel (samples_per_pixel);
	wave->set_show_zero_line (true);
	wave->set_clip_level (ARDOUR_UI::config()->get_waveform_clip_level ());

	wave->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_wave_view_event), wave, this));
	
	switch (ARDOUR_UI::config()->get_waveform_shape()) {
	case Rectified:
		wave->set_shape (WaveView::Rectified);
		break;
	default:
		wave->set_shape (WaveView::Normal);
	}
		
	wave->set_logscaled (ARDOUR_UI::config()->get_waveform_scale() == Logarithmic);

	vector<ArdourCanvas::WaveView*> v;
	v.push_back (wave);
	set_some_waveform_colors (v);

	if (!ARDOUR_UI::config()->get_show_waveforms ()) {
		wave->hide();
	}

	/* note: calling this function is serialized by the lock
	   held in the peak building thread that signals that
	   peaks are ready for use *or* by the fact that it is
	   called one by one from the GUI thread.
	*/

	if (which < nchans) {
		tmp_waves[which] = wave;
	} else {
		/* n-channel track, >n-channel source */
	}

	/* see if we're all ready */

	for (n = 0; n < nchans; ++n) {
		if (tmp_waves[n] == 0) {
			break;
		}
	}

	if (n == nwaves && waves.empty()) {
		/* all waves are ready */
		tmp_waves.resize(nwaves);

		waves = tmp_waves;
		tmp_waves.clear ();

		/* all waves created, don't hook into peaks ready anymore */
		delete _data_ready_connections[which];
		_data_ready_connections[which] = 0;
	}
}

void
AudioRegionView::peaks_ready_handler (uint32_t which)
{
	Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&AudioRegionView::create_one_wave, this, which, false));
	// cerr << "AudioRegionView::peaks_ready_handler() called on " << which << " this: " << this << endl;
}

void
AudioRegionView::add_gain_point_event (ArdourCanvas::Item *item, GdkEvent *ev, bool with_guard_points)
{
	if (!gain_line) {
		return;
	}

	double x, y;

	/* don't create points that can't be seen */

	update_envelope_visibility ();

	x = ev->button.x;
	y = ev->button.y;

	item->canvas_to_item (x, y);

	framepos_t fx = trackview.editor().pixel_to_sample (x);

	if (fx > _region->length()) {
		return;
	}

	/* compute vertical fractional position */

	y = 1.0 - (y / (_height));

	/* map using gain line */

	gain_line->view_to_model_coord (x, y);

	/* XXX STATEFUL: can't convert to stateful diff until we
	   can represent automation data with it.
	*/

	trackview.session()->begin_reversible_command (_("add gain control point"));
	XMLNode &before = audio_region()->envelope()->get_state();

	if (!audio_region()->envelope_active()) {
		XMLNode &region_before = audio_region()->get_state();
		audio_region()->set_envelope_active(true);
		XMLNode &region_after = audio_region()->get_state();
		trackview.session()->add_command (new MementoCommand<AudioRegion>(*(audio_region().get()), &region_before, &region_after));
	}

	audio_region()->envelope()->add (fx, y, with_guard_points);

	XMLNode &after = audio_region()->envelope()->get_state();
	trackview.session()->add_command (new MementoCommand<AutomationList>(*audio_region()->envelope().get(), &before, &after));
	trackview.session()->commit_reversible_command ();
}

void
AudioRegionView::remove_gain_point_event (ArdourCanvas::Item *item, GdkEvent* /*ev*/)
{
	ControlPoint *cp = reinterpret_cast<ControlPoint *> (item->get_data ("control_point"));
	audio_region()->envelope()->erase (cp->model());
}

GhostRegion*
AudioRegionView::add_ghost (TimeAxisView& tv)
{
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(&trackview);
	assert(rtv);

	double unit_position = _region->position () / samples_per_pixel;
	AudioGhostRegion* ghost = new AudioGhostRegion (tv, trackview, unit_position);
	uint32_t nchans;

	nchans = rtv->track()->n_channels().n_audio();

	for (uint32_t n = 0; n < nchans; ++n) {

		if (n >= audio_region()->n_channels()) {
			break;
		}

		WaveView *wave = new WaveView (ghost->group, audio_region());
		CANVAS_DEBUG_NAME (wave, string_compose ("ghost wave for %1", get_item_name()));

		wave->set_channel (n);
		wave->set_samples_per_pixel (samples_per_pixel);
		wave->set_amplitude_above_axis (_amplitude_above_axis);

		ghost->waves.push_back(wave);
	}

	ghost->set_height ();
	ghost->set_duration (_region->length() / samples_per_pixel);
	ghost->set_colors();
	ghosts.push_back (ghost);

	return ghost;
}

void
AudioRegionView::entered (bool internal_editing)
{
	trackview.editor().set_current_trimmable (_region);
	trackview.editor().set_current_movable (_region);
	
	if (gain_line) {
		/* these may or may not be visible depending on mouse mode */
		gain_line->add_visibility (AutomationLine::ControlPoints);
	}

	if (!internal_editing &&  ( trackview.editor().current_mouse_mode() == Editing::MouseObject ) ) {
		if (start_xfade_rect) {
			start_xfade_rect->set_outline (true);
		}
		if (end_xfade_rect) {
			end_xfade_rect->set_outline (true);
		}
		if (fade_in_handle) {
			fade_in_handle->show ();
			fade_in_handle->raise_to_top ();
		}
		if (fade_out_handle) {
			fade_out_handle->show ();
			fade_out_handle->raise_to_top ();
		}
    }
}

void
AudioRegionView::exited ()
{
	trackview.editor().set_current_trimmable (boost::shared_ptr<Trimmable>());
	trackview.editor().set_current_movable (boost::shared_ptr<Movable>());

	if (gain_line) {
		gain_line->remove_visibility (AutomationLine::ControlPoints);
	}

	if (fade_in_handle)       { fade_in_handle->hide(); }
	if (fade_out_handle)      { fade_out_handle->hide(); }
	if (start_xfade_rect)     { start_xfade_rect->set_outline (false); }
	if (end_xfade_rect)       { end_xfade_rect->set_outline (false); }
}

void
AudioRegionView::envelope_active_changed ()
{
	if (gain_line) {
		gain_line->set_line_color (audio_region()->envelope_active() ? 
					   ARDOUR_UI::config()->get_canvasvar_GainLine() : 
					   ARDOUR_UI::config()->get_canvasvar_GainLineInactive());
		update_envelope_visibility ();
	}
}

void
AudioRegionView::color_handler ()
{
	//case cMutedWaveForm:
	//case cWaveForm:
	//case cWaveFormClip:
	//case cZeroLine:
	set_colors ();

	//case cGainLineInactive:
	//case cGainLine:
	envelope_active_changed();

}

void
AudioRegionView::set_waveform_colors ()
{
	set_some_waveform_colors (waves);
}

void
AudioRegionView::set_some_waveform_colors (vector<ArdourCanvas::WaveView*>& waves_to_color)
{
	ArdourCanvas::Color fill;
	ArdourCanvas::Color outline;
	ArdourCanvas::Color clip = ARDOUR_UI::config()->get_canvasvar_WaveFormClip();
	ArdourCanvas::Color zero = ARDOUR_UI::config()->get_canvasvar_ZeroLine();

	if (_selected) {
		if (_region->muted()) {
			/* hide outline with zero alpha */
			outline =
            zero    =
			fill    = UINT_RGBA_CHANGE_A(ARDOUR_UI::config()->get_canvasvar_SelectedWaveForm(), min (255, 2*MUTED_ALPHA));
		} else {
			outline =
            zero    =
			fill    = ARDOUR_UI::config()->get_canvasvar_SelectedWaveForm();
		}
	} else {
		if (_recregion) {
			outline =
            zero =
			fill = ARDOUR_UI::config()->get_canvasvar_RecWaveFormFill();
		} else {
			if (_region->muted()) {
				/* hide outline with zero alpha */
				outline =
                zero =
				fill = UINT_RGBA_CHANGE_A(0, MUTED_ALPHA);
			} else {
				outline =
				zero =
				fill = get_fill_color ();
			}
		}
	}

    for (vector<ArdourCanvas::WaveView*>::iterator w = waves_to_color.begin(); w != waves_to_color.end(); ++w) {
		(*w)->set_fill_color (fill);
		(*w)->set_outline_color (outline);
		(*w)->set_clip_color (clip);
		(*w)->set_zero_color (zero);
	}
}

void
AudioRegionView::set_frame_color ()
{
	if (!frame) {
		return;
	}

	RegionView::set_frame_color ();
	set_waveform_colors ();
}

void
AudioRegionView::set_fade_visibility (bool yn)
{
	if (yn) {
		if (start_xfade_curve)    { start_xfade_curve->show (); }
		if (end_xfade_curve)      { end_xfade_curve->show (); }
		if (start_xfade_rect)     { start_xfade_rect->show (); }
		if (end_xfade_rect)       { end_xfade_rect->show (); }
		} else {
		if (start_xfade_curve)    { start_xfade_curve->hide(); }
		if (end_xfade_curve)      { end_xfade_curve->hide(); }
		if (fade_in_handle)       { fade_in_handle->hide(); }
		if (fade_out_handle)      { fade_out_handle->hide(); }
		if (start_xfade_rect)     { start_xfade_rect->hide (); }
		if (end_xfade_rect)       { end_xfade_rect->hide (); }
		if (start_xfade_rect)     { start_xfade_rect->set_outline (false); }
		if (end_xfade_rect)       { end_xfade_rect->set_outline (false); }
	}
}

void
AudioRegionView::update_coverage_frames (LayerDisplay d)
{
	RegionView::update_coverage_frames (d);

	if (fade_in_handle)       { fade_in_handle->raise_to_top (); }
	if (fade_out_handle)      { fade_out_handle->raise_to_top (); }
}

void
AudioRegionView::transients_changed ()
{
	AnalysisFeatureList analysis_features = _region->transients();

	while (feature_lines.size() < analysis_features.size()) {

		ArdourCanvas::Line* canvas_item = new ArdourCanvas::Line(group);
		CANVAS_DEBUG_NAME (canvas_item, string_compose ("transient group for %1", region()->name()));

		canvas_item->set (ArdourCanvas::Duple (-1.0, 2.0),
				  ArdourCanvas::Duple (1.0, _height - 1));

		canvas_item->raise_to_top ();
		canvas_item->show ();

		canvas_item->set_data ("regionview", this);
		canvas_item->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_feature_line_event), canvas_item, this));

		feature_lines.push_back (make_pair(0, canvas_item));
	}

	while (feature_lines.size() > analysis_features.size()) {
		ArdourCanvas::Line* line = feature_lines.back().second;
		feature_lines.pop_back ();
		delete line;
	}

	AnalysisFeatureList::const_iterator i;
	list<std::pair<framepos_t, ArdourCanvas::Line*> >::iterator l;

	for (i = analysis_features.begin(), l = feature_lines.begin(); i != analysis_features.end() && l != feature_lines.end(); ++i, ++l) {

		float *pos = new float;
		*pos = trackview.editor().sample_to_pixel (*i);

		(*l).second->set (
			ArdourCanvas::Duple (*pos, 2.0),
			ArdourCanvas::Duple (*pos, _height - 1)
			);

		(*l).second->set_data ("position", pos);
		(*l).first = *i;
	}
}

void
AudioRegionView::update_transient(float /*old_pos*/, float new_pos)
{
	/* Find frame at old pos, calulate new frame then update region transients*/
	list<std::pair<framepos_t, ArdourCanvas::Line*> >::iterator l;

	for (l = feature_lines.begin(); l != feature_lines.end(); ++l) {

		/* Line has been updated in drag so we compare to new_pos */

		float* pos = (float*) (*l).second->get_data ("position");

		if (rint(new_pos) == rint(*pos)) {

		    framepos_t old_frame = (*l).first;
		    framepos_t new_frame = trackview.editor().pixel_to_sample (new_pos);

		    _region->update_transient (old_frame, new_frame);

		    break;
		}
	}
}

void
AudioRegionView::remove_transient(float pos)
{
	/* Find frame at old pos, calulate new frame then update region transients*/
	list<std::pair<framepos_t, ArdourCanvas::Line*> >::iterator l;

	for (l = feature_lines.begin(); l != feature_lines.end(); ++l) {

		/* Line has been updated in drag so we compare to new_pos */
		float *line_pos = (float*) (*l).second->get_data ("position");

		if (rint(pos) == rint(*line_pos)) {
		    _region->remove_transient ((*l).first);
		    break;
		}
	}
}

void
AudioRegionView::thaw_after_trim ()
{
	RegionView::thaw_after_trim ();
	unhide_envelope ();
	drag_end ();
}


void
AudioRegionView::show_xfades ()
{
	show_start_xfade ();
	show_end_xfade ();
}

void
AudioRegionView::drag_start ()
{
	TimeAxisViewItem::drag_start ();

	//we used to hide xfades here.  I don't see the point with the new model, but we can re-implement if needed
}

void
AudioRegionView::drag_end ()
{
	TimeAxisViewItem::drag_end ();
	//see comment for drag_start

	if (fade_in_handle && fade_in_handle->visible()) {
		// lenght of region or fade changed, re-check
		// if fade_in_trim_handle or fade_out_trim_handle should
		// be visible. -- If the fade_in_handle is visible
		// we have focus and are not in internal edit mode.
		entered(false);
	}
}

void
AudioRegionView::parameter_changed (string const & p)
{
	if (p == "show-waveforms") {
		setup_waveform_visibility ();
	}
}
