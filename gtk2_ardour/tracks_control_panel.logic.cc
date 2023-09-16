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
#include <stdlib.h>
#include <string>
#include <stdio.h>

#include "tracks_control_panel.h"
#include "waves_button.h"

#include "pbd/replace_all.h"
#include "pbd/unwind.h"

#include <gtkmm2ext/utils.h>

#include "ardour/types.h"
#include "ardour/engine_state_controller.h"
#include "ardour/rc_configuration.h"
#include "ardour/recent_sessions.h"
#include "ardour/filename_extensions.h"

#include "ardour/utils.h"
#include "ardour_ui.h"
#include "gui_thread.h"
#include "utils.h"
#include "i18n.h"
#include "pbd/convert.h"

#include "timecode/time.h"
#include "time.h"

#include "open_file_dialog.h"
#include "waves_message_dialog.h"
#include "dbg_msg.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Glib;
using namespace Timecode;

namespace {

    static const char* audio_capture_name_prefix = "system:capture:";
    static const char* audio_playback_name_prefix = "system:playback:";
    static const char* midi_port_name_prefix = "system_midi:";
    static const char* midi_capture_suffix = " capture";
    static const char* midi_playback_suffix = " playback";
    
    struct MidiDeviceDescriptor {
        std::string name;
        std::string capture_name;
        bool capture_active;
        std::string playback_name;
        bool playback_active;
        
        MidiDeviceDescriptor(const std::string& name) :
        name(name),
        capture_name(""),
        capture_active(false),
        playback_name(""),
        playback_active(false)
        {}
        
        bool operator==(const MidiDeviceDescriptor& rhs) {
            return name == rhs.name;
        }
    };
    
    typedef std::vector<MidiDeviceDescriptor> MidiDeviceDescriptorVec;
    
    void dropdown_element_data_cleaner (void* data)
    {
        free (data);
    }
    
    ARDOUR::SyncSource
    SyncSourceTracks_to_SyncSource (int el_number)
    {
        switch (el_number) {
            case TracksControlPanel::MTC:
                return ARDOUR::MTC;
            case TracksControlPanel::LTC:
                return ARDOUR::LTC;
            default:
                fatal << "Wrong argument in converting from SyncSourceTracks to ARDOUR::SyncSource" << endmsg;
                // just to avoid warning
                return ARDOUR::MTC;
        }
    }
    
    TracksControlPanel::SyncSourceTracks
    SyncSource_to_SyncSourceTracks (ARDOUR::SyncSource sync_source)
    {
        switch (sync_source) {
            case ARDOUR::MTC:
                return TracksControlPanel::MTC;
            case ARDOUR::LTC:
                return TracksControlPanel::LTC;
            default:
                fatal << "Wrong argument in converting from ARDOUR::SyncSource to TracksControlPanel::SyncSourceTracks" << endmsg;
                // just to avoid warning
                return TracksControlPanel::MTC;
        }
    }
}

void
TracksControlPanel::init ()
{
	_ok_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_ok));
	_cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_cancel));

	_audio_settings_tab_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_a_settings_tab_button_clicked));
	_midi_settings_tab_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_a_settings_tab_button_clicked));
	_session_settings_tab_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_a_settings_tab_button_clicked));
	_general_settings_tab_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_a_settings_tab_button_clicked));
    _sync_settings_tab_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_a_settings_tab_button_clicked));
    
    _all_inputs_on_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_all_inputs_on_button));
    _all_inputs_off_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_all_inputs_off_button));
    _all_outputs_on_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_all_outputs_on_button));
    _all_outputs_off_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_all_outputs_off_button));
    
    _multi_out_button.signal_clicked.connect(sigc::mem_fun (*this, &TracksControlPanel::on_multi_out));
    _stereo_out_button.signal_clicked.connect(sigc::mem_fun (*this, &TracksControlPanel::on_stereo_out));
    
    _browse_button.signal_clicked.connect(sigc::mem_fun (*this, &TracksControlPanel::on_browse_button));    
    
    _enable_ltc_generator_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_enable_ltc_generator_button));
    _ltc_send_continuously_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_ltc_send_continuously_button));
    
	EngineStateController::instance ()->EngineRunning.connect (running_connection, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::engine_running, this), gui_context());
	EngineStateController::instance ()->EngineStopped.connect (stopped_connection, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::engine_stopped, this), gui_context());
	EngineStateController::instance ()->EngineHalted.connect (stopped_connection, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::engine_stopped, this), gui_context());

	/* Subscribe for udpates from EngineStateController */
    EngineStateController::instance()->PortRegistrationChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_port_registration_update, this), gui_context());
	EngineStateController::instance()->BufferSizeChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_buffer_size_update, this), gui_context());
    EngineStateController::instance()->DeviceListChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_device_list_update, this, _1), gui_context());
    EngineStateController::instance()->InputConfigChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_audio_input_configuration_changed, this), gui_context());
    EngineStateController::instance()->OutputConfigChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_audio_output_configuration_changed, this), gui_context());
    EngineStateController::instance()->MIDIInputConfigChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_midi_input_configuration_changed, this), gui_context());
    EngineStateController::instance()->MIDIOutputConfigChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_midi_output_configuration_changed, this), gui_context());
    EngineStateController::instance()->MTCInputChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_mtc_input_changed, this, _1), gui_context());
    EngineStateController::instance()->DeviceError.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_device_error, this, _1), gui_context ());
	EngineStateController::instance()->CheckSampleRateMismatch.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::ask_about_sample_rate_mismatch, this), gui_context());

    /* Global configuration parameters update */
    Config->ParameterChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_parameter_changed, this, _1), gui_context());
    
    ARDOUR_UI::config()->ParameterChanged.connect_same_thread (update_connections, boost::bind (&TracksControlPanel::on_ui_parameter_changed, this, _1) );
    
    _engine_dropdown.selected_item_changed.connect (sigc::mem_fun(*this, &TracksControlPanel::on_engine_dropdown_item_clicked));
    _device_dropdown.selected_item_changed.connect (sigc::mem_fun(*this, &TracksControlPanel::on_device_dropdown_item_clicked));
	_sample_rate_dropdown.selected_item_changed.connect (sigc::mem_fun(*this, &TracksControlPanel::on_sample_rate_dropdown_item_clicked));
	_buffer_size_dropdown.selected_item_changed.connect (sigc::mem_fun(*this, &TracksControlPanel::on_buffer_size_dropdown_item_clicked));
    _mtc_in_dropdown.selected_item_changed.connect (sigc::mem_fun(*this, &TracksControlPanel::on_mtc_in_dropdown_changed));
    _ltc_in_dropdown.selected_item_changed.connect (sigc::mem_fun(*this, &TracksControlPanel::on_ltc_in_dropdown_changed));
    _sync_tool_dropdown.selected_item_changed.connect (sigc::mem_fun(*this, &TracksControlPanel::on_sync_tool_dropdown_changed));
     _ltc_out_dropdown.selected_item_changed.connect (sigc::mem_fun(*this, &TracksControlPanel::on_ltc_out_dropdown_changed));

    
    /* Session configuration parameters update */
	_file_type_dropdown.selected_item_changed.connect (sigc::mem_fun(*this, &TracksControlPanel::on_file_type_dropdown_item_clicked));
    _bit_depth_dropdown.selected_item_changed.connect (sigc::mem_fun(*this, &TracksControlPanel::on_bit_depth_dropdown_item_clicked));
    _frame_rate_dropdown.selected_item_changed.connect (sigc::mem_fun (*this, &TracksControlPanel::on_frame_rate_item_clicked));

    _name_tracks_after_driver.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_name_tracks_after_driver));
    _reset_tracks_name_to_default.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_reset_tracks_name_to_default));

    _control_panel_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_control_panel_button));
    _freewheel_timeout_adjustment.signal_value_changed ().connect (mem_fun (*this, &TracksControlPanel::freewheel_timeout_adjustment_changed));
    _ltc_generator_level_adjustment.signal_value_changed ().connect (mem_fun (*this, &TracksControlPanel::ltc_generator_level_adjustment_changed));

    _yes_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_yes_button));
    _no_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_no_button));
    _yes_button.set_visible (false);
    _no_button.set_visible (false);
    
	populate_engine_dropdown ();
	populate_device_dropdown ();

    populate_mtc_in_dropdown ();
    
	populate_output_mode ();

    populate_file_type_dropdown ();
    populate_input_channels ();
    populate_output_channels ();
    populate_midi_ports ();
    populate_default_session_path ();
    
    // Init session Settings
    populate_bit_depth_dropdown ();
    populate_frame_rate_dropdown ();
    populate_auto_lock_timer_dropdown ();
    populate_auto_save_timer_dropdown ();
    populate_pre_record_buffer_dropdown ();
    
    show_buffer_duration ();
	_audio_settings_tab_button.set_active (true);
    
    Config->set_max_freewheel_timeout_ms (_freewheel_timeout_adjustment.get_upper ());

	display_general_preferences ();
}

DeviceConnectionControl& TracksControlPanel::add_device_capture_control (const std::string& port_name, bool active, uint16_t capture_number, const std::string& track_name)
{
    std::string device_capture_name(port_name);
    replace_all (device_capture_name, audio_capture_name_prefix, "");
    
	DeviceConnectionControl &capture_control = *manage (new DeviceConnectionControl(device_capture_name, active, capture_number, track_name));
    
    char * id_str = new char [port_name.length()+1];
    std::strcpy (id_str, port_name.c_str());
    capture_control.set_data(DeviceConnectionControl::id_name, id_str);
	
    _device_capture_list.pack_start (capture_control, false, false);
	capture_control.signal_active_changed.connect (sigc::mem_fun (*this, &TracksControlPanel::on_capture_active_changed));
	return capture_control;
}

DeviceConnectionControl& TracksControlPanel::add_device_playback_control(const std::string& port_name, bool active, uint16_t playback_number)
{
    std::string device_playback_name(port_name);
    replace_all (device_playback_name, audio_playback_name_prefix, "");
    
	DeviceConnectionControl &playback_control = *manage (new DeviceConnectionControl(device_playback_name, active, playback_number));
    
    char * id_str = new char [port_name.length()+1];
    std::strcpy (id_str, port_name.c_str());
    playback_control.set_data(DeviceConnectionControl::id_name, id_str);
    
	_device_playback_list.pack_start (playback_control, false, false);
	playback_control.signal_active_changed.connect(sigc::mem_fun (*this, &TracksControlPanel::on_playback_active_changed));
	return playback_control;
}

MidiDeviceConnectionControl& TracksControlPanel::add_midi_device_control (const std::string& midi_device_name,
                                                                         const std::string& capture_name, bool capture_active,
                                                                         const std::string& playback_name, bool playback_active)
{
	MidiDeviceConnectionControl &midi_device_control = *manage (new MidiDeviceConnectionControl(midi_device_name, !capture_name.empty(), capture_active, !playback_name.empty(), playback_active));
    
    if (!capture_name.empty()) {
        char * capture_id_str = new char [capture_name.length()+1];
        std::strcpy (capture_id_str, capture_name.c_str());
        midi_device_control.set_data(MidiDeviceConnectionControl::capture_id_name, capture_id_str);
    }
    
    if (!playback_name.empty()) {
        char * playback_id_str = new char [playback_name.length()+1];
        std::strcpy (playback_id_str, playback_name.c_str());
        midi_device_control.set_data(MidiDeviceConnectionControl::playback_id_name, playback_id_str);
    }
    
	_midi_device_list.pack_start (midi_device_control, false, false);
	midi_device_control.signal_capture_active_changed.connect (sigc::mem_fun (*this, &TracksControlPanel::on_midi_capture_active_changed));
    midi_device_control.signal_playback_active_changed.connect(sigc::mem_fun (*this, &TracksControlPanel::on_midi_playback_active_changed));
	return midi_device_control;
}

namespace  {
    // Strings which are shown to user in the Preference panel
    const std::string string_CAF = "Caf";
    const std::string string_BWav = "Wave";
    const std::string string_Aiff = "Aiff";
    const std::string string_Wav64 = "Wave64";
    const std::string string_RF64_WAV = "RF64/WAV";
    
    ARDOUR::HeaderFormat
    string_to_HeaderFormat(std::string s)
    {
        if(s == string_CAF)
            return CAF;
        
        if(s == string_BWav)
            return BWF;
        
        if(s == string_Aiff)
            return AIFF;
        
        if(s == string_Wav64)
            return WAVE64;
        
        if(s == string_RF64_WAV)
            return RF64_WAV;

		ARDOUR::HeaderFormat hf = RF64_WAV;
		if (sscanf (s.c_str (), "%d", &hf) != 1) {
			// defaul value
			hf = RF64_WAV;
		}


		switch (hf) {
		case CAF:
		case BWF:
		case AIFF:
		case WAVE64:
		case RF64_WAV:
			break;
		default:
			// defaul value
	        hf = RF64_WAV;
		}

        return hf;
    }
    
    enum FileTypeDropdownData {
        _caf = 0,
        _wave = 1,
        _aiff = 2,
        _wave64 = 3,
		_rf64_wav = 4,
        _bwf = 5
    };
    
    FileTypeDropdownData
    HeaderFormat_to_FileTypeDropdownData (ARDOUR::HeaderFormat hf) {
        switch (hf) {
            case CAF:
                return _caf;
            case AIFF:
                return _aiff;
            case WAVE64:
                return _wave64;
            case WAVE:
                return _wave;
            case BWF:
                return _bwf;
			case RF64_WAV:
            default:
                return _rf64_wav;
        }
    }
    
    ARDOUR::HeaderFormat
    FileTypeDropdownData_to_HeaderFormat (int data) {
        switch (data) {
            case (_caf):
                return CAF;
            case (_aiff):
                return AIFF;
            case (_wave64):
                return WAVE64;
            case (_wave):
                return WAVE;
            case (_bwf):
                return BWF;
            case (_rf64_wav):
            default:
				return RF64_WAV;
        }
    }

    std::string
    xml_string_to_user_string(std::string xml_string);
    
    enum SessionProperty {
        Native_File_Header_Format,
        Native_File_Data_Format,
        Timecode_Format
    };
    
    std::string
    read_property_from_last_session (SessionProperty session_property)
    {        
        ARDOUR::RecentSessions rs;
        ARDOUR::read_recent_sessions (rs);
        
        if( rs.size() > 0 )
        {
            std::string full_session_name = Glib::build_filename ( rs[0].second, rs[0].first );
            full_session_name += statefile_suffix;
            
            // read property from session projectfile
            boost::shared_ptr<XMLTree> state_tree (new XMLTree());
            
            if (!state_tree->read (full_session_name))
                return std::string ("");
            
            XMLNode& root (*state_tree->root());

            if (root.name () != X_("Session"))
                return std::string("");
            
            XMLNode* config_main_node = root.child ("Config");
            if( !config_main_node )
                return std::string ("");

            XMLNodeList config_nodes_list = config_main_node->children ();
            XMLNodeConstIterator config_node_iter = config_nodes_list.begin ();
            
            std::string required_property_name;
            
            switch (session_property) {
                case Native_File_Header_Format:
                    required_property_name = "native-file-header-format";
                    break;
                case Native_File_Data_Format:
                    required_property_name = "native-file-data-format";
                    break;
                case Timecode_Format:
                    required_property_name = "timecode-format";
                    break;
                default:
                    return std::string("");
            }
            
            for (; config_node_iter != config_nodes_list.end(); ++config_node_iter)
            {
                XMLNode* config_node = *config_node_iter;
                XMLProperty* node_name = config_node->property ("name");
                
				// Very HOT fix of very strange design:
                if ((node_name == 0)  || (node_name->value() != required_property_name )) {
					continue;
				}

				XMLProperty* node_value = config_node->property ("value");
				if (node_value == 0) {
					continue;
				}

				std::string value = xml_string_to_user_string (node_value->value());
				if (value.empty ()) {
					value = node_value->value();
				}

				return value;
            }
        } 
        
        return std::string("");
    }
}

namespace {
    // Strings which are shown to user in the Preference panel
    const std::string string_bit32 = "32 bit floating point";
    const std::string string_bit24 = "24 bit";
    const std::string string_bit16 = "16 bit";
    
    ARDOUR::SampleFormat
    string_to_SampleFormat(std::string s)
    {
        if(s == string_bit32)
            return FormatFloat;
        
        if(s == string_bit24)
            return FormatInt24;
        
        if(s == string_bit16)
            return FormatInt16;
        
   		ARDOUR::SampleFormat sf = FormatInt24;
		if (sscanf (s.c_str (), "%d", (int*)&sf) != 1) {
			// default value
			sf = FormatInt24;
		}

		switch (sf) {
		case FormatFloat:
		case FormatInt24:
		case FormatInt16:
			break;
		default:
			// default value
			sf = FormatInt24;
		}

        return sf;
    }
    
    enum BitDepthDropdownData {
        _16bit,
        _24bit,
        _32fbit
    };
    
    ARDOUR::SampleFormat
    BitDepthDropdownData_to_SampleFormat (int data)
    {
        switch (data) {
            case (_16bit):
                return FormatInt16;
            case (_32fbit):
                return FormatFloat;
            case (_24bit):
            default:
                return FormatInt24;
        }
    }
    
    BitDepthDropdownData
    SampleFormat_to_BitDepthDropdownData (ARDOUR::SampleFormat sample_format)
    {
        switch (sample_format) {
            case (FormatInt16):
                return _16bit;
            case (FormatFloat):
                return _32fbit;
            case (FormatInt24):
            default:
                return _24bit;
        }
    }
}

void
TracksControlPanel::populate_bit_depth_dropdown()
{
    // Get BIT_DEPTH from last used session
    std::string sample_format_string = read_property_from_last_session(Native_File_Data_Format);
    
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    ARDOUR::SampleFormat sample_format = string_to_SampleFormat(sample_format_string);
    ardour_ui->set_sample_format( sample_format );
    
    {
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
        _bit_depth_dropdown.set_current_item_by_data_i ( SampleFormat_to_BitDepthDropdownData (sample_format) );
	}
   
    return;
}

namespace  {
    const std::string string_24fps = "24 fps";
    const std::string string_25fps = "25 fps";
    const std::string string_30fps = "30 fps";
    const std::string string_23976fps = "23.976 fps";
    const std::string string_2997fps = "29.97 fps";
    const std::string string_30drop_fps = "30d fps";
    const std::string string_2997drop_fps = "29.97d fps";
    
    Timecode::TimecodeFormat
    string_to_TimecodeFormat(std::string s)
    {
        if(s == string_24fps)
            return timecode_24;
        if(s == string_25fps)
            return timecode_25;
        if(s == string_30fps)
            return timecode_30;
        if(s == string_23976fps)
            return timecode_23976;
        if(s == string_2997fps)
            return timecode_2997;
        if(s == string_2997drop_fps)
            return timecode_2997drop;
        if(s == string_30drop_fps)
            return timecode_30drop;
        
        //defaul value
        return timecode_25;
    }
    
    enum FrameRateDropdownItem {
        _24fps,
        _25fps,
        _30fps,
        _30d_fps,
        _23976fps,
        _2997fps,
        _2997d_fps
    };
    
    FrameRateDropdownItem
    TimecodeFormat_to_FrameRateDropdownItem (Timecode::TimecodeFormat timecode_format)
    {
        switch (timecode_format) {
            case timecode_24:
                return _24fps;
            case timecode_30:
                return _30fps;
            case timecode_23976:
                return _23976fps;
            case timecode_2997:
                return _2997fps;
            case timecode_2997drop:
                return _2997d_fps;
            case timecode_30drop:
                return _30d_fps;
            case timecode_25:
            default:
                return _25fps;
        }
    }

    Timecode::TimecodeFormat
    FrameRateDropdownItem_to_TimecodeFormat (int dropdown_item)
    {
        switch (dropdown_item) {
            case _24fps:
                return timecode_24;
            case _30fps:
                return timecode_30;
            case _23976fps:
                return timecode_23976;
            case _2997fps:
                return timecode_2997;
            case _2997d_fps:
                return timecode_2997drop;
            case _30d_fps:
                return timecode_30drop;
            case _25fps:
            default:
                return timecode_25;
        }
    }

    
    std::string
    xml_string_to_user_string (std::string xml_string)
    {
        // Bit depth format
        if(xml_string == enum_2_string (FormatFloat))
            return string_bit32;
        
        if(xml_string == enum_2_string (FormatInt24))
            return string_bit24;
        
        if(xml_string == enum_2_string (FormatInt16))
            return string_bit16;
        
        
        // Header format (File type)
        if(xml_string == enum_2_string (CAF))
            return string_CAF;
        
        if(xml_string == enum_2_string (BWF))
            return string_BWav;
        
        if(xml_string == enum_2_string (AIFF))
            return string_Aiff;
        
        if(xml_string == enum_2_string (WAVE64))
            return string_Wav64;
        
        // fps (Timecode)
        if(xml_string == enum_2_string (Timecode::timecode_24))
            return string_24fps;
        
        if(xml_string == enum_2_string (Timecode::timecode_25))
            return string_25fps;
        
        if(xml_string == enum_2_string (Timecode::timecode_30))
            return string_30fps;
        
        if(xml_string == enum_2_string (Timecode::timecode_23976))
            return string_23976fps;
        
        if(xml_string == enum_2_string (Timecode::timecode_2997))
            return string_2997fps;
        
        if(xml_string == enum_2_string (Timecode::timecode_2997drop))
            return string_2997drop_fps;
        
        if(xml_string == enum_2_string (Timecode::timecode_30drop))
            return string_30drop_fps;
        
        return std::string("");
    }

}

void
TracksControlPanel::populate_frame_rate_dropdown()
{
    // Get FRAME_RATE from last used session
    std::string last_used_frame_rate = read_property_from_last_session(Timecode_Format);
    
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    Timecode::TimecodeFormat timecode_format = string_to_TimecodeFormat(last_used_frame_rate);
    ardour_ui->set_timecode_format( timecode_format );
    
    {
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
        _frame_rate_dropdown.set_current_item_by_data_i ( TimecodeFormat_to_FrameRateDropdownItem (timecode_format) );
	}
    
    return;
}

void
TracksControlPanel::populate_auto_lock_timer_dropdown()
{
    int time = ARDOUR_UI::config()->get_auto_lock_timer();
    size_t size = _auto_lock_timer_dropdown.get_menu ().items ().size ();
    for (size_t i = 0; i < size; ++i) {
        if (_auto_lock_timer_dropdown.get_item_data_i (i) == time) {
        	_auto_lock_timer_dropdown.set_current_item (i);
            break;
        }
    }
}

void
TracksControlPanel::populate_auto_save_timer_dropdown()
{
    int time = ARDOUR_UI::config()->get_auto_save_timer();
    size_t size = _auto_save_timer_dropdown.get_menu ().items ().size ();
    for (size_t i = 0; i < size; ++i) {
        if (_auto_save_timer_dropdown.get_item_data_i (i) == time) {
            _auto_save_timer_dropdown.set_current_item (i);
            break;
        }
    }
}

void
TracksControlPanel::populate_pre_record_buffer_dropdown()
{
    int time = ARDOUR_UI::config()->get_pre_record_buffer();
    size_t size = _pre_record_buffer_dropdown.get_menu ().items ().size ();
    for (size_t i = 0; i < size; ++i) {
        if (_pre_record_buffer_dropdown.get_item_data_i (i) == time) {
        	_pre_record_buffer_dropdown.set_current_item (i);
            break;
        }
    }
}

void
TracksControlPanel::refresh_session_settings_info()
{
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    if( !ardour_ui )
        return;
    
    Session* session = ardour_ui->the_session();
    if( !session )
        return;
    _bit_depth_dropdown.set_current_item_by_data_i ( SampleFormat_to_BitDepthDropdownData (session->config.get_native_file_data_format()) );
    
    _file_type_dropdown.set_current_item_by_data_i ( HeaderFormat_to_FileTypeDropdownData (session->config.get_native_file_header_format()) );
    
    refresh_session_frame_rate ();
}

void
TracksControlPanel::refresh_session_frame_rate ()
{
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    if( !ardour_ui )
        return;
    
    Session* session = ardour_ui->the_session();
    if( !session )
        return;
    _frame_rate_dropdown.set_current_item_by_data_i (TimecodeFormat_to_FrameRateDropdownItem (session->config.get_timecode_format ()) );
}

void
TracksControlPanel::populate_default_session_path()
{
    std::string std_path = Config->get_default_session_parent_dir();
    bool folderExist = Glib::file_test(std_path, FILE_TEST_EXISTS);
    
    if ( !folderExist )
        Config->set_default_session_parent_dir(Glib::get_home_dir());
    
    _default_open_path.set_text(Config->get_default_session_parent_dir());
}

void
TracksControlPanel::populate_engine_dropdown()
{
	if (_ignore_changes) {
		return;
	}

    std::vector<const AudioBackendInfo*> backends;
    EngineStateController::instance()->available_backends(backends);

	if (backends.empty()) {
		WavesMessageDialog message_dialog ("", string_compose (_("No audio/MIDI backends detected. %1 cannot run\n(This is a build/packaging/system error.\nIt should never happen.)"), PROGRAM_NAME));
		message_dialog.run ();
		throw failed_constructor ();
	}
	for (std::vector<const AudioBackendInfo*>::const_iterator b = backends.begin(); b != backends.end(); ++b) {
		_engine_dropdown.add_menu_item ((*b)->name, strdup ((*b)->name), dropdown_element_data_cleaner);
	}

	{
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		_engine_dropdown.set_sensitive (backends.size() > 1);
	}

	if (!backends.empty() )
	{
        std::string backend_name = EngineStateController::instance()->get_current_backend_name ();
		_engine_dropdown.set_current_item_by_data_str (backend_name);
	}
}

void
TracksControlPanel::populate_device_dropdown ()
{
    std::vector<AudioBackend::DeviceStatus> all_devices;
	EngineStateController::instance()->enumerate_devices (all_devices);

	_device_dropdown.clear_items ();
	for (std::vector<AudioBackend::DeviceStatus>::const_iterator i = all_devices.begin(); i != all_devices.end(); ++i) {
		_device_dropdown.add_menu_item (i->name, strdup (i->name.c_str ()), dropdown_element_data_cleaner);
	}

	{
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		_device_dropdown.set_sensitive (all_devices.size() > 1);
	}

    if(!all_devices.empty() ) {
        std::string device_name = EngineStateController::instance ()->get_current_device_name ();
		_device_dropdown.set_current_item_by_data_str (device_name);
        device_changed();
    }
}

void
TracksControlPanel::populate_file_type_dropdown()
{
    // Get FILE_TYPE from last used session
    std::string header_format_string = read_property_from_last_session(Native_File_Header_Format);
    
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    HeaderFormat header_format = string_to_HeaderFormat(header_format_string);
    ardour_ui->set_header_format( header_format );
	{
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
        _file_type_dropdown.set_current_item_by_data_i ( HeaderFormat_to_FileTypeDropdownData (header_format) );
    }
    return;
}

void
TracksControlPanel::populate_sample_rate_dropdown()
{
    std::vector<float> sample_rates;
    EngineStateController::instance()->available_sample_rates_for_current_device(sample_rates);

	_sample_rate_dropdown.clear_items ();

	for (std::vector<float>::const_iterator x = sample_rates.begin(); x != sample_rates.end(); ++x) {
        std::string _rate_as_string = ARDOUR_UI_UTILS::rate_as_string (*x);
        _sample_rate_dropdown.add_menu_item (_rate_as_string, strdup (_rate_as_string.c_str ()), dropdown_element_data_cleaner);
    }

	// set _ignore_changes flag to ignore changes in combo-box callbacks
	PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
	_sample_rate_dropdown.set_sensitive (sample_rates.size() > 1);

	if (!sample_rates.empty() ) {
		std::string active_sr = ARDOUR_UI_UTILS::rate_as_string(EngineStateController::instance()->get_current_sample_rate() );
		_sample_rate_dropdown.set_current_item_by_data_str (active_sr);
	}
}

void
TracksControlPanel::populate_buffer_size_dropdown()
{
	std::vector<pframes_t> buffer_sizes;
	EngineStateController::instance()->available_buffer_sizes_for_current_device(buffer_sizes);

	_buffer_size_dropdown.clear_items ();
	for (std::vector<pframes_t>::const_iterator x = buffer_sizes.begin(); x != buffer_sizes.end(); ++x) {
        std::string _bufsize_as_string = bufsize_as_string (*x);
		_buffer_size_dropdown.add_menu_item (_bufsize_as_string, strdup (_bufsize_as_string.c_str ()), dropdown_element_data_cleaner);
	}

	{
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		_buffer_size_dropdown.set_sensitive (buffer_sizes.size() > 1);

		if (!buffer_sizes.empty() ) {
			std::string active_bs = bufsize_as_string(EngineStateController::instance()->get_current_buffer_size());
			_buffer_size_dropdown.set_current_item_by_data_str (active_bs);
		}
	}
}

void
TracksControlPanel::populate_mtc_in_dropdown ()
{
    std::vector<EngineStateController::MidiPortState> midi_states;
    static const char* midi_port_name_prefix = "system_midi:";
    const char* midi_type_suffix;
    
    EngineStateController::instance ()->get_physical_midi_input_states (midi_states);
    midi_type_suffix = X_ (" capture");
    
    _mtc_in_dropdown.clear_items ();
    
    _mtc_in_dropdown.add_menu_item ("Off", strdup (""), dropdown_element_data_cleaner);
    
    std::vector<EngineStateController::MidiPortState>::const_iterator state_iter;
    for (state_iter = midi_states.begin (); state_iter != midi_states.end (); ++state_iter) {
        
	    // strip the device name from input port name
	    std::string device_name(state_iter->name);
	    replace_all (device_name, midi_port_name_prefix, "");
	    replace_all (device_name, midi_type_suffix, "");
        
        _mtc_in_dropdown.add_menu_item (device_name, strdup (state_iter->name.c_str()),dropdown_element_data_cleaner);
    }
    
    display_mtc_in_source ();
}

void
TracksControlPanel::populate_output_mode()
{
    _multi_out_button.set_active(Config->get_output_auto_connect() & AutoConnectPhysical);
    _stereo_out_button.set_active(Config->get_output_auto_connect() & AutoConnectMaster);
    
    _all_outputs_on_button.set_sensitive(Config->get_output_auto_connect() & AutoConnectPhysical);
    _all_outputs_off_button.set_sensitive(Config->get_output_auto_connect() & AutoConnectPhysical);
}


void
TracksControlPanel::populate_input_channels ()
{
    cleanup_input_channels_list();
    
    // process captures (inputs)
    std::vector<EngineStateController::PortState> input_states;
    EngineStateController::instance()->get_physical_audio_input_states(input_states);
    
    std::vector<EngineStateController::PortState>::const_iterator input_iter;
    
    uint16_t number_count = 1;
    for (input_iter = input_states.begin(); input_iter != input_states.end(); ++input_iter ) {
        
        uint16_t number = DeviceConnectionControl::NoNumber;
        std::string track_name;

        if (input_iter->active) {
	        std::string port_name(input_iter->name);
	        replace_all (port_name, audio_capture_name_prefix, "");
	        number = number_count++;
	        
	        if (Config->get_tracks_auto_naming () & UseDefaultNames) {
		        track_name = string_compose ("%1 %2", Session::default_track_name_pattern (DataType::AUDIO), number);
	        } else if (Config->get_tracks_auto_naming () & NameAfterDriver) {
		        track_name = port_name;
	        }
        }
        
        add_device_capture_control (input_iter->name, input_iter->active, number, track_name);
    }
    
    _all_inputs_on_button.set_sensitive (!input_states.empty () );
    _all_inputs_off_button.set_sensitive (!input_states.empty () );
    
    // if list of audio-in ports was changed, list of ltc-in ports must be also changed
    populate_ltc_in_dropdown ();
}


void
TracksControlPanel::populate_ltc_in_dropdown ()
{
    _ltc_in_dropdown.clear_items ();
    std::vector<EngineStateController::PortState> input_states;
    EngineStateController::instance ()->get_physical_audio_input_states (input_states);
    
    std::vector<EngineStateController::PortState>::const_iterator input_iter;
    
    for (input_iter = input_states.begin(); input_iter != input_states.end (); ++input_iter ) {
	    std::string port_name (input_iter->name);
	    replace_all (port_name, audio_capture_name_prefix, "");
	    
	    _ltc_in_dropdown.add_menu_item (port_name, strdup (input_iter->name.c_str ()), dropdown_element_data_cleaner );
    }
    
    display_ltc_in_source ();
}


void
TracksControlPanel::populate_output_channels()
{
    cleanup_output_channels_list();
        
    // process captures (outputs)
    std::vector<EngineStateController::PortState> output_states;
    EngineStateController::instance ()->get_physical_audio_output_states (output_states);
    
    std::vector<EngineStateController::PortState>::const_iterator output_iter;
    
    uint16_t number_count = 1;
    for (output_iter = output_states.begin (); output_iter != output_states.end(); ++output_iter ) {
        
        uint16_t number = DeviceConnectionControl::NoNumber;
        
        if (output_iter->active) {
            number = number_count++;
        }
        
        add_device_playback_control (output_iter->name, output_iter->active, number);
    }
    
    bool stereo_out_disabled = (Config->get_output_auto_connect () & AutoConnectPhysical);
    _all_outputs_on_button.set_sensitive(!output_states.empty () && stereo_out_disabled );
    _all_outputs_off_button.set_sensitive(!output_states.empty () && stereo_out_disabled );
    
    // if the list of audio-out ports was changed, the list of ltc-out ports must be also changed
    populate_ltc_out_dropdown ();
}


void
TracksControlPanel::populate_ltc_out_dropdown ()
{
    _ltc_out_dropdown.clear_items ();
    std::vector<EngineStateController::PortState> output_states;
    EngineStateController::instance ()->get_physical_audio_output_states (output_states);
    
    std::vector<EngineStateController::PortState>::const_iterator output_iter;
    
    for (output_iter = output_states.begin(); output_iter != output_states.end (); ++output_iter ) {
	    // strip the device name from input port name
	    std::string port_name (output_iter->name);
	    replace_all (port_name, audio_playback_name_prefix, "");
	    
	    _ltc_out_dropdown.add_menu_item (port_name, strdup (output_iter->name.c_str ()), dropdown_element_data_cleaner);
    }
    
    display_ltc_output_port ();
}


void
TracksControlPanel::populate_midi_ports ()
{
    cleanup_midi_device_list();
    
    std::vector<EngineStateController::MidiPortState> midi_input_states, midi_output_states;
    EngineStateController::instance()->get_physical_midi_input_states(midi_input_states);
    EngineStateController::instance()->get_physical_midi_output_states(midi_output_states);
    
    // now group corresponding inputs and outputs into a std::vector of midi device descriptors
    MidiDeviceDescriptorVec midi_device_descriptors;
    std::vector<EngineStateController::MidiPortState>::const_iterator state_iter;
    // process inputs
    for (state_iter = midi_input_states.begin(); state_iter != midi_input_states.end(); ++state_iter) {
        // strip the device name from input port name
	    std::string device_name(state_iter->name);
	    replace_all (device_name, midi_port_name_prefix, "");
	    replace_all (device_name, midi_capture_suffix, "");
        
        MidiDeviceDescriptor device_descriptor(device_name);
        device_descriptor.capture_name = state_iter->name;
        device_descriptor.capture_active = state_iter->active;
        midi_device_descriptors.push_back(device_descriptor);
    }
    
    // process outputs
    for (state_iter = midi_output_states.begin(); state_iter != midi_output_states.end(); ++state_iter){
        // strip the device name from input port name
	    std::string device_name(state_iter->name);
	    replace_all (device_name, midi_port_name_prefix, "");
	    replace_all (device_name, midi_playback_suffix, "");
        
        // check if we already have descriptor for this device
        MidiDeviceDescriptor device_descriptor(device_name);
        MidiDeviceDescriptorVec::iterator found_iter;
        found_iter = std::find(midi_device_descriptors.begin(), midi_device_descriptors.end(), device_descriptor );
        
        if (found_iter != midi_device_descriptors.end() ) {
            found_iter->playback_name = state_iter->name;
            found_iter->playback_active = state_iter->active;
        } else {
            device_descriptor.capture_name.clear();
            device_descriptor.playback_name = state_iter->name;
            device_descriptor.playback_active = state_iter->active;
            midi_device_descriptors.push_back(device_descriptor);
        }
    }
    
    // now add midi device controls
    MidiDeviceDescriptorVec::iterator iter;
    for (iter = midi_device_descriptors.begin(); iter != midi_device_descriptors.end(); ++iter ) {
        add_midi_device_control (iter->name, iter->capture_name, iter->capture_active,
                                            iter->playback_name, iter->playback_active);
    }
}


void
TracksControlPanel::cleanup_input_channels_list()
{
    std::vector<Gtk::Widget*> capture_controls = _device_capture_list.get_children();
        
    while (capture_controls.size() != 0) {
        Gtk::Widget* item = capture_controls.back();
        
        DeviceConnectionControl* control = dynamic_cast<DeviceConnectionControl*>(item);
        
        if (control) {
            control->remove_data(DeviceConnectionControl::id_name);
        }
        
        capture_controls.pop_back();
        _device_capture_list.remove(*item);
        delete item;
    }
}


void
TracksControlPanel::cleanup_output_channels_list()
{
    std::vector<Gtk::Widget*> playback_controls = _device_playback_list.get_children();

    while (playback_controls.size() != 0) {
        Gtk::Widget* item = playback_controls.back();
        
        DeviceConnectionControl* control = dynamic_cast<DeviceConnectionControl*>(item);
        
        if (control) {
            control->remove_data(DeviceConnectionControl::id_name);
        }
        
        playback_controls.pop_back();
        _device_playback_list.remove(*item);
        delete item;
    }
}


void
TracksControlPanel::cleanup_midi_device_list()
{
    std::vector<Gtk::Widget*> midi_device_controls = _midi_device_list.get_children();
    
    while (midi_device_controls.size() != 0) {
        Gtk::Widget* item = midi_device_controls.back();
        
        MidiDeviceConnectionControl* control = dynamic_cast<MidiDeviceConnectionControl*>(item);
        
        if (control) {
            control->remove_data(MidiDeviceConnectionControl::capture_id_name);
            control->remove_data(MidiDeviceConnectionControl::playback_id_name);
        }
        
        midi_device_controls.pop_back();
        _midi_device_list.remove(*item);
        delete item;
    }
}

void TracksControlPanel::display_waveform_shape ()
{
	ARDOUR::WaveformShape shape = ARDOUR_UI::config()->get_waveform_shape ();
	switch (shape) {
	case Traditional:
		_waveform_shape_dropdown.set_current_item (0);
		break;
	case Rectified:
		_waveform_shape_dropdown.set_current_item (1);
		break;
	default:
		dbg_msg ("TracksControlPanel::display_waveform_shape ():\nUnexpected WaveFormShape !");
		break;
	}
}

void
TracksControlPanel::display_meter_hold ()
{
	float peak_hold_time = ARDOUR_UI::config()->get_meter_hold ();
	int selected_item = 0;
	if (peak_hold_time <= (MeterHoldOff + 0.1)) {
		selected_item = 0;
	} else if (peak_hold_time <= (MeterHoldShort + 0.1)) {
		selected_item = 1;
	} else if (peak_hold_time <= (MeterHoldMedium + 0.1)) {
		selected_item = 2;
	} else if (peak_hold_time <= (MeterHoldLong + 0.1)) {
		selected_item = 3;
	} 
	_peak_hold_time_dropdown.set_current_item (selected_item);
}

void
TracksControlPanel::display_meter_falloff ()
{
	float meter_falloff = Config->get_meter_falloff ();
	int selected_item = 0;

	if (meter_falloff <= (METER_FALLOFF_OFF + 0.1)) {
		selected_item = 0;
	} else if (meter_falloff <= (METER_FALLOFF_SLOWEST + 0.1)) {
		selected_item = 1;
	} else if (meter_falloff <= (METER_FALLOFF_SLOW + 0.1)) {
		selected_item = 2;
	} else if (meter_falloff <= (METER_FALLOFF_SLOWISH + 0.1)) {
		selected_item = 3;
	} else if (meter_falloff <= (METER_FALLOFF_MODERATE + 0.1)) {
		selected_item = 4;
	} else if (meter_falloff <= (METER_FALLOFF_MEDIUM + 0.1)) {
		selected_item = 5;
	} else if (meter_falloff <= (METER_FALLOFF_FAST + 0.1)) {
		selected_item = 6;
	} else if (meter_falloff <= (METER_FALLOFF_FASTER + 0.1)) {
		selected_item = 7;
	} else if (meter_falloff <= (METER_FALLOFF_FASTEST + 0.1)) {
		selected_item = 8;
	} 
	_dpm_fall_off_dropdown.set_current_item (selected_item);
}

void
TracksControlPanel::display_hdd_buffering ()
{
	BufferingPreset preset = Config->get_buffering_preset ();
	int selected_item = 0;

    switch (preset) {
        case Small:
            selected_item = 0;
            break;
        case Medium:
            selected_item = 1;
            break;
        case Large:
            selected_item = 2;
            break;
		case Custom:
		default:
			break;
    }
    
    _hard_disk_buffering_dropdown.set_current_item (selected_item);
}	

void
TracksControlPanel::display_mmc_control ()
{
	_obey_mmc_commands_button.set_active_state (Config->get_mmc_control () ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
TracksControlPanel::display_send_mmc ()
{
	_send_mmc_commands_button.set_active_state (Config->get_send_mmc () ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
TracksControlPanel::display_mmc_send_device_id ()
{
	_outbound_mmc_device_spinbutton.set_value (Config->get_mmc_send_device_id ());
}

void
TracksControlPanel::display_mmc_receive_device_id ()
{
	_inbound_mmc_device_spinbutton.set_value (Config->get_mmc_receive_device_id ());
}

void
TracksControlPanel::display_history_depth ()
{
	_limit_undo_history_spinbutton.set_value (Config->get_history_depth ());
}

void
TracksControlPanel::display_saved_history_depth ()
{
	_save_undo_history_spinbutton.set_value (Config->get_saved_history_depth ());
}

void
TracksControlPanel::display_only_copy_imported_files ()
{
	_copy_imported_files_button.set_active_state (ARDOUR_UI::config()->get_only_copy_imported_files () ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
TracksControlPanel::display_color_track_panel ()
{
	_color_track_panel_button.set_active_state (ARDOUR_UI::config()->get_color_track_panel () ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
TracksControlPanel::display_denormal_protection ()
{
	_dc_bias_against_denormals_button.set_active_state (Config->get_denormal_protection () ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
TracksControlPanel::display_current_sync_tool ()
{
    ARDOUR::SyncSource sync_tool_type = Config->get_sync_source ();
    
    _mtc_in_dropdown.set_visible (sync_tool_type == ARDOUR::MTC);
    _ltc_in_dropdown.set_visible (sync_tool_type == ARDOUR::LTC
                                  && !EngineStateController::instance ()->get_ltc_source_port ().empty ());
    _sync_input_port_layout.set_visible (_mtc_in_dropdown.get_visible () || _ltc_in_dropdown.get_visible ());
    
    _sync_tool_dropdown.set_current_item (SyncSource_to_SyncSourceTracks (sync_tool_type));
    
    _freewheel_vbox.set_visible (_mtc_in_dropdown.get_visible () || _ltc_in_dropdown.get_visible ());
}

void
TracksControlPanel::display_mtc_in_source ()
{
    std::string mtc_in_source = EngineStateController::instance ()->get_mtc_source_port ();

    int size = _mtc_in_dropdown.get_menu ().items ().size ();
    for (int i = 0; i < size; ++i) {
        char* full_name_of_mtc_input_port = (char*) _mtc_in_dropdown.get_item_data_pv (i);
        if (full_name_of_mtc_input_port && full_name_of_mtc_input_port == mtc_in_source ) {
            _mtc_in_dropdown.set_current_item (i);
            return ;
        }
    }
}

void
TracksControlPanel::display_ltc_in_source ()
{
    std::string ltc_in_source = EngineStateController::instance ()->get_ltc_source_port ();
    
    if (Config->get_sync_source () == ARDOUR::LTC) {
        if (ltc_in_source.empty ()) {
            _ltc_in_dropdown.set_visible (false);
            _sync_input_port_layout.set_visible (false);
            _freewheel_vbox.set_visible (false);
            return ;
        } else {
            _ltc_in_dropdown.set_visible (true);
            _sync_input_port_layout.set_visible (true);
            _freewheel_vbox.set_visible (true);
        }
    }
    
    int size = _ltc_in_dropdown.get_menu ().items ().size ();
    for (int i = 0; i < size; ++i) {
        char* full_name_of_ltc_input_port = (char*) _ltc_in_dropdown.get_item_data_pv (i);
        if (full_name_of_ltc_input_port && full_name_of_ltc_input_port == ltc_in_source ) {
            _ltc_in_dropdown.set_current_item (i);
            return ;
        }
    }
}


void
TracksControlPanel::display_freewheel_fader ()
{
    uint32_t freewheel_timeout = Config->get_freewheel_timeout_ms ();
    set_value_of_freewheel_display_elements (freewheel_timeout);
}


void
TracksControlPanel::set_value_of_freewheel_display_elements (uint32_t freewheel_timeout)
{
    _freewheel_timeout_adjustment.set_value (freewheel_timeout);
    
    if (freewheel_timeout == 0) {
        _freewheel_timeout_label.set_text ("Off");
    } else if (freewheel_timeout < Config->get_max_freewheel_timeout_ms ()){
        _freewheel_timeout_label.set_text (string_compose ("%1", freewheel_timeout));
    } else {
        _freewheel_timeout_label.set_text ("Jam Sync");
    }

}


void
TracksControlPanel::display_ltc_output_port ()
{
    // get the list of audio output
    std::vector<EngineStateController::PortState> output_states;
    EngineStateController::instance ()->get_physical_audio_output_states (output_states);
    
    if (output_states.empty ()) {
        // no output audio port
        ltc_output_settings_set_visible (false);
        return ;
    } else {
        ltc_output_settings_set_visible (true);
    }
    
    std::string ltc_output_port = EngineStateController::instance ()->get_ltc_output_port ();
    
    int size = _ltc_out_dropdown.get_menu ().items ().size ();
    for (int i = 0; i < size; ++i) {
        char* full_name_of_ltc_output_port = (char*) _ltc_out_dropdown.get_item_data_pv (i);
        if (full_name_of_ltc_output_port && full_name_of_ltc_output_port == ltc_output_port ) {
            _ltc_out_dropdown.set_current_item (i);
            return ;
        }
    }
}

void
TracksControlPanel::display_transport_record_locked ()
{
    _transport_record_locked_button.set_active_state (Config->get_latched_record_enable () ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
TracksControlPanel::display_enable_ltc_generator ()
{
    _enable_ltc_generator_button.set_active_state (Config->get_send_ltc () ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);

    // if generator is enabled - show its settings
    ltc_generator_settings_set_visible (Config->get_send_ltc ());
}

void
TracksControlPanel::display_ltc_send_continuously ()
{
    _ltc_send_continuously_button.set_active_state (Config->get_ltc_send_continuously () ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
TracksControlPanel::display_ltc_generator_level_fader ()
{
    double generator_level_in_db = accurate_coefficient_to_dB (Config->get_ltc_output_volume ());
    _ltc_generator_level_adjustment.set_value (generator_level_in_db);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision (1) << generator_level_in_db;
    _ltc_generator_level_label.set_text (ss.str ());
}


void
TracksControlPanel::ltc_output_settings_set_visible (bool visible)
{
    _enable_ltc_generator_vbox.set_visible (visible);
    _ltc_output_port_vbox.set_visible (visible);
    // show generator settings only if it's enabled
    ltc_generator_settings_set_visible (visible && Config->get_send_ltc ());
}

void
TracksControlPanel::ltc_generator_settings_set_visible (bool visible)
{
    _ltc_send_continuously_hbox.set_visible (visible);
    _ltc_generator_level_vbox.set_visible (visible);
}


void
TracksControlPanel::display_general_preferences ()
{
	display_waveform_shape ();
	display_meter_hold ();
	display_meter_falloff ();
	display_hdd_buffering ();
	display_mmc_control ();
	display_send_mmc ();
	display_mmc_send_device_id ();
	display_mmc_receive_device_id ();
	display_only_copy_imported_files ();
	display_color_track_panel ();
	display_history_depth ();
	display_saved_history_depth ();
	display_denormal_protection ();
    display_current_sync_tool ();
    display_transport_record_locked ();
    display_freewheel_fader ();
    display_enable_ltc_generator ();
    display_ltc_send_continuously ();
    display_ltc_generator_level_fader ();
}

#define RGB_TO_UINT(r,g,b) ((((guint)(r))<<16)|(((guint)(g))<<8)|((guint)(b)))
#define RGB_TO_RGBA(x,a) (((x) << 8) | ((((guint)a) & 0xff)))
#define RGBA_TO_UINT(r,g,b,a) RGB_TO_RGBA(RGB_TO_UINT(r,g,b), a)
void
TracksControlPanel::save_general_preferences ()
{
	int selected_item = _waveform_shape_dropdown.get_current_item ();
	switch (selected_item) {
	case 0:
		ARDOUR_UI::config()->set_waveform_shape (Traditional);
		break;
	case 1:
		ARDOUR_UI::config()->set_waveform_shape (Rectified);
		break;
	default:
		dbg_msg ("TracksControlPanel::general_preferences ():\nUnexpected WaveFormShape !");
		break;
	}
    
	selected_item = _peak_hold_time_dropdown.get_current_item ();
	switch (selected_item) {
	case 0:
		ARDOUR_UI::config()->set_meter_hold (MeterHoldOff);
		break;
	case 1:
		ARDOUR_UI::config()->set_meter_hold (MeterHoldShort);
		break;
	case 2:
		ARDOUR_UI::config()->set_meter_hold (MeterHoldMedium);
		break;
	case 3:
		ARDOUR_UI::config()->set_meter_hold (MeterHoldLong);
		break;
	default:
		dbg_msg ("TracksControlPanel::general_preferences ():\nUnexpected peak hold time!");
		break;
	}

	selected_item = _dpm_fall_off_dropdown.get_current_item ();
	switch (selected_item) {
	case 0:
		Config->set_meter_falloff (METER_FALLOFF_OFF);
		break;
	case 1:
		Config->set_meter_falloff (METER_FALLOFF_SLOWEST);
		break;
	case 2:
		Config->set_meter_falloff (METER_FALLOFF_SLOW);
		break;
	case 3:
		Config->set_meter_falloff (METER_FALLOFF_SLOWISH);
		break;
	case 4:
		Config->set_meter_falloff (METER_FALLOFF_MODERATE);
		break;
	case 5:
		Config->set_meter_falloff (METER_FALLOFF_MEDIUM);
		break;
	case 6:
		Config->set_meter_falloff (METER_FALLOFF_FAST);
		break;
	case 7:
		Config->set_meter_falloff (METER_FALLOFF_FASTER);
		break;
	case 8:
		Config->set_meter_falloff (METER_FALLOFF_FASTEST);
		break;
	default:
		dbg_msg ("TracksControlPanel::general_preferences ():\nUnexpected meter fall off time!");
		break;
	}
    
	Config->set_mmc_control (_obey_mmc_commands_button.active_state () == Gtkmm2ext::ExplicitActive);
	Config->set_send_mmc (_send_mmc_commands_button.active_state () == Gtkmm2ext::ExplicitActive);
	ARDOUR_UI::config()->set_only_copy_imported_files (_copy_imported_files_button.active_state () ==  Gtkmm2ext::ExplicitActive);
	ARDOUR_UI::config()->set_color_track_panel (_color_track_panel_button.active_state () ==  Gtkmm2ext::ExplicitActive);
	Config->set_denormal_protection (_dc_bias_against_denormals_button.active_state () == Gtkmm2ext::ExplicitActive);

	Config->set_mmc_receive_device_id (_inbound_mmc_device_spinbutton.get_value ());
	Config->set_mmc_send_device_id (_outbound_mmc_device_spinbutton.get_value ());
	Config->set_history_depth (_limit_undo_history_spinbutton.get_value ());
	Config->set_saved_history_depth (_save_undo_history_spinbutton.get_value ());
	Config->set_save_history (_save_undo_history_spinbutton.get_value () > 0);
    
    EngineStateController::instance ()->set_latched_record_enable (_transport_record_locked_button.active_state () == Gtkmm2ext::ExplicitActive);

    int cur_item_num = _hard_disk_buffering_dropdown.get_current_item ();

    BufferingPreset preset;
    preset = BufferingPreset(_hard_disk_buffering_dropdown.get_item_data_u (cur_item_num));

	Config->set_buffering_preset (preset);
    
    Config->set_freewheel_timeout_ms (_freewheel_timeout_adjustment.get_value ());
}

void TracksControlPanel::on_engine_dropdown_item_clicked (WavesDropdown*, int)
{
	if (_ignore_changes) {
		return;
	}

	const char* backend_name = (char*) _engine_dropdown.get_item_data_pv (_engine_dropdown.get_current_item ());
    if (!backend_name) {
        return ;
    }
    
	if ( EngineStateController::instance()->set_new_backend_as_current (backend_name) )
	{
		_have_control = EngineStateController::instance ()->is_setup_required ();
        populate_device_dropdown ();
        return;
	}
    
    std::cerr << "\tfailed to set backend [" << backend_name << "]\n";
}

void
TracksControlPanel::on_device_dropdown_item_clicked (WavesDropdown*, int)
{
	if (_ignore_changes) {
		return;
	}
    const char* device_name_ptr = (char*) _device_dropdown.get_item_data_pv (_device_dropdown.get_current_item ());
    if (!device_name_ptr || device_name_ptr == EngineStateController::instance()->get_current_device_name ()) {
        // nothing to do
        return ;
    }
    
    std::string device_name = device_name_ptr;
    
    std::string message = _("Would you like to switch to ") + device_name + "?";

    set_keep_above (false);
    WavesMessageDialog message_dialog ("",
									  message,
									  WavesMessageDialog::BUTTON_YES |
									  WavesMessageDialog::BUTTON_NO);
    int response = message_dialog.run ();
    if ( response == Gtk::RESPONSE_YES || response == WavesDialog::RESPONSE_DEFAULT ) {
        set_keep_above (true);
        device_changed ();
    } else {
        // set _ignore_changes flag to ignore changes in combo-box callbacks
        PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
        std::string device_name = EngineStateController::instance()->get_current_device_name ();
        _device_dropdown.set_current_item_by_data_str (device_name);
        set_keep_above (true);
        return;
    }
}

void
TracksControlPanel::device_changed ()
{
	if (_ignore_changes) {
		return ;
	}
    
    const char* device_name_ptr = (char*) _device_dropdown.get_item_data_pv (_device_dropdown.get_current_item ());
    
    if (!device_name_ptr)
        return ;
    
    if (EngineStateController::instance()->set_new_device_as_current (device_name_ptr) )
    {
        populate_buffer_size_dropdown ();
        populate_sample_rate_dropdown ();
        // disable LTC generator in case of device change
        set_ltc_generator_status (false);
        return ;
    }
    
    {
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		// restore previous device name in combo box
        std::string device_name = EngineStateController::instance()->get_current_device_name ();
        _device_dropdown.set_current_item_by_data_str (device_name);
	}
    set_keep_above (false);
    WavesMessageDialog (PROGRAM_NAME, _("Error activating selected device.")).run();
    set_keep_above (true);
}


void
TracksControlPanel::on_all_inputs_on_button(WavesButton*)
{
    EngineStateController::instance()->set_state_to_all_inputs(true);
}

void
TracksControlPanel::on_name_tracks_after_driver(WavesButton*)
{
    _yes_button.set_visible(true);
    _no_button.set_visible(true);

    _tracks_naming_rule = NameAfterDriver;
}

void
TracksControlPanel::on_reset_tracks_name_to_default(WavesButton*)
{
    _yes_button.set_visible(true);
    _no_button.set_visible(true);
    
    _tracks_naming_rule = UseDefaultNames;
}

void
TracksControlPanel::on_yes_button(WavesButton*)
{
    Config->set_tracks_auto_naming(_tracks_naming_rule);
    
    _yes_button.set_visible(false);
    _no_button.set_visible(false);
}

void
TracksControlPanel::on_no_button(WavesButton*)
{
    _yes_button.set_visible(false);
    _no_button.set_visible(false);
}

void
TracksControlPanel::on_control_panel_button(WavesButton*)
{
    AudioEngine::instance()->launch_device_control_app ();
}

void
TracksControlPanel::on_all_inputs_off_button(WavesButton*)
{
    EngineStateController::instance()->set_state_to_all_inputs(false);
}

void
TracksControlPanel::on_all_outputs_on_button(WavesButton*)
{
    EngineStateController::instance()->set_state_to_all_outputs(true);
}

void
TracksControlPanel::on_all_outputs_off_button(WavesButton*)
{
    EngineStateController::instance()->set_state_to_all_outputs(false);
}

void
TracksControlPanel::on_file_type_dropdown_item_clicked (WavesDropdown*, int)
{ 
}

void
TracksControlPanel::on_bit_depth_dropdown_item_clicked (WavesDropdown*, int)
{
}

void
TracksControlPanel::on_frame_rate_item_clicked (WavesDropdown*, int)
{
}

void 
TracksControlPanel::on_buffer_size_dropdown_item_clicked (WavesDropdown*, int)
{
	if (_ignore_changes) {
		return;
	}

	pframes_t new_buffer_size = get_buffer_size();
	if ( EngineStateController::instance()->set_new_buffer_size_in_controller(new_buffer_size) ) {
		EngineStateController::instance()->push_current_state_to_backend (false);

	} else {

		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
        // restore current buffer size value in combo box
        std::string buffer_size_str = bufsize_as_string (EngineStateController::instance()->get_current_buffer_size() );
        WavesMessageDialog msg("", _("Buffer size set to the value which is not supported"));
        msg.run();
        _buffer_size_dropdown.set_current_item_by_data_str (buffer_size_str);
    }

	show_buffer_duration();
}

void
TracksControlPanel::on_sample_rate_dropdown_item_clicked (WavesDropdown*, int)
{
	if (_ignore_changes) {
		return;
	}

	framecnt_t new_sample_rate = get_sample_rate ();
    if ( EngineStateController::instance()->set_new_sample_rate_in_controller (new_sample_rate) ) {
		EngineStateController::instance()->push_current_state_to_backend (false);
    
	} else {

		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		// restore current sample rate value in combo box
		std::string sample_rate_str = ARDOUR_UI_UTILS::rate_as_string (EngineStateController::instance()->get_current_sample_rate() );
		WavesMessageDialog msg("", _("Sample rate set to the value which is not supported"));
		msg.run();
		_sample_rate_dropdown.set_current_item_by_data_str (sample_rate_str);
	}

	show_buffer_duration ();
}

void
TracksControlPanel::on_mtc_in_dropdown_changed (WavesDropdown* dropdown, int el_number)
{
    mtc_in_dropdown_change (el_number);
}

void
TracksControlPanel::mtc_in_dropdown_change (int el_number)
{
    char* full_name_of_chosen_port = (char*)_mtc_in_dropdown.get_item_data_pv (el_number);

    if (full_name_of_chosen_port) {
        if (full_name_of_chosen_port == EngineStateController::instance ()->get_mtc_source_port ()) {
            return ;
        }
        
        EngineStateController::instance ()->set_mtc_source_port (full_name_of_chosen_port);
    } else {
        EngineStateController::instance ()->set_mtc_source_port ("");
    }
}

void
TracksControlPanel::on_ltc_in_dropdown_changed (WavesDropdown* dropdown, int el_number)
{
    ltc_in_dropdown_change (el_number);
}

void
TracksControlPanel::ltc_in_dropdown_change (int el_number)
{
    char* full_name_of_chosen_port = (char*)_ltc_in_dropdown.get_item_data_pv (el_number);
 
    if (full_name_of_chosen_port) {
        if (full_name_of_chosen_port == EngineStateController::instance ()->get_ltc_source_port ()) {
            return ;
        }
    
        EngineStateController::instance ()->set_ltc_source_port (full_name_of_chosen_port);
    } else {
        EngineStateController::instance ()->set_ltc_source_port ("");
    }
}

void
TracksControlPanel::on_ltc_out_dropdown_changed (WavesDropdown* dropdown, int el_number)
{
    char* full_name_of_chosen_port = (char*)_ltc_out_dropdown.get_item_data_pv (el_number);
    
    if (full_name_of_chosen_port) {
        if (full_name_of_chosen_port == EngineStateController::instance ()->get_ltc_output_port ()) {
            return ;
        }
        EngineStateController::instance ()->set_ltc_output_port (full_name_of_chosen_port);
    } else {
        EngineStateController::instance ()->set_ltc_output_port ("");
    }
}

void
TracksControlPanel::on_sync_tool_dropdown_changed (WavesDropdown* dropdown, int el_number)
{
    ARDOUR::SyncSource sync_tool = SyncSourceTracks_to_SyncSource (el_number);
    if (sync_tool != Config->get_sync_source ()) {
        
        if (_session) {
            // set Internal mode in case of sync tool change
            _session->config.set_external_sync (false);
        }
        
        Config->set_sync_source (sync_tool);
    
        switch (el_number) {
    
            case (TracksControlPanel::MTC):
                EngineStateController::instance()-> set_ltc_source_port ("");
                mtc_in_dropdown_change (0);
                break;
            
            case (TracksControlPanel::LTC):
                EngineStateController::instance()-> set_mtc_source_port ("");
                ltc_in_dropdown_change (0);
                break;
            
            default:
                return ;
        }
    }
}


void
TracksControlPanel::on_enable_ltc_generator_button (WavesButton*)
{
    set_ltc_generator_status (_enable_ltc_generator_button.active_state () == Gtkmm2ext::ExplicitActive);
}

void
TracksControlPanel::set_ltc_generator_status (bool status)
{
   Config->set_send_ltc (status);
}


void
TracksControlPanel::on_ltc_send_continuously_button (WavesButton*)
{
    Config->set_ltc_send_continuously (_ltc_send_continuously_button.active_state () == Gtkmm2ext::ExplicitActive);
}

void
TracksControlPanel::freewheel_timeout_adjustment_changed ()
{
   set_value_of_freewheel_display_elements (_freewheel_timeout_adjustment.get_value ());
}

void
TracksControlPanel::ltc_generator_level_adjustment_changed ()
{
    double generator_level_in_db = _ltc_generator_level_adjustment.get_value (); // -50..0
    Config->set_ltc_output_volume (dB_to_coefficient (generator_level_in_db));
}


void
TracksControlPanel::engine_running ()
{
	populate_buffer_size_dropdown();
	populate_sample_rate_dropdown();
    show_buffer_duration ();
}

void
TracksControlPanel::engine_stopped ()
{
}

void
TracksControlPanel::on_a_settings_tab_button_clicked (WavesButton* clicked_button)
{
	bool visible = (&_midi_settings_tab_button == clicked_button);
	_midi_settings_tab.set_visible (visible);
	_midi_settings_tab_button.set_active(visible);

	visible = (&_session_settings_tab_button == clicked_button);
	_session_settings_tab.set_visible (visible);;
	_session_settings_tab_button.set_active(visible);

	visible = (&_audio_settings_tab_button == clicked_button);
	_audio_settings_tab.set_visible (visible);
	_audio_settings_tab_button.set_active(visible);

	visible = (&_general_settings_tab_button == clicked_button);
	_general_settings_tab.set_visible (visible);
	_general_settings_tab_button.set_active(visible);
    
    visible = (&_sync_settings_tab_button == clicked_button);
    _sync_settings_tab.set_visible (visible);
    _sync_settings_tab_button.set_active(visible);
}

void
TracksControlPanel::show_and_open_tab (int tab_id)
{
    show ();
    
    bool visible = (tab_id == MIDISystemSettingsTab);
	_midi_settings_tab.set_visible (visible);
	_midi_settings_tab_button.set_active(visible);
    
	visible = (tab_id == SessionSettingsTab);
	_session_settings_tab.set_visible (visible);;
	_session_settings_tab_button.set_active(visible);
    
	visible = (tab_id == AudioSystemSettingsTab);
	_audio_settings_tab.set_visible (visible);
	_audio_settings_tab_button.set_active(visible);
    
	visible = (tab_id == PreferencesTab);
	_general_settings_tab.set_visible (visible);
	_general_settings_tab_button.set_active(visible);
    
    visible = (tab_id == SyncTab);
    _sync_settings_tab.set_visible (visible);
    _sync_settings_tab_button.set_active(visible);

}

void
TracksControlPanel::on_device_error (std::string what)
{
	set_keep_above (false);
    WavesMessageDialog message_dialog ("", string_compose (_("%1"), what));
    
    message_dialog.set_position (Gtk::WIN_POS_MOUSE);
    message_dialog.set_keep_above (true);
    message_dialog.run ();
	set_keep_above (true);
}

void
TracksControlPanel::on_multi_out (WavesButton*)
{
    if (Config->get_output_auto_connect() & AutoConnectPhysical) {
        return;
    }
    
    Config->set_output_auto_connect(AutoConnectPhysical);
}

void
TracksControlPanel::on_stereo_out (WavesButton*)
{
    if (Config->get_output_auto_connect() & AutoConnectMaster) {
        return;
    }
    
    Config->set_output_auto_connect(AutoConnectMaster);
}

void
TracksControlPanel::on_browse_button (WavesButton*)
{
    set_keep_above (false);
    _default_path_name = ARDOUR::choose_folder_dialog(Config->get_default_session_parent_dir(), _("Choose Default Path"));
    set_keep_above (true);    
    
    if (!_default_path_name.empty()) {
        _default_open_path.set_text(_default_path_name);
    } else {
        _default_open_path.set_text(Config->get_default_session_parent_dir());
    }
}

void
TracksControlPanel::save_default_session_path()
{
    if(!_default_path_name.empty())
    {
        Config->set_default_session_parent_dir(_default_path_name);
        Config->save_state();
    }
}

void
TracksControlPanel::save_auto_lock_time()
{
    int time = _auto_lock_timer_dropdown.get_item_data_u (_auto_lock_timer_dropdown.get_current_item ());
    ARDOUR_UI::config()->set_auto_lock_timer(time);
}

void
TracksControlPanel::save_auto_save_time()
{
    int time = _auto_save_timer_dropdown.get_item_data_u (_auto_save_timer_dropdown.get_current_item ());
    ARDOUR_UI::config()->set_auto_save_timer(time);
}

void
TracksControlPanel::save_pre_record_buffer()
{
    int time = _pre_record_buffer_dropdown.get_item_data_u (_pre_record_buffer_dropdown.get_current_item ());
    ARDOUR_UI::config()->set_pre_record_buffer(time);
}

void
TracksControlPanel::update_session_config ()
{
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    
    if( ardour_ui )
    {
        int dropdown_data = _frame_rate_dropdown.get_item_data_i (_frame_rate_dropdown.get_current_item ());
        Timecode::TimecodeFormat timecode_format = FrameRateDropdownItem_to_TimecodeFormat (dropdown_data);
        ardour_ui->set_timecode_format (timecode_format);
        
        dropdown_data = _bit_depth_dropdown.get_item_data_i (_bit_depth_dropdown.get_current_item ());
        ARDOUR::SampleFormat sample_format = BitDepthDropdownData_to_SampleFormat (dropdown_data);
        ardour_ui->set_sample_format (sample_format);
        
        dropdown_data = _file_type_dropdown.get_item_data_i (_file_type_dropdown.get_current_item ());
        ARDOUR::HeaderFormat header_format = FileTypeDropdownData_to_HeaderFormat (dropdown_data);
        ardour_ui->set_header_format (header_format);
        
        ARDOUR::Session* session = ardour_ui->the_session();
        
        if ( session )
        {
            session->config.set_native_file_header_format (header_format);
            
            session->config.set_native_file_data_format (sample_format);
            
            session->config.set_timecode_format (timecode_format);
        }
    }
}

void
TracksControlPanel::update_configs()
{
    // update session config
    update_session_config ();
    
    // update global config
    save_default_session_path ();
    save_auto_lock_time ();
    save_auto_save_time ();
    save_pre_record_buffer ();
	save_general_preferences ();

    // save ARDOUR_UI::config to disk persistently
    ARDOUR_UI::config()->save_state();
}

void
TracksControlPanel::on_ok (WavesButton*)
{
	accept ();
}

void
TracksControlPanel::accept ()
{
	response(Gtk::RESPONSE_OK);
    
    update_configs();
    hide();
}

void
TracksControlPanel::reject ()
{
	response(Gtk::RESPONSE_CANCEL);
    
    // restore previous value in combo-boxes
    populate_auto_lock_timer_dropdown ();
    populate_auto_save_timer_dropdown ();
    populate_pre_record_buffer_dropdown ();
    
    std::string buffer_size_str = bufsize_as_string (EngineStateController::instance()->get_current_buffer_size() );
    _buffer_size_dropdown.set_current_item_by_data_str (buffer_size_str);
    std::string sample_rate_str = ARDOUR_UI_UTILS::rate_as_string (EngineStateController::instance()->get_current_sample_rate() );
    _sample_rate_dropdown.set_current_item_by_data_str (sample_rate_str);
    show_buffer_duration ();
    
    _default_open_path.set_text(Config->get_default_session_parent_dir());
	display_general_preferences ();

    hide ();
}

void
TracksControlPanel::on_cancel (WavesButton*)
{
    reject ();
}

bool
TracksControlPanel::on_key_press_event (GdkEventKey* ev)
{
    switch (ev->keyval)
    {
        case GDK_Return:
        case GDK_KP_Enter:
            accept ();
            return true;
        case GDK_Escape:
            reject ();
            return true;
    }
    
	return WavesDialog::on_key_press_event (ev);
}

void
TracksControlPanel::on_capture_active_changed(DeviceConnectionControl* capture_control, bool active)
{
    const char * id_name = (char*)capture_control->get_data(DeviceConnectionControl::id_name);
    EngineStateController::instance()->set_physical_audio_input_state(id_name, active);
}

void
TracksControlPanel::on_playback_active_changed(DeviceConnectionControl* playback_control, bool active)
{
    const char * id_name = (char*)playback_control->get_data(DeviceConnectionControl::id_name);
    EngineStateController::instance()->set_physical_audio_output_state(id_name, active);
}

void
TracksControlPanel::on_midi_capture_active_changed(MidiDeviceConnectionControl* control, bool active)
{
    const char * id_name = (char*)control->get_data(MidiDeviceConnectionControl::capture_id_name);
    EngineStateController::instance()->set_physical_midi_input_state(id_name, active);
}

void
TracksControlPanel::on_midi_playback_active_changed(MidiDeviceConnectionControl* control, bool active)
{
    const char * id_name = (char*)control->get_data(MidiDeviceConnectionControl::playback_id_name);
    EngineStateController::instance()->set_physical_midi_output_state(id_name, active);
}


void
TracksControlPanel::on_port_registration_update ()
{
    populate_input_channels ();
    populate_output_channels ();
    populate_midi_ports ();
    populate_mtc_in_dropdown ();
}

void
TracksControlPanel::on_buffer_size_update ()
{
    populate_buffer_size_dropdown();
}

void 
TracksControlPanel::ask_about_sample_rate_mismatch ()
{
	if(ARDOUR_UI::instance()->the_session() == NULL) {
		return;
	}

    if(ARDOUR_UI::instance()->the_session()->frame_rate() == AudioEngine::instance ()->sample_rate ()) {
        return;
    }
    
	set_keep_above (false);
    int result = WavesMessageDialog ("sr_missmatch_dialog.xml",
                                     "",
                                     "",
                                     WavesMessageDialog::BUTTON_YES |
                                     WavesMessageDialog::BUTTON_NO).run ();
	set_keep_above (true);

	bool switch_sr = false;
	switch (result) {
            // button "YES" was pressed
            case WavesDialog::RESPONSE_DEFAULT:
            case RESPONSE_YES:
                switch_sr = true;
                break;
            default:
				// button "NO" was pressed
                break;
        }
    
	if (switch_sr == true) {
		// move session to a new sample rate
		EngineStateController::instance ()->set_new_sample_rate_in_controller (AudioEngine::instance ()->sample_rate ());
		EngineStateController::instance()->push_current_state_to_backend (false);
	} else {
		// switch to None device
		EngineStateController::instance ()->set_new_device_as_current ("None");
		populate_device_dropdown();
	}
}

void
TracksControlPanel::on_device_list_update (bool current_device_disconnected)
{
    populate_device_dropdown();
    
    if (current_device_disconnected) {
        std::string message = _("Audio device has been removed");
        
        set_keep_above (false);
        WavesMessageDialog message_dialog ("", message);
        
        message_dialog.set_position (Gtk::WIN_POS_MOUSE);
        message_dialog.run();
        set_keep_above (true);
        
        return;
    }
}
                                      
void
TracksControlPanel::on_parameter_changed (const std::string& parameter_name)
{
    if (parameter_name == "output-auto-connect") {
        populate_output_mode();
    } else if (parameter_name == "tracks-auto-naming") {
        on_audio_input_configuration_changed ();
    } else if (parameter_name == "default-session-parent-dir") {
        _default_open_path.set_text(Config->get_default_session_parent_dir());
	} else if (parameter_name == "buffering-preset") {
		display_hdd_buffering ();
	} else if (parameter_name == "mmc-control") {
		display_mmc_control ();
	} else if (parameter_name == "send-mmc") {
		display_send_mmc ();
	} else if (parameter_name == "mmc-receive-device-id") {
		display_mmc_receive_device_id ();
	} else if (parameter_name == "mmc-send-device-id") {
		display_mmc_send_device_id ();
	} else if (parameter_name == "denormal-protection") {
		display_denormal_protection ();
	} else if (parameter_name == "history-depth") {
		display_history_depth ();
	} else if (parameter_name == "save-history-depth") {
		display_saved_history_depth ();
    } else if (parameter_name == "ltc-source-port") {
        display_ltc_in_source ();
    } else if (parameter_name == "sync-source") {
        display_current_sync_tool ();
    } else if (parameter_name == "freewheel-timeout-ms") {
        display_freewheel_fader ();
    } else if (parameter_name == "ltc-output-port") {
        display_ltc_output_port ();
    } else if (parameter_name == "latched-record-enable") {
        display_transport_record_locked ();
    } else if (parameter_name == "send-ltc") {
        display_enable_ltc_generator ();
    } else if (parameter_name == "ltc-send-continuously") {
        display_ltc_send_continuously ();
    } else if (parameter_name == "ltc-output-volume") {
        display_ltc_generator_level_fader ();
    }
}

void
TracksControlPanel::on_ui_parameter_changed (const std::string& parameter_name)
{
    if (parameter_name == "waveform-shape") {
        display_waveform_shape ();
    } else if (parameter_name == "meter-hold") {
        display_meter_hold ();
    } else if (parameter_name == "meter-falloff") {
        display_meter_falloff ();
    } else if (parameter_name == "only-copy-imported-files") {
        display_only_copy_imported_files ();
    } else if (parameter_name == "color-track-panel") {
        display_color_track_panel ();
    }
}

void
TracksControlPanel::on_audio_input_configuration_changed ()
{
    std::vector<Gtk::Widget*> capture_controls = _device_capture_list.get_children();
    
    std::vector<Gtk::Widget*>::iterator control_iter = capture_controls.begin();
    
    uint16_t number_count = 1;
    for (; control_iter != capture_controls.end(); ++control_iter) {
        DeviceConnectionControl* control = dynamic_cast<DeviceConnectionControl*> (*control_iter);
        
        if (control) {
            
            const char* id_name = (char*)control->get_data(DeviceConnectionControl::id_name);
            
            if (id_name) {
                bool new_state = EngineStateController::instance()->get_physical_audio_input_state(id_name );
                
                uint16_t number = DeviceConnectionControl::NoNumber;
                std::string track_name ("");
                
                if (new_state) {

                    number = number_count++;
                    
                    if (Config->get_tracks_auto_naming() & UseDefaultNames) {
	                    track_name = string_compose ("%1 %2", Session::default_track_name_pattern (DataType::AUDIO), number);
                    } else if (Config->get_tracks_auto_naming() & NameAfterDriver) {
                        track_name = control->get_port_name();
                    }
                }
                
                control->set_track_name(track_name);
                control->set_number(number);
                control->set_active(new_state);
            }
        }
    }
}

void
TracksControlPanel::on_audio_output_configuration_changed()
{
    std::vector<Gtk::Widget*> playback_controls = _device_playback_list.get_children();
    
    std::vector<Gtk::Widget*>::iterator control_iter = playback_controls.begin();
    
    uint16_t number_count = 1;
    for (; control_iter != playback_controls.end(); ++control_iter) {
        DeviceConnectionControl* control = dynamic_cast<DeviceConnectionControl*> (*control_iter);
        
        if (control) {
            
            const char * id_name = (char*)control->get_data(DeviceConnectionControl::id_name);
            
            if (id_name != NULL) {
                bool new_state = EngineStateController::instance()->get_physical_audio_output_state(id_name );
                
                uint16_t number = DeviceConnectionControl::NoNumber;
                
                if (new_state) {
                    number = number_count++;
                }
                
                control->set_number(number);
                control->set_active(new_state);
            }
        }
    }

}

void
TracksControlPanel::on_midi_input_configuration_changed ()
{
    std::vector<Gtk::Widget*> midi_controls = _midi_device_list.get_children();
    
    std::vector<Gtk::Widget*>::iterator control_iter = midi_controls.begin();
    
    for (; control_iter != midi_controls.end(); ++control_iter) {
        MidiDeviceConnectionControl* control = dynamic_cast<MidiDeviceConnectionControl*> (*control_iter);
        
        if (control && control->has_capture() ) {
            
            const char* capture_id_name = (char*)control->get_data(MidiDeviceConnectionControl::capture_id_name);
            
            if (capture_id_name != NULL) {
                bool connected;
                bool new_state = EngineStateController::instance()->get_physical_midi_input_state(capture_id_name, connected );
                control->set_capture_active(new_state);
            }
        }
    }
    
    populate_mtc_in_dropdown ();
}

void
TracksControlPanel::on_midi_output_configuration_changed ()
{
    std::vector<Gtk::Widget*> midi_controls = _midi_device_list.get_children();
    
    std::vector<Gtk::Widget*>::iterator control_iter = midi_controls.begin();
    
    for (; control_iter != midi_controls.end(); ++control_iter) {
        MidiDeviceConnectionControl* control = dynamic_cast<MidiDeviceConnectionControl*> (*control_iter);
        
        if (control && control->has_playback() ) {
            
            const char* playback_id_name = (char*)control->get_data(MidiDeviceConnectionControl::playback_id_name);
            
            if (playback_id_name != NULL) {
                bool connected;
                bool new_state = EngineStateController::instance()->get_physical_midi_output_state(playback_id_name, connected );
                control->set_playback_active(new_state);
            }
        }
    }
}

void
TracksControlPanel::on_mtc_input_changed (const std::string&)
{
    display_mtc_in_source ();
}

std::string
TracksControlPanel::bufsize_as_string (uint32_t sz)
{
	/* Translators: "samples" is always plural here, so no
	need for plural+singular forms.
	*/
	char  buf[32];
	snprintf (buf, sizeof (buf), _("%u samples"), sz);
	return buf;
}

framecnt_t
TracksControlPanel::get_sample_rate () const
{
    const char* sample_rate = (char*) _sample_rate_dropdown.get_item_data_pv (_sample_rate_dropdown.get_current_item ());
    if (!sample_rate) {
        sample_rate = "";
    }
    return ARDOUR_UI_UTILS::string_as_rate (sample_rate);
}

pframes_t
TracksControlPanel::get_buffer_size() const
{
    const char* bs_text = (char*) _buffer_size_dropdown.get_item_data_pv (_buffer_size_dropdown.get_current_item ());
    if (!bs_text) {
        bs_text = "";
    }
    pframes_t samples = PBD::atoi (bs_text); /* will ignore trailing text */
	return samples;
}

void
TracksControlPanel::show_buffer_duration ()
{
	 float latency = (get_buffer_size() * 1000.0) / get_sample_rate();

	 char buf[256];
	 snprintf (buf, sizeof (buf), _("INPUT LATENCY: %.1f MS      OUTPUT LATENCY: %.1f MS      TOTAL LATENCY: %.1f MS"), 
			   latency, latency, 2*latency);
	 _latency_label.set_text (buf);
}
