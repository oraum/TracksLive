/*
    Copyright (C) 2002 Paul Davis

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

#include <unistd.h>
#include <cerrno>
#include <vector>
#include <exception>
#include <stdexcept>
#include <sstream>
#include <cmath>

#include <glibmm/timer.h>
#include <glibmm/pattern.h>
#include <glibmm/module.h>

#include "pbd/epa.h"
#include "pbd/file_utils.h"
#include "pbd/pthread_utils.h"
#include "pbd/stacktrace.h"
#include "pbd/unknown_type.h"
#include "pbd/watchdog_timer.h"

#include "midi++/port.h"
#include "midi++/mmc.h"

#include "ardour/async_midi_port.h"
#include "ardour/audio_port.h"
#include "ardour/audio_backend.h"
#include "ardour/audioengine.h"
#include "ardour/search_paths.h"
#include "ardour/buffer.h"
#include "ardour/butler.h"
#include "ardour/cycle_timer.h"
#include "ardour/internal_send.h"
#include "ardour/meter.h"
#include "ardour/midi_port.h"
#include "ardour/midiport_manager.h"
#include "ardour/mididm.h"
#include "ardour/mtdm.h"
#include "ardour/port.h"
#include "ardour/process_thread.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

AudioEngine* AudioEngine::_instance = 0;

#ifdef SILENCE_AFTER
#define SILENCE_AFTER_SECONDS 600
#endif

static const unsigned int AUDIO_CALLBACK_TIMEOUT_USEC = 2000000;
static const unsigned int DEVICE_ALIVE_CALLBACKS_THRESHOLD = 2;

AudioEngine::AudioEngine ()
	: session_remove_pending (false)
	, session_removal_countdown (-1)
	, _running (false)
	, _freewheeling (false)
	, monitor_check_interval (INT32_MAX)
	, last_monitor_check (0)
	, _processed_frames (0)
	, m_meter_thread (0)
	, _main_thread (0)
	, _mtdm (0)
	, _mididm (0)
	, _measuring_latency (MeasureNone)
	, _latency_input_port (0)
	, _latency_output_port (0)
	, _latency_flush_frames (0)
	, _latency_signal_latency (0)
	, _stopped_for_latency (false)
	, _started_for_latency (false)
	, _in_destructor (false)
    , _hw_reset_event_thread(0)
    , _hw_reset_request_count(0)
    , _callbacks_after_reset_ignore(0)
    , _hw_reset_force_reset(0)
    , _stop_hw_reset_processing(0)
    , _hw_devicelist_update_thread(0)
    , _hw_devicelist_update_count(0)
    , _stop_hw_devicelist_processing(0)
#ifdef SILENCE_AFTER_SECONDS
	, _silence_countdown (0)
	, _silence_hit_cnt (0)
#endif
{
	reset_silence_countdown ();
	start_hw_event_processing();
	discover_backends ();
}

AudioEngine::~AudioEngine ()
{
	_in_destructor = true;
	stop_hw_event_processing();
	drop_backend ();
	for (BackendMap::const_iterator i = _backends.begin(); i != _backends.end(); ++i) {
		i->second->deinstantiate();
	}
}

AudioEngine*
AudioEngine::create ()
{
	if (_instance) {
		return _instance;
	}

	_instance = new AudioEngine ();
	
	return _instance;
}

void
AudioEngine::split_cycle (pframes_t offset)
{
	/* caller must hold process lock */

	Port::increment_global_port_buffer_offset (offset);

	/* tell all Ports that we're going to start a new (split) cycle */

	boost::shared_ptr<Ports> p = ports.reader();

	for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
		i->second->cycle_split ();
	}
}

int
AudioEngine::sample_rate_change (pframes_t nframes)
{
	/* check for monitor input change every 1/10th of second */

	monitor_check_interval = nframes / 10;
	last_monitor_check = 0;

	if (_session) {
		if (_session->frame_rate () != nframes) {
			_session->set_frame_rate (nframes);
		}
	}

	SampleRateChanged (nframes); /* EMIT SIGNAL */

#ifdef SILENCE_AFTER_SECONDS
	_silence_countdown = nframes * SILENCE_AFTER_SECONDS;
#endif
	
	return 0;
}

int 
AudioEngine::buffer_size_change (pframes_t bufsiz)
{
	if (_session) {
		if (_session->get_block_size() != bufsiz) {
            _session->set_block_size (bufsiz);
            last_monitor_check = 0;
        }
	}

	BufferSizeChanged (bufsiz); /* EMIT SIGNAL */

	return 0;
}

/** Method called by our ::process_thread when there is work to be done.
 *  @param nframes Number of frames to process.
 */
#ifdef __clang__
__attribute__((annotate("realtime")))
#endif
int
AudioEngine::process_callback (pframes_t nframes)
{
    if (_device_watch_dog.active () ) {
        g_atomic_int_inc (&_callbacks_after_reset_ignore);
    }
    
	Glib::Threads::Mutex::Lock tm (_process_lock, Glib::Threads::TRY_LOCK);

	PT_TIMING_REF;
	PT_TIMING_CHECK (1);

	/// The number of frames that will have been processed when we've finished
	pframes_t next_processed_frames;

	/* handle wrap around of total frames counter */

	if (max_framepos - _processed_frames < nframes) {
		next_processed_frames = nframes - (max_framepos - _processed_frames);
	} else {
		next_processed_frames = _processed_frames + nframes;
	}

	if (!tm.locked()) {
		/* return having done nothing */
		if (_session) {
			Xrun();
		}
		/* really only JACK requires this
		 * (other backends clear the output buffers
		 * before the process_callback. it may even be 
		 * jack/alsa only). but better safe than sorry.
		 */
		PortManager::silence_outputs (nframes);
		return 0;
	}

	bool return_after_remove_check = false;

	if (_measuring_latency == MeasureAudio && _mtdm) {
		/* run a normal cycle from the perspective of the PortManager
		   so that we get silence on all registered ports.
		   
		   we overwrite the silence on the two ports used for latency
		   measurement.
		*/
		
		PortManager::cycle_start (nframes);
		PortManager::silence (nframes);

		if (_latency_input_port && _latency_output_port) {
			PortEngine& pe (port_engine());

			Sample* in = (Sample*) pe.get_buffer (_latency_input_port, nframes);
			Sample* out = (Sample*) pe.get_buffer (_latency_output_port, nframes);

			_mtdm->process (nframes, in, out);
		}

		PortManager::cycle_end (nframes);
		return_after_remove_check = true;

	} else if (_measuring_latency == MeasureMIDI && _mididm) {
		/* run a normal cycle from the perspective of the PortManager
		   so that we get silence on all registered ports.

		   we overwrite the silence on the two ports used for latency
		   measurement.
		*/

		PortManager::cycle_start (nframes);
		PortManager::silence (nframes);

		if (_latency_input_port && _latency_output_port) {
			PortEngine& pe (port_engine());

			_mididm->process (nframes, pe,
					pe.get_buffer (_latency_input_port, nframes),
					pe.get_buffer (_latency_output_port, nframes));
		}

		PortManager::cycle_end (nframes);
		return_after_remove_check = true;

	} else if (_latency_flush_frames) {
		
		/* wait for the appropriate duration for the MTDM signal to
		 * drain from the ports before we revert to normal behaviour.
		 */

		PortManager::cycle_start (nframes);
		PortManager::silence (nframes);
		PortManager::cycle_end (nframes);
		
                if (_latency_flush_frames > nframes) {
                        _latency_flush_frames -= nframes;
                } else {
                        _latency_flush_frames = 0;
                }

		return_after_remove_check = true;
	}

	if (session_remove_pending) {

		/* perform the actual session removal */

		if (session_removal_countdown < 0) {

			/* fade out over 1 second */
			session_removal_countdown = sample_rate()/2;
			session_removal_gain = GAIN_COEFF_UNITY;
			session_removal_gain_step = 1.0/session_removal_countdown;

		} else if (session_removal_countdown > 0) {

			/* we'll be fading audio out.
			   
			   if this is the last time we do this as part 
			   of session removal, do a MIDI panic now
			   to get MIDI stopped. This relies on the fact
			   that "immediate data" (aka "out of band data") from
			   MIDI tracks is *appended* after any other data, 
			   so that it emerges after any outbound note ons, etc.
			*/

			if (session_removal_countdown <= nframes) {
				_session->midi_panic ();
			}

		} else {
			/* fade out done */
			_session = 0;
			session_removal_countdown = -1; // reset to "not in progress"
			session_remove_pending = false;
			session_removed.signal(); // wakes up thread that initiated session removal
		}
	}

	if (return_after_remove_check) {
		return 0;
	}

	if (_session == 0) {

		if (!_freewheeling) {
			PortManager::cycle_start (nframes);
			PortManager::cycle_end (nframes);
		}

		_processed_frames = next_processed_frames;

		return 0;
	}

	/* tell all relevant objects that we're starting a new cycle */

	InternalSend::CycleStart (nframes);

	/* tell all Ports that we're starting a new cycle */

	PortManager::cycle_start (nframes);

	/* test if we are freewheeling and there are freewheel signals connected.
           ardour should act normally even when freewheeling unless /it/ is
           exporting (which is what Freewheel.empty() tests for).
	*/

	if (_freewheeling && !Freewheel.empty()) {
		Freewheel (nframes);
	} else {
		_session->process (nframes);
	}

	if (_freewheeling) {
		PortManager::cycle_end (nframes);
		return 0;
	}

	if (!_running) {
		_processed_frames = next_processed_frames;
		return 0;
	}

	if (last_monitor_check + monitor_check_interval < next_processed_frames) {
		
		PortManager::check_monitoring ();
		last_monitor_check = next_processed_frames;
	}

#ifdef SILENCE_AFTER_SECONDS

	bool was_silent = (_silence_countdown == 0);
	
	if (_silence_countdown >= nframes) {
		_silence_countdown -= nframes;
	} else {
		_silence_countdown = 0;
	}

	if (!was_silent && _silence_countdown == 0) {
		_silence_hit_cnt++;
		BecameSilent (); /* EMIT SIGNAL */
	}

	if (_silence_countdown == 0 || _session->silent()) {
		PortManager::silence (nframes);
	}
	
#else	
	if (_session->silent()) {
		PortManager::silence (nframes);
	}
#endif
	
	if (session_remove_pending && session_removal_countdown) {

		PortManager::fade_out (session_removal_gain, session_removal_gain_step, nframes);
		
		if (session_removal_countdown > nframes) {
			session_removal_countdown -= nframes;
		} else {
			session_removal_countdown = 0;
		}

		session_removal_gain -= (nframes * session_removal_gain_step);
	}

	PortManager::cycle_end (nframes);

	_processed_frames = next_processed_frames;

	PT_TIMING_CHECK (2);
	
	return 0;
}

void
AudioEngine::reset_silence_countdown ()
{
#ifdef SILENCE_AFTER_SECONDS
	double sr = 48000; /* default in case there is no backend */

	sr = sample_rate();

	_silence_countdown = max (60 * sr, /* 60 seconds */
	                          sr * (SILENCE_AFTER_SECONDS / ::pow (2.0, (double) _silence_hit_cnt)));

#endif
}

void
AudioEngine::launch_device_control_app()
{
	if (_state_lock.trylock () ) {
		_backend->launch_control_app ();
		_state_lock.unlock ();
	}
}


void
AudioEngine::request_backend_reset(bool force/*=false*/)
{
    // make sure it's safe to place a request
    Glib::Threads::Mutex::Lock guard (_reset_request_lock);
    g_atomic_int_inc (&_hw_reset_request_count);
    g_atomic_int_set (&_hw_reset_force_reset, (int)force);
    _hw_reset_condition.signal ();
}

int
AudioEngine::backend_reset_requested()
{
	return g_atomic_int_get (&_hw_reset_request_count);
}

void
AudioEngine::do_reset_backend()
{
	SessionEvent::create_per_thread_pool (X_("Backend reset processing thread"), 1024);
    
	// hold the _reset_request_lock to use _hw_reset_condition
	Glib::Threads::Mutex::Lock guard (_reset_request_lock);
    
	while (!g_atomic_int_get(&_stop_hw_reset_processing) ) {
        
        // hold the _reset_request_lock while we are checkig _hw_reset_request_count
		if (g_atomic_int_get (&_hw_reset_request_count) != 0 && _backend) {
	        
            // let others to place reset requests
			_reset_request_lock.unlock();
	        
			Glib::Threads::RecMutex::Lock pl (_state_lock);
			g_atomic_int_dec_and_test (&_hw_reset_request_count);
	        
            // in normal case reset is forbidden when transport is rolling
            // but there are cases when the reset request must be handled uncoditioanaly
            if (g_atomic_int_get (&_hw_reset_force_reset) ||
                !_session || (_session && !_session->transport_rolling() ) ) {
                
                info << "AudioEngine::RESET::Reset request processing. Requests left: " << _hw_reset_request_count << endmsg;
                
                // notify about device reset to be started
                DeviceResetStarted();

                // backup the device name
                std::string name = _backend->device_name ();

                // PERFORM THE RESET
                info << "AudioEngine::RESET::Reseting device..." << endmsg;
                if ( ( 0 == stop () ) &&
                     ( 0 == _backend->reset_device () ) &&
                     ( 0 == start () ) ) {
                    
                    info << "AudioEngine::RESET::Engine started..." << endmsg;
                    
                    // Device is successfuly started
                    DeviceResetFinished(); // notify about device reset finish
					CheckSampleRateMismatch();
                
                } else { // ERROR DURING RESET
                
                    DeviceResetFinished(); // notify about device reset finish
                    // we've got an error
                    DeviceError("Device cannot operate properly. Switched to None device.");
                }
                
                info << "AudioEngine::RESET::Done." << endmsg;
                
                // drop "force reset" flag, as the reset has been done
                g_atomic_int_set (&_hw_reset_force_reset, 0);
            } else {
                
                // we ignored at least on reset request
                g_atomic_int_set (&_callbacks_after_reset_ignore, 0);
                
                // set up watchdog
                _device_watch_dog.set(AUDIO_CALLBACK_TIMEOUT_USEC, boost::bind(&AudioEngine::device_callback_timeout, this) );

                info << "AudioEngine::RESET::Reset request rejected!!! Reset is not allowed when trasport is rolling. " << endmsg;
            }
            
            // grab back the lock to check _reset_request_lock at the beggining of the loop
			// and use _hw_reset_condition
			_reset_request_lock.lock();
            
		} else {
            
            // wait() call releases _reset_request_lock
            // and grabs it back when the thread is woken up
            // so after wait() returns _reset_request_lock is held by woken thread
			_hw_reset_condition.wait (_reset_request_lock);
            
		}
	}
}

void
AudioEngine::request_device_list_update()
{
    Glib::Threads::Mutex::Lock guard (_devicestate_update_lock);
    g_atomic_int_inc (&_hw_devicelist_update_count);
    _hw_devicestate_update_condition.signal ();
}

void
AudioEngine::request_device_connection_loss_handling ()
{
    Glib::Threads::Mutex::Lock guard (_devicestate_update_lock);
	g_atomic_int_set (&_hw_current_device_connection_loss, 1);
    _hw_devicestate_update_condition.signal ();
}

void
AudioEngine::do_device_state_processing()
{
    SessionEvent::create_per_thread_pool (X_("Device list update processing thread"), 512);
    
    Glib::Threads::Mutex::Lock guard (_devicestate_update_lock);
    while (!_stop_hw_devicelist_processing) {
        
        if (g_atomic_int_get(&_hw_devicelist_update_count) ) {

            _devicestate_update_lock.unlock();
            
            Glib::Threads::RecMutex::Lock pl (_state_lock);
            
            g_atomic_int_dec_and_test (&_hw_devicelist_update_count);
            DeviceListChanged (); /* EMIT SIGNAL */
        
            _devicestate_update_lock.lock();
            
        } else if (g_atomic_int_get(&_hw_current_device_connection_loss) ) {

			_devicestate_update_lock.unlock();

			Glib::Threads::RecMutex::Lock pl (_state_lock);

			g_atomic_int_set (&_hw_current_device_connection_loss, 0);
			// stop watchdog timer
			_device_watch_dog.cancel ();
			DeviceError ("Application has lost sync with your audio device. Switched to None device."); /* EMIT SIGNAL */
			
			_devicestate_update_lock.lock();

		} else {
            _hw_devicestate_update_condition.wait (_devicestate_update_lock);
        }
    }
}

void
AudioEngine::device_callback_timeout ()
{
    if (g_atomic_int_get (&_callbacks_after_reset_ignore) < DEVICE_ALIVE_CALLBACKS_THRESHOLD ) {
        info << "AudioEngine::AudioCallback watchdog timer: forcing device reset " << endmsg;
        request_backend_reset(true);
    }
}

void
AudioEngine::start_hw_event_processing()
{   
    if (_hw_reset_event_thread == 0) {
        g_atomic_int_set(&_hw_reset_request_count, 0);
        g_atomic_int_set(&_stop_hw_reset_processing, 0);
        _hw_reset_event_thread = Glib::Threads::Thread::create (boost::bind (&AudioEngine::do_reset_backend, this));
    }
    
    if (_hw_devicelist_update_thread == 0) {
        g_atomic_int_set(&_hw_devicelist_update_count, 0);
        g_atomic_int_set(&_stop_hw_devicelist_processing, 0);
        _hw_devicelist_update_thread = Glib::Threads::Thread::create (boost::bind (&AudioEngine::do_device_state_processing, this));
    }
}


void
AudioEngine::stop_hw_event_processing()
{
    if (_hw_reset_event_thread) {
        g_atomic_int_set(&_stop_hw_reset_processing, 1);
        g_atomic_int_set(&_hw_reset_request_count, 0);
        _hw_reset_condition.signal ();
        _hw_reset_event_thread->join ();
        _hw_reset_event_thread = 0;
    }
    
    if (_hw_devicelist_update_thread) {
        g_atomic_int_set(&_stop_hw_devicelist_processing, 1);
        g_atomic_int_set(&_hw_devicelist_update_count, 0);
        _hw_devicestate_update_condition.signal ();
        _hw_devicelist_update_thread->join ();
        _hw_devicelist_update_thread = 0;
    }
	
}


void
AudioEngine::set_session (Session *s)
{
	Glib::Threads::Mutex::Lock pl (_process_lock);

	SessionHandlePtr::set_session (s);

	if (_session) {
		pframes_t blocksize = samples_per_cycle ();
		if (_session->get_block_size() != blocksize) {
            _session->set_block_size (blocksize);
            last_monitor_check = 0;
        }

		PortManager::cycle_start (blocksize);

		_session->process (blocksize);
		_session->process (blocksize);
		_session->process (blocksize);
		_session->process (blocksize);
		_session->process (blocksize);
		_session->process (blocksize);
		_session->process (blocksize);
		_session->process (blocksize);

		PortManager::cycle_end (blocksize);
	}
}

void
AudioEngine::remove_session ()
{
	Glib::Threads::Mutex::Lock lm (_process_lock);

	if (_running) {

		if (_session) {
			session_remove_pending = true;
			session_removal_countdown = 0;
			session_removed.wait(_process_lock);
		}

	} else {
		SessionHandlePtr::set_session (0);
	}

	remove_all_ports ();
}


void
AudioEngine::reconnect_session_routes (bool reconnect_inputs, bool reconnect_outputs)
{
#ifdef USE_TRACKS_CODE_FEATURES
	if (_session) {
		_session->reconnect_existing_routes(true, true, reconnect_inputs, reconnect_outputs);
	}
#endif	
}


void
AudioEngine::died ()
{
	/* called from a signal handler for SIGPIPE */
	_running = false;
}

int
AudioEngine::reset_timebase ()
{
	if (_session) {
		if (_session->config.get_jack_time_master()) {
			_backend->set_time_master (true);
		} else {
			_backend->set_time_master (false);
		}
	}
	return 0;
}


void
AudioEngine::destroy ()
{
	delete _instance;
	_instance = 0;
}

int
AudioEngine::discover_backends ()
{
	vector<std::string> backend_modules;

	_backends.clear ();

	Glib::PatternSpec so_extension_pattern("*backend.so");
	Glib::PatternSpec dylib_extension_pattern("*backend.dylib");

#if defined(PLATFORM_WINDOWS) && defined(DEBUGGABLE_BACKENDS)
	#if defined(DEBUG) || defined(_DEBUG)
		Glib::PatternSpec dll_extension_pattern("*backendD.dll");
	#else
		Glib::PatternSpec dll_extension_pattern("*backendRDC.dll");
	#endif
#else
	Glib::PatternSpec dll_extension_pattern("*backend.dll");
#endif

	find_files_matching_pattern (backend_modules, backend_search_path (),
	                             so_extension_pattern);

	find_files_matching_pattern (backend_modules, backend_search_path (),
	                             dylib_extension_pattern);

	find_files_matching_pattern (backend_modules, backend_search_path (),
	                             dll_extension_pattern);

	DEBUG_TRACE (DEBUG::AudioEngine, string_compose ("looking for backends in %1\n", backend_search_path().to_string()));

	for (vector<std::string>::iterator i = backend_modules.begin(); i != backend_modules.end(); ++i) {

		AudioBackendInfo* info;

		DEBUG_TRACE (DEBUG::AudioEngine, string_compose ("Checking possible backend in %1\n", *i));

		if ((info = backend_discover (*i)) != 0) {
			_backends.insert (make_pair (info->name, info));
		}
	}

	DEBUG_TRACE (DEBUG::AudioEngine, string_compose ("Found %1 backends\n", _backends.size()));

	return _backends.size();
}

AudioBackendInfo*
AudioEngine::backend_discover (const string& path)
{
#ifdef PLATFORM_WINDOWS
	// do not show popup dialog (e.g. missing libjack.dll)
	// win7+ should use SetThreadErrorMode()
	SetErrorMode(SEM_FAILCRITICALERRORS);
#endif
	Glib::Module module (path);
#ifdef PLATFORM_WINDOWS
	SetErrorMode(0); // reset to system default
#endif
	AudioBackendInfo* info;
	AudioBackendInfo* (*dfunc)(void);
	void* func = 0;

	if (!module) {
		error << string_compose(_("AudioEngine: cannot load module \"%1\" (%2)"), path,
					Glib::Module::get_last_error()) << endmsg;
		return 0;
	}
	
	if (!module.get_symbol ("descriptor", func)) {
		error << string_compose(_("AudioEngine: backend at \"%1\" has no descriptor function."), path) << endmsg;
		error << Glib::Module::get_last_error() << endmsg;
		return 0;
	}
	
	dfunc = (AudioBackendInfo* (*)(void))func;
	info = dfunc();
	if (!info->available()) {
		return 0;
	}

	module.make_resident ();
	
	return info;
}

vector<const AudioBackendInfo*>
AudioEngine::available_backends() const
{
	vector<const AudioBackendInfo*> r;
	
	for (BackendMap::const_iterator i = _backends.begin(); i != _backends.end(); ++i) {
		r.push_back (i->second);
	}

	return r;
}

string
AudioEngine::current_backend_name() const
{
	if (_backend) {
		return _backend->name();
	} 
	return string();
}

void
AudioEngine::drop_backend ()
{
	if (_backend) {
		_backend->stop ();
		_backend->drop_device ();
		_backend.reset ();
		_running = false;
	}
}

boost::shared_ptr<AudioBackend>
AudioEngine::set_default_backend ()
{
	if (_backends.empty()) {
		return boost::shared_ptr<AudioBackend>();
	}

	return set_backend (_backends.begin()->first, "", "");
}

boost::shared_ptr<AudioBackend>
AudioEngine::set_backend (const std::string& name, const std::string& arg1, const std::string& arg2)
{
	BackendMap::iterator b = _backends.find (name);

	if (b == _backends.end()) {
		return boost::shared_ptr<AudioBackend>();
	}

	drop_backend ();
	
	try {
		if (b->second->instantiate (arg1, arg2)) {
			throw failed_constructor ();
		}
		
		_backend = b->second->factory (*this);

	} catch (exception& e) {
		error << string_compose (_("Could not create backend for %1: %2"), name, e.what()) << endmsg;
		return boost::shared_ptr<AudioBackend>();
	}

	return _backend;
}

/* BACKEND PROXY WRAPPERS */

int
AudioEngine::start (bool for_latency)
{
	if (!_backend) {
		return -1;
	}

	if (_running) {
		return 0;
	}

	_processed_frames = 0;
	last_monitor_check = 0;
    
    g_atomic_int_set (&_callbacks_after_reset_ignore, 0);
    
	if (_backend->start (for_latency)) {
		return -1;
	}

	_running = true;
    
	if (_session) {
		if (_session->config.get_jack_time_master()) {
			_backend->set_time_master (true);
		}
	}
	
	if (!for_latency) {
		Running(); /* EMIT SIGNAL */
	}
	
	return 0;
}

int
AudioEngine::stop (bool for_latency)
{
	if (!_backend) {
		return 0;
	}

    // cancel watchdog timer
    _device_watch_dog.cancel ();
    
	if (_backend->stop ()) {
		return -1;
	}
	
    g_atomic_int_set (&_callbacks_after_reset_ignore, 0);
    
	if (_session && _running &&
	    (_session->state_of_the_state() & Session::Loading) == 0 &&
	    (_session->state_of_the_state() & Session::Deletion) == 0) {
		// it's not a halt, but should be handled the same way:
		// disable record, stop transport and I/O processign but save the data.
		_session->engine_halted ();
	}

	_running = false;
	_processed_frames = 0;
	_measuring_latency = MeasureNone;
	_latency_output_port = 0;
	_latency_input_port = 0;
	_started_for_latency = false;
	
	Port::PortDrop ();

	if (!for_latency) {
		Stopped (); /* EMIT SIGNAL */
	}
	
	return 0;
}

int
AudioEngine::freewheel (bool start_stop)
{
	if (!_backend) {
		return -1;
	}

	/* _freewheeling will be set when first Freewheel signal occurs */

	return _backend->freewheel (start_stop);
}

float
AudioEngine::get_dsp_load() const 
{
	if (!_backend) {
		return 0.0;
	}
	return _backend->dsp_load ();
}

bool
AudioEngine::is_realtime() const 
{
	if (!_backend) {
		return false;
	}

	return _backend->is_realtime();
}

bool
AudioEngine::connected() const 
{
	if (!_backend) {
		return false;
	}

	return _backend->available();
}

void
AudioEngine::transport_start ()
{
	if (!_backend) {
		return;
	}
	return _backend->transport_start ();
}

void
AudioEngine::transport_stop ()
{
	if (!_backend) {
		return;
	}
	return _backend->transport_stop ();
}

TransportState
AudioEngine::transport_state ()
{
	if (!_backend) {
		return TransportStopped;
	}
	return _backend->transport_state ();
}

void
AudioEngine::transport_locate (framepos_t pos)
{
	if (!_backend) {
		return;
	}
	return _backend->transport_locate (pos);
}

framepos_t
AudioEngine::transport_frame()
{
	if (!_backend) {
		return 0;
	}
	return _backend->transport_frame ();
}

framecnt_t
AudioEngine::sample_rate () const
{
	if (!_backend) {
		return 0;
	}
	return _backend->sample_rate ();
}

pframes_t
AudioEngine::samples_per_cycle () const
{
	if (!_backend) {
		return 0;
	}
	return _backend->buffer_size ();
}

int
AudioEngine::usecs_per_cycle () const
{
	if (!_backend) {
		return -1;
	}
	return _backend->usecs_per_cycle ();
}

size_t
AudioEngine::raw_buffer_size (DataType t)
{
	if (!_backend) {
		return -1;
	}
	return _backend->raw_buffer_size (t);
}

framepos_t
AudioEngine::sample_time ()
{
	if (!_backend) {
		return 0;
	}
	return _backend->sample_time ();
}

framepos_t
AudioEngine::sample_time_at_cycle_start ()
{
	if (!_backend) {
		return 0;
	}
	return _backend->sample_time_at_cycle_start ();
}

pframes_t
AudioEngine::samples_since_cycle_start ()
{
	if (!_backend) {
		return 0;
	}
	return _backend->samples_since_cycle_start ();
}

bool
AudioEngine::get_sync_offset (pframes_t& offset) const
{
	if (!_backend) {
		return false;
	}
	return _backend->get_sync_offset (offset);
}

int
AudioEngine::create_process_thread (boost::function<void()> func)
{
	if (!_backend) {
		return -1;
	}
	return _backend->create_process_thread (func);
}

int
AudioEngine::join_process_threads ()
{
	if (!_backend) {
		return -1;
	}
	return _backend->join_process_threads ();
}

bool
AudioEngine::in_process_thread ()
{
	if (!_backend) {
		return false;
	}
	return _backend->in_process_thread ();
}

uint32_t
AudioEngine::process_thread_count ()
{
	if (!_backend) {
		return 0;
	}
	return _backend->process_thread_count ();
}

int
AudioEngine::set_device_name (const std::string& name)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_device_name  (name);
}

int
AudioEngine::set_sample_rate (float sr)
{
	if (!_backend) {
		return -1;
	}

	return _backend->set_sample_rate  (sr);
}

int
AudioEngine::set_buffer_size (uint32_t bufsiz)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_buffer_size  (bufsiz);
}

int
AudioEngine::set_interleaved (bool yn)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_interleaved  (yn);
}

int
AudioEngine::set_input_channels (uint32_t ic)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_input_channels  (ic);
}

int
AudioEngine::set_output_channels (uint32_t oc)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_output_channels (oc);
}

int
AudioEngine::set_systemic_input_latency (uint32_t il)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_systemic_input_latency  (il);
}

int
AudioEngine::set_systemic_output_latency (uint32_t ol)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_systemic_output_latency  (ol);
}

bool
AudioEngine::thread_initialised_for_audio_processing ()
{
    return SessionEvent::has_per_thread_pool () && AsyncMIDIPort::is_process_thread();
}

/* END OF BACKEND PROXY API */

void
AudioEngine::thread_init_callback (void* arg)
{
	/* make sure that anybody who needs to know about this thread
	   knows about it.
	*/

	pthread_set_name (X_("audioengine"));

	SessionEvent::create_per_thread_pool (X_("AudioEngine"), 512);

	PBD::notify_gui_about_thread_creation ("gui", pthread_self(), X_("AudioEngine"), 4096);
	PBD::notify_gui_about_thread_creation ("midiui", pthread_self(), X_("AudioEngine"), 128);

	AsyncMIDIPort::set_process_thread (pthread_self());

	if (arg) {
		/* the special thread created/managed by the backend */
		AudioEngine::instance()->_main_thread = new ProcessThread;
	}
}

int
AudioEngine::sync_callback (TransportState state, framepos_t position)
{
	if (_session) {
		return _session->backend_sync_callback (state, position);
	}
	return 0;
}

void
AudioEngine::freewheel_callback (bool onoff)
{
	_freewheeling = onoff;
}

void
AudioEngine::latency_callback (bool for_playback)
{
        if (_session) {
                _session->update_latency (for_playback);
        }
}

void
AudioEngine::update_latencies ()
{
	if (_backend) {
		_backend->update_latencies ();
	}
}

void
AudioEngine::halted_callback (const char* why)
{
	if (_in_destructor) {
		/* everything is under control */
		return;
	}

	_running = false;

	Port::PortDrop (); /* EMIT SIGNAL */

	if (!_started_for_latency) {
		Halted (why);      /* EMIT SIGNAL */
	}
}

bool
AudioEngine::setup_required () const
{
	if (_backend) {
		if (_backend->info().already_configured())
			return false;
	} else {
		if (_backends.size() == 1 && _backends.begin()->second->already_configured()) {
			return false;
		}
	}
	
	return true;
}

int
AudioEngine::prepare_for_latency_measurement ()
{
	if (running()) {
		_stopped_for_latency = true;
		stop (true);
	}

	if (start (true)) {
		_started_for_latency = true;
		return -1;
	}

	return 0;
}

int
AudioEngine::start_latency_detection (bool for_midi)
{
	if (!running()) {
		if (prepare_for_latency_measurement ()) {
			return -1;
		}
	}

	PortEngine& pe (port_engine());

	delete _mtdm;
	_mtdm = 0;

	delete _mididm;
	_mididm = 0;

	/* find the ports we will connect to */

	PortEngine::PortHandle out = pe.get_port_by_name (_latency_output_name);
	PortEngine::PortHandle in = pe.get_port_by_name (_latency_input_name);

	if (!out || !in) {
		stop (true);
		return -1;
	}

	/* create the ports we will use to read/write data */
	if (for_midi) {
		if ((_latency_output_port = pe.register_port ("latency_out", DataType::MIDI, IsOutput)) == 0) {
			stop (true);
			return -1;
		}
		if (pe.connect (_latency_output_port, _latency_output_name)) {
			pe.unregister_port (_latency_output_port);
			stop (true);
			return -1;
		}

		const string portname ("latency_in");
		if ((_latency_input_port = pe.register_port (portname, DataType::MIDI, IsInput)) == 0) {
			pe.unregister_port (_latency_input_port);
			pe.unregister_port (_latency_output_port);
			stop (true);
			return -1;
		}
		if (pe.connect (_latency_input_name, make_port_name_non_relative (portname))) {
			pe.unregister_port (_latency_input_port);
			pe.unregister_port (_latency_output_port);
			stop (true);
			return -1;
		}

		_mididm = new MIDIDM (sample_rate());

	} else {

		if ((_latency_output_port = pe.register_port ("latency_out", DataType::AUDIO, IsOutput)) == 0) {
			stop (true);
			return -1;
		}
		if (pe.connect (_latency_output_port, _latency_output_name)) {
			pe.unregister_port (_latency_output_port);
			stop (true);
			return -1;
		}

		const string portname ("latency_in");
		if ((_latency_input_port = pe.register_port (portname, DataType::AUDIO, IsInput)) == 0) {
			pe.unregister_port (_latency_input_port);
			pe.unregister_port (_latency_output_port);
			stop (true);
			return -1;
		}
		if (pe.connect (_latency_input_name, make_port_name_non_relative (portname))) {
			pe.unregister_port (_latency_input_port);
			pe.unregister_port (_latency_output_port);
			stop (true);
			return -1;
		}

		_mtdm = new MTDM (sample_rate());

	}

	LatencyRange lr;
	_latency_signal_latency = 0;
	lr = pe.get_latency_range (in, false);
	_latency_signal_latency = lr.max;
	lr = pe.get_latency_range (out, true);
	_latency_signal_latency += lr.max;

	/* all created and connected, lets go */
	_latency_flush_frames = samples_per_cycle();
	_measuring_latency = for_midi ? MeasureMIDI : MeasureAudio;

	return 0;
}

void
AudioEngine::stop_latency_detection ()
{
	_measuring_latency = MeasureNone;

	if (_latency_output_port) {
		port_engine().unregister_port (_latency_output_port);
		_latency_output_port = 0;
	}
	if (_latency_input_port) {
		port_engine().unregister_port (_latency_input_port);
		_latency_input_port = 0;
	}

	stop (true);

	if (_stopped_for_latency) {
		start ();
	}

	_stopped_for_latency = false;
	_started_for_latency = false;
}

void
AudioEngine::set_latency_output_port (const string& name)
{
	_latency_output_name = name;
}

void
AudioEngine::set_latency_input_port (const string& name)
{
	_latency_input_name = name;
}
