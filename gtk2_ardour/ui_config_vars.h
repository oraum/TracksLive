/*
    Copyright (C) 2012 Paul Davis 

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

UI_CONFIG_VARIABLE(std::string, icon_set, "icon-set", "default")
UI_CONFIG_VARIABLE(std::string, ui_rc_file, "ui-rc-file", "ui_dark.rc")
UI_CONFIG_VARIABLE(bool, flat_buttons, "flat-buttons", false)
UI_CONFIG_VARIABLE(float, waveform_gradient_depth, "waveform-gradient-depth", 0)
UI_CONFIG_VARIABLE(float, timeline_item_gradient_depth, "timeline-item-gradient-depth", 0.5)
UI_CONFIG_VARIABLE(bool, all_floating_windows_are_dialogs, "all-floating-windows-are-dialogs", false)
UI_CONFIG_VARIABLE (bool, color_regions_using_track_color, "color-regions-using-track-color", true)
UI_CONFIG_VARIABLE (bool, show_waveform_clipping, "show-waveform-clipping", true)
UI_CONFIG_VARIABLE (int, auto_lock_timer, "auto-lock-timer", 0)
UI_CONFIG_VARIABLE (int, auto_save_timer, "auto-save-timer", 0)
UI_CONFIG_VARIABLE (int, pre_record_buffer, "pre-record-buffer", 0)
UI_CONFIG_VARIABLE (bool, draggable_playhead, "draggable-playhead", true)
UI_CONFIG_VARIABLE (bool, super_rapid_clock_update, "super-rapid-clock-update", true)
UI_CONFIG_VARIABLE (bool, show_plugin_scan_window, "show-plugin-scan-window", false)
UI_CONFIG_VARIABLE (bool, keep_tearoffs, "keep-tearoffs", false)
UI_CONFIG_VARIABLE (bool, show_track_meters, "show-track-meters", true)
UI_CONFIG_VARIABLE (float, gain_min_threshold, "gain-min-threshold", -139.9f)

UI_CONFIG_VARIABLE (bool, link_region_and_track_selection, "link-region-and-track-selection", false)  // DEPRECATED
UI_CONFIG_VARIABLE (bool, link_editor_and_mixer_selection, "link-editor-and-mixer-selection", true)
UI_CONFIG_VARIABLE (std::string, keyboard_layout_name, "keyboard-layout-name", "ansi")
UI_CONFIG_VARIABLE (bool, autoscroll_editor, "autoscroll-editor", true)
UI_CONFIG_VARIABLE (bool, primary_clock_delta_edit_cursor, "primary-clock-delta-edit-cursor", false)
UI_CONFIG_VARIABLE (bool, secondary_clock_delta_edit_cursor, "secondary-clock-delta-edit-cursor", false)
UI_CONFIG_VARIABLE (bool, follow_edits, "follow-edits", false)
UI_CONFIG_VARIABLE (float, meter_hold, "meter-hold", 100.0f)
UI_CONFIG_VARIABLE (ARDOUR::VUMeterStandard, meter_vu_standard, "meter-vu-standard", MeteringVUstandard)
UI_CONFIG_VARIABLE (ARDOUR::MeterLineUp, meter_line_up_level, "meter-line-up-level", MeteringLineUp18)
UI_CONFIG_VARIABLE (ARDOUR::MeterLineUp, meter_line_up_din, "meter-line-up-din", MeteringLineUp15)
UI_CONFIG_VARIABLE (float, meter_peak, "meter-peak", 0.0f)
UI_CONFIG_VARIABLE (float, meter_peak_2, "meter_peak_2", -5.9f)
UI_CONFIG_VARIABLE (float, numeric_peak_min_treshold, "numeric-peak-min-treshold", -144.4f)
UI_CONFIG_VARIABLE (bool, meter_style_led, "meter-style-led", true)
UI_CONFIG_VARIABLE (bool, show_editor_meter, "show-editor-meter", true)
UI_CONFIG_VARIABLE (double, waveform_clip_level, "waveform-clip-level", -0.0) /* units of dB */
UI_CONFIG_VARIABLE (bool, no_new_session_dialog, "no-new-session-dialog", false)
UI_CONFIG_VARIABLE (bool, only_copy_imported_files, "only-copy-imported-files", true)
UI_CONFIG_VARIABLE (bool, color_track_panel, "color-track-panel", false)
UI_CONFIG_VARIABLE (std::string, keyboard_layout, "keyboard-layout", "ansi")
UI_CONFIG_VARIABLE (std::string, default_bindings, "default-bindings", "ardour")
UI_CONFIG_VARIABLE (bool, default_narrow_ms, "default-narrow_ms", false)
UI_CONFIG_VARIABLE (bool, name_new_markers, "name-new-markers", false)
UI_CONFIG_VARIABLE (bool, rubberbanding_snaps_to_grid, "rubberbanding-snaps-to-grid", true)
UI_CONFIG_VARIABLE (long, font_scale, "font-scale", 81920)
UI_CONFIG_VARIABLE (bool, show_waveforms, "show-waveforms", true)
UI_CONFIG_VARIABLE (bool, show_waveforms_while_recording, "show-waveforms-while-recording", true)
UI_CONFIG_VARIABLE (ARDOUR::WaveformScale, waveform_scale, "waveform-scale", Linear)
UI_CONFIG_VARIABLE (ARDOUR::WaveformShape, waveform_shape, "waveform-shape", Traditional)
UI_CONFIG_VARIABLE (bool, update_editor_during_summary_drag, "update-editor-during-summary-drag", true)
UI_CONFIG_VARIABLE (bool, never_display_periodic_midi, "never-display-periodic-midi", true)
UI_CONFIG_VARIABLE (bool, sound_midi_notes, "sound-midi-notes", false)
UI_CONFIG_VARIABLE (bool, show_zoom_tools, "show-zoom-tools", true)
UI_CONFIG_VARIABLE (bool, widget_prelight, "widget-prelight", true)
UI_CONFIG_VARIABLE (bool, use_tooltips, "use-tooltips", true)
UI_CONFIG_VARIABLE (std::string, mixer_strip_visibility, "mixer-strip-visibility", "PhaseInvert,SoloSafe,SoloIsolated,Group,MeterPoint")
UI_CONFIG_VARIABLE (bool, allow_non_quarter_pulse, "allow-non-quarter-pulse", false)
UI_CONFIG_VARIABLE (bool, show_region_gain, "show-region-gain", false)
UI_CONFIG_VARIABLE (bool, show_name_highlight, "show-name-highlight", true)
