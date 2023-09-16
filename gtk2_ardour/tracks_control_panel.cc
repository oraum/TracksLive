/*
    Copyright (C) 2014 Waves Audio Ltd.

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
#include "tracks_control_panel.h"
#include "waves_button.h"
#include "i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Glib;

TracksControlPanel::TracksControlPanel ()
	: WavesDialog ("tracks_preferences.xml")
	, _device_capture_list (get_v_box("device_capture_list"))
	, _device_playback_list (get_v_box("device_playback_list"))
	, _midi_device_list (get_v_box("midi_device_list"))
    , _enable_ltc_generator_vbox (get_v_box("enable_ltc_generator_vbox"))
    , _ltc_output_port_vbox (get_v_box("ltc_output_port_vbox"))
    , _ltc_generator_level_vbox (get_v_box("ltc_generator_level_vbox"))
    , _freewheel_vbox (get_v_box ("freewheel_vbox"))
    , _ltc_send_continuously_hbox (get_h_box("ltc_send_continuously_hbox"))
	, _all_inputs_on_button (get_waves_button("all_inputs_on_button"))
    , _all_inputs_off_button (get_waves_button("all_inputs_off_button"))
    , _all_outputs_on_button (get_waves_button("all_outputs_on_button"))
    , _all_outputs_off_button (get_waves_button("all_outputs_off_button"))
	, _audio_settings_tab (get_container ("audio_settings_tab"))
	, _midi_settings_tab (get_container ("midi_settings_tab"))
	, _session_settings_tab (get_container ("session_settings_tab"))
	, _general_settings_tab (get_container ("general_settings_tab"))
    , _sync_settings_tab (get_container ("sync_settings_tab"))
	, _audio_settings_tab_button (get_waves_button ("audio_settings_tab_button"))
	, _midi_settings_tab_button (get_waves_button ("midi_settings_tab_button"))
	, _session_settings_tab_button (get_waves_button ("session_settings_tab_button"))
	, _general_settings_tab_button (get_waves_button ("general_settings_tab_button"))
    , _sync_settings_tab_button (get_waves_button ("sync_settings_tab_button"))
	, _ok_button (get_waves_button ("ok_button"))
	, _cancel_button (get_waves_button ("cancel_button"))
	, _control_panel_button (get_waves_button ("control_panel_button"))
	, _no_button (get_waves_button ("no_button"))
	, _yes_button (get_waves_button ("yes_button"))
    , _transport_record_locked_button (get_waves_button("transport_record_locked_button"))
    , _enable_ltc_generator_button (get_waves_button ("enable_ltc_generator_button"))
    , _ltc_send_continuously_button (get_waves_button ("ltc_send_continuously_button"))
	, _engine_dropdown (get_waves_dropdown ("engine_dropdown"))
	, _device_dropdown (get_waves_dropdown ("device_dropdown"))
	, _sample_rate_dropdown (get_waves_dropdown ("sample_rate_dropdown"))
	, _buffer_size_dropdown (get_waves_dropdown ("buffer_size_dropdown"))
    , _latency_label (get_label("latency_label"))
    , _default_open_path (get_label("default_open_path"))
    , _freewheel_timeout_label (get_label ("freewheel_timeout_label"))
    , _ltc_generator_level_label (get_label("ltc_generator_level_label"))
	, _multi_out_button(get_waves_button ("multi_out_button"))
	, _stereo_out_button(get_waves_button ("stereo_out_button"))
    , _name_tracks_after_driver(get_waves_button ("name_tracks_after_driver_button"))
    , _reset_tracks_name_to_default(get_waves_button ("reset_tracks_name_to_default_button"))
    , _freewheel_timeout_adjustment (get_adjustment ("freewheel_timeout_adjustment"))
    , _ltc_generator_level_adjustment (get_adjustment ("ltc_generator_level_adjustment"))
	, _obey_mmc_commands_button (get_waves_button ("obey_mmc_commands_button"))
	, _send_mmc_commands_button (get_waves_button ("send_mmc_commands_button"))
	, _dc_bias_against_denormals_button (get_waves_button ("dc_bias_against_denormals_button"))
	, _copy_imported_files_button (get_waves_button ("copy_imported_files_button"))
	, _color_track_panel_button (get_waves_button ("color_track_panel_button"))
	, _inbound_mmc_device_spinbutton (get_spin_button ("inbound_mmc_device_spinbutton"))
	, _outbound_mmc_device_spinbutton (get_spin_button ("outbound_mmc_device_spinbutton"))
	, _limit_undo_history_spinbutton (get_spin_button ("limit_undo_history_spinbutton"))
	, _save_undo_history_spinbutton (get_spin_button ("save_undo_history_spinbutton"))
    , _file_type_dropdown (get_waves_dropdown ("file_type_dropdown"))
    , _bit_depth_dropdown (get_waves_dropdown ("bit_depth_dropdown"))
    , _frame_rate_dropdown (get_waves_dropdown ("frame_rate_dropdown"))
    , _browse_button (get_waves_button("browse_default_folder"))
    , _auto_lock_timer_dropdown (get_waves_dropdown("auto_lock_timer_dropdown"))
    , _auto_save_timer_dropdown (get_waves_dropdown("auto_save_timer_dropdown"))
    , _pre_record_buffer_dropdown (get_waves_dropdown("pre_record_buffer_dropdown"))
	, _waveform_shape_dropdown (get_waves_dropdown ("waveform_shape_dropdown"))
	, _peak_hold_time_dropdown (get_waves_dropdown ("peak_hold_time_dropdown"))
	, _dpm_fall_off_dropdown (get_waves_dropdown ("dpm_fall_off_dropdown"))
	, _hard_disk_buffering_dropdown (get_waves_dropdown ("hard_disk_buffering_dropdown"))
    , _sync_tool_dropdown (get_waves_dropdown ("sync_tool_dropdown"))
    , _mtc_in_dropdown (get_waves_dropdown ("mtc_in"))
    , _ltc_in_dropdown (get_waves_dropdown ("ltc_in"))
    , _ltc_out_dropdown (get_waves_dropdown ("ltc_out"))
    , _sync_input_port_layout (get_layout ("sync_input_port_layout"))
    , _have_control (false)
	, _ignore_changes (0)
{
	init();
}

TracksControlPanel::~TracksControlPanel ()
{
	_ignore_changes = true;
}
