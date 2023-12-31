/*
    Copyright (C) 2000-2006 Paul Davis

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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <algorithm>

#include <sndfile.h>

#include "pbd/pthread_utils.h"
#include "pbd/basename.h"
#include "pbd/shortpath.h"
#include "pbd/stateful_diff_command.h"

#include <gtkmm2ext/choice.h>

#include "ardour/audio_track.h"
#include "ardour/audiofilesource.h"
#include "ardour/audioregion.h"
#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/operations.h"
#include "ardour/region_factory.h"
#include "ardour/smf_source.h"
#include "ardour/source_factory.h"
#include "ardour/utils.h"
#include "pbd/memento_command.h"

#include "file_sample_rate_mismatch_dialog.h"
#include "ardour_ui.h"
#include "editor.h"
#include "waves_import_dialog.h"
#include "editing.h"
#include "audio_time_axis.h"
#include "midi_time_axis.h"
#include "session_import_dialog.h"
#include "gui_thread.h"
#include "interthread_progress_window.h"
#include "mouse_cursors.h"
#include "editor_cursors.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;
using std::string;

/* Functions supporting the incorporation of external (non-captured) audio material into ardour */

void
Editor::add_external_audio_action (ImportMode mode_hint)
{
	external_audio_dialog ();
}

void
Editor::external_audio_dialog ()
{
	vector<string> paths;
	uint32_t audio_track_cnt;
	uint32_t midi_track_cnt;

	if (_session == 0) {
		WavesMessageDialog msg ("", _("You can't import or embed an audiofile until you have a session loaded."));
		msg.run ();
		return;
	}

	audio_track_cnt = 0;
	midi_track_cnt = 0;

	for (TrackSelection::iterator x = selection->tracks.begin(); x != selection->tracks.end(); ++x) {
		AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*>(*x);

		if (atv) {
			if (atv->is_audio_track()) {
				audio_track_cnt++;
			} 

		} else {
			MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*>(*x);

			if (mtv) {
				if (mtv->is_midi_track()) {
					midi_track_cnt++;
				}
			}
		}
	}

	WavesImportDialog import_dialog (_session, audio_track_cnt);
	import_dialog.run_import ();
}

void
Editor::session_import_dialog ()
{
	SessionImportDialog dialog (_session);
	ensure_float (dialog);
	dialog.run ();
}

typedef std::map<PBD::ID,boost::shared_ptr<ARDOUR::Source> > SourceMap;

/**
 * Updating is still disabled, see note in libs/ardour/import.cc Session::import_files()
 *
 * all_or_nothing:
 *   true  = show "Update", "Import" and "Skip"
 *   false = show "Import", and "Cancel"
 *
 * Returns:
 *     0  To update an existing source of the same name
 *     1  To import/embed the file normally (make sure the new name will be unique)
 *     2  If the user wants to skip this file
 **/
int
Editor::check_whether_and_how_to_import(string path, bool all_or_nothing)
{
	string wave_name (Glib::path_get_basename(path));

	bool already_exists = false;
	uint32_t existing;

	if ((existing = _session->count_sources_by_origin (path)) > 0) {
		already_exists = true;
	}

	if (already_exists)
    {
		string message;
        std::string layout_name;
        
		if (all_or_nothing) {
			// updating is still disabled
			//message = string_compose(_("The session already contains a source file named %1. Do you want to update that file (and thus all regions using the file) or import this file as a new file?"),wave_name);
			layout_name = "waves_how_to_import_dialog_1.xml";
            message = string_compose (_("The session already contains a source file named %1.  \nDo you want to import %1 as a new file, or skip it?"), wave_name);
		} else {
            layout_name = "waves_how_to_import_dialog_2.xml";
			message = string_compose (_("The session already contains a source file named %1.  \nDo you want to import %2 as a new source, or skip it?"), wave_name, wave_name);

		}
        
        WavesMessageDialog msg (layout_name, "", message, WavesMessageDialog::BUTTON_OK | WavesMessageDialog::BUTTON_CANCEL );
        msg.set_position (Gtk::WIN_POS_CENTER);
        
        switch ( msg.run() ) {
            case Gtk::RESPONSE_OK:
                return 1;
                break;
            case Gtk::RESPONSE_CANCEL:
                return 2;
                break;
        }

	}

	return 1;
}

boost::shared_ptr<AudioTrack>
Editor::get_nth_selected_audio_track (int nth) const
{
	AudioTimeAxisView* atv;
	TrackSelection::iterator x;

	for (x = selection->tracks.begin(); nth > 0 && x != selection->tracks.end(); ++x) {

		atv = dynamic_cast<AudioTimeAxisView*>(*x);

		if (!atv) {
			continue;
		} else if (atv->is_audio_track()) {
			--nth;
		}
	}

	if (x == selection->tracks.end()) {
		atv = dynamic_cast<AudioTimeAxisView*>(selection->tracks.back());
	} else {
		atv = dynamic_cast<AudioTimeAxisView*>(*x);
	}

	if (!atv || !atv->is_audio_track()) {
		return boost::shared_ptr<AudioTrack>();
	}

	return atv->audio_track();
}

boost::shared_ptr<MidiTrack>
Editor::get_nth_selected_midi_track (int nth) const
{
	MidiTimeAxisView* mtv;
	TrackSelection::iterator x;

	for (x = selection->tracks.begin(); nth > 0 && x != selection->tracks.end(); ++x) {

		mtv = dynamic_cast<MidiTimeAxisView*>(*x);

		if (!mtv) {
			continue;
		} else if (mtv->is_midi_track()) {
			--nth;
		}
	}

	if (x == selection->tracks.end()) {
		mtv = dynamic_cast<MidiTimeAxisView*>(selection->tracks.back());
	} else {
		mtv = dynamic_cast<MidiTimeAxisView*>(*x);
	}

	if (!mtv || !mtv->is_midi_track()) {
		return boost::shared_ptr<MidiTrack>();
	}

	return mtv->midi_track();
}

void
Editor::do_import (vector<string> paths, ImportDisposition disposition, ImportMode mode, SrcQuality quality, framepos_t& pos)
{
	boost::shared_ptr<Track> track;
	vector<string> to_import;
	int nth = 0;
	bool use_timestamp = (pos == -1);

	current_interthread_info = &import_status;
	import_status.current = 1;
	import_status.total = paths.size ();
	import_status.all_done = false;

	ImportProgressWindow ipw (&import_status);

	bool ok = true;

	if (disposition == Editing::ImportMergeFiles) {

		/* create 1 region from all paths, add to 1 track,
		   ignore "track"
		*/

		bool cancel = false;
		for (vector<string>::iterator a = paths.begin(); a != paths.end(); ++a) {
			int check = check_whether_and_how_to_import(*a, false);
			if (check == 2) {
				cancel = true;
				break;
			}
		}

		if (cancel) {
			ok = false;
		} else {
			ipw.show ();
			ok = (import_sndfiles (paths, disposition, mode, quality, pos, 1, 1, track, false) == 0);
		}

	} else {

		bool replace = false;

		for (vector<string>::iterator a = paths.begin(); a != paths.end(); ++a) {

			const int check = check_whether_and_how_to_import (*a, true);

			switch (check) {
			case 2:
				// user said skip
				continue;
			case 0:
				fatal << "Updating existing sources should be disabled!" << endmsg;
				/* NOTREACHED*/
				break;
			case 1:
				replace = false;
				break;
			default:
				fatal << "Illegal return " << check <<  " from check_whether_and_how_to_import()!" << endmsg;
				/* NOTREACHED*/
			}

			/* have to reset this for every file we handle */

			if (use_timestamp) {
				pos = -1;
			}

			ipw.show ();

			switch (disposition) {
			case Editing::ImportDistinctFiles:

				to_import.clear ();
				to_import.push_back (*a);

				if (mode == Editing::ImportToTrack) {
					track = get_nth_selected_audio_track (nth++);
				}

				ok = (import_sndfiles (to_import, disposition, mode, quality, pos, 1, -1, track, replace) == 0);
				break;

			case Editing::ImportDistinctChannels:

				to_import.clear ();
				to_import.push_back (*a);

				ok = (import_sndfiles (to_import, disposition, mode, quality, pos, -1, -1, track, replace) == 0);
				break;

			case Editing::ImportSerializeFiles:

				to_import.clear ();
				to_import.push_back (*a);

				ok = (import_sndfiles (to_import, disposition, mode, quality, pos, 1, 1, track, replace) == 0);
				break;

			case Editing::ImportMergeFiles:
				// Not entered, handled in earlier if() branch
				break;
			}
		}
	}

	if (ok) {
		_session->save_state ("");
	}

	import_status.all_done = true;
}

void
Editor::do_embed (vector<string> paths, ImportDisposition import_as, ImportMode mode, framepos_t& pos)
{
	boost::shared_ptr<Track> track;
	bool check_sample_rate = true;
	bool ok = false;
	vector<string> to_embed;
	bool multi = paths.size() > 1;
	int nth = 0;
	bool use_timestamp = (pos == -1);

	switch (import_as) {
	case Editing::ImportDistinctFiles:
		for (vector<string>::iterator a = paths.begin(); a != paths.end(); ++a) {

			/* have to reset this for every file we handle */
			if (use_timestamp) {
				pos = -1;
			}

			to_embed.clear ();
			to_embed.push_back (*a);

			if (mode == Editing::ImportToTrack) {
				track = get_nth_selected_audio_track (nth++);
			}

			if (embed_sndfiles (to_embed, multi, check_sample_rate, import_as, mode, pos, 1, -1, track) < -1) {
				goto out;
			}
		}
		break;

	case Editing::ImportDistinctChannels:
		for (vector<string>::iterator a = paths.begin(); a != paths.end(); ++a) {

			/* have to reset this for every file we handle */
			if (use_timestamp) {
				pos = -1;
			}

			to_embed.clear ();
			to_embed.push_back (*a);

			if (embed_sndfiles (to_embed, multi, check_sample_rate, import_as, mode, pos, -1, -1, track) < -1) {
				goto out;
			}
		}
		break;

	case Editing::ImportMergeFiles:
		if (embed_sndfiles (paths, multi, check_sample_rate, import_as, mode, pos, 1, 1, track) < -1) {
			goto out;
		}
		break;

	case Editing::ImportSerializeFiles:
		for (vector<string>::iterator a = paths.begin(); a != paths.end(); ++a) {

			/* have to reset this for every file we handle */
			if (use_timestamp) {
				pos = -1;
			}

			to_embed.clear ();
			to_embed.push_back (*a);

			if (embed_sndfiles (to_embed, multi, check_sample_rate, import_as, mode, pos, 1, 1, track) < -1) {
				goto out;
			}
		}
		break;
	}

	ok = true;

  out:
	if (ok) {
		_session->save_state ("");
	}
}

int
Editor::import_sndfiles (vector<string> paths, ImportDisposition disposition, ImportMode mode, SrcQuality quality, framepos_t& pos,
			 int target_regions, int target_tracks, boost::shared_ptr<Track>& track, bool replace)
{
	import_status.paths = paths;
	import_status.done = false;
	import_status.cancel = false;
	import_status.freeze = false;
	import_status.quality = quality;
	import_status.replace_existing_source = replace;

	import_status.mode = mode;
	import_status.pos = pos;
	import_status.target_tracks = target_tracks;
	import_status.target_regions = target_regions;
	import_status.track = track;
	import_status.replace = replace;

	push_canvas_cursor (_cursors->wait);
	gdk_flush ();

	/* start import thread for this spec. this will ultimately call Session::import_files()
	   which, if successful, will add the files as regions to the region list. its up to us
	   (the GUI) to direct additional steps after that.
	*/

	pthread_create_and_store ("import", &import_status.thread, _import_thread, this);
	pthread_detach (import_status.thread);

	while (!import_status.done && !import_status.cancel) {
		gtk_main_iteration ();
	}

	import_status.done = true;

	int result = -1;

	if (!import_status.cancel && !import_status.sources.empty()) {
		result = add_sources (
			import_status.paths,
			import_status.sources,
			import_status.pos,
			disposition,
			import_status.mode,
			import_status.target_regions,
			import_status.target_tracks,
			track, false
			);

		/* update position from results */

		pos = import_status.pos;
	}

	pop_canvas_cursor ();
	return result;
}

int
Editor::embed_sndfiles (vector<string> paths, bool multifile,
			bool& check_sample_rate, ImportDisposition disposition, ImportMode mode, 
			framepos_t& pos, int target_regions, int target_tracks,
			boost::shared_ptr<Track>& track)
{
	boost::shared_ptr<AudioFileSource> source;
	SourceList sources;
	string linked_path;
	SoundFileInfo finfo;
	int ret = 0;

        push_canvas_cursor (_cursors->wait);
        gdk_flush ();

	for (vector<string>::iterator p = paths.begin(); p != paths.end(); ++p) {

		string path = *p;
		string error_msg;

		/* note that we temporarily truncated _id at the colon */

		if (!AudioFileSource::get_soundfile_info (path, finfo, error_msg)) {
			error << string_compose(_("Editor: cannot open file \"%1\", (%2)"), path, error_msg ) << endmsg;
			goto out;
		}

		if (check_sample_rate  && (finfo.samplerate != (int) _session->frame_rate())) {
			vector<string> choices;

			if (multifile) {
				choices.push_back (_("Cancel entire import"));
				choices.push_back (_("Don't embed it"));
				choices.push_back (_("Embed all without questions"));

				Gtkmm2ext::Choice rate_choice (
					_("Sample rate"),
					string_compose (_("%1\nThis audiofile's sample rate doesn't match the session sample rate!"),
							short_path (path, 40)),
					choices, false
					);

				int resx = rate_choice.run ();

				switch (resx) {
				case 0: /* stop a multi-file import */
					ret = -2;
					goto out;
				case 1: /* don't embed this one */
					ret = -1;
					goto out;
				case 2: /* do it, and the rest without asking */
					check_sample_rate = false;
					break;
				case 3: /* do it */
					break;
				default:
					ret = -2;
					goto out;
				}
			} else {
                
                FileSampleRateMismatchDialog fsrm_dialog(path);
                switch ( fsrm_dialog.run () ) {
                    case Gtk::RESPONSE_CANCEL: /* don't import */
                        ret = -1;
                        goto out;
                        
                    case Gtk::RESPONSE_ACCEPT: /* do it */
                        break;
                    default:
                        ret = -2;
                        goto out;
                }
			}
		}

		for (int n = 0; n < finfo.channels; ++n) {

			try {

				/* check if we have this thing embedded already */

				boost::shared_ptr<Source> s;

				if ((s = _session->audio_source_by_path_and_channel (path, n)) == 0) {

					source = boost::dynamic_pointer_cast<AudioFileSource> (
						SourceFactory::createExternal (DataType::AUDIO, *_session,
									       path, n, 
									       (mode == ImportAsTapeTrack
										? Source::Destructive
										: Source::Flag (0)),
									true, true));
				} else {
					source = boost::dynamic_pointer_cast<AudioFileSource> (s);
				}

				sources.push_back(source);
			}

			catch (failed_constructor& err) {
				error << string_compose(_("could not open %1"), path) << endmsg;
				goto out;
			}

			gtk_main_iteration();
		}
	}

	if (sources.empty()) {
		goto out;
	}


	ret = add_sources (paths, sources, pos, disposition, mode, target_regions, target_tracks, track, true);

  out:
	pop_canvas_cursor ();
	return ret;
}

int
Editor::add_sources (vector<string> paths, SourceList& sources, framepos_t& pos, ImportDisposition disposition, ImportMode mode,
		     int target_regions, int target_tracks, boost::shared_ptr<Track>& track, bool /*add_channel_suffix*/)
{
	vector<boost::shared_ptr<Region> > regions;
	string region_name;
	uint32_t input_chan = 0;
	uint32_t output_chan = 0;
	bool use_timestamp;
	vector<string> track_names;

	use_timestamp = (pos == -1);

	// kludge (for MIDI we're abusing "channel" for "track" here)
	if (SMFSource::safe_midi_file_extension (paths.front())) {
		target_regions = -1;
	}

	if (target_regions == 1) {

		/* take all the sources we have and package them up as a region */

		region_name = region_name_from_path (paths.front(), (sources.size() > 1), false);
        
        int take_number = 1;
        char buf[32];
        snprintf (buf, sizeof(buf), "%d", take_number);
        region_name+='-';
        region_name+=buf;

		/* we checked in import_sndfiles() that there were not too many */

		while (RegionFactory::region_by_name (region_name)) {
            region_name = bump_name_number (region_name);
		}

		PropertyList plist;

		plist.add (ARDOUR::Properties::start, 0);
		plist.add (ARDOUR::Properties::length, sources[0]->length (pos));
		plist.add (ARDOUR::Properties::name, region_name);
		plist.add (ARDOUR::Properties::layer, 0);
		plist.add (ARDOUR::Properties::whole_file, true);
		plist.add (ARDOUR::Properties::external, true);

		boost::shared_ptr<Region> r = RegionFactory::create (sources, plist);

		if (boost::dynamic_pointer_cast<AudioRegion> (r)) {
			boost::dynamic_pointer_cast<AudioRegion> (r)->special_set_position (sources[0]->natural_position ());
        }

		regions.push_back (r);

		/* if we're creating a new track, name it according to the filename
         * e.g. Track1-1.wav (filename) - Track1-1 (track name)
         */
        std::string name_from_path = region_name_from_path (paths.front(), (sources.size() > 1), false);
		track_names.push_back (name_from_path);

	} else if (target_regions == -1 || target_regions > 1) {

		/* take each source and create a region for each one */

		SourceList just_one;
		SourceList::iterator x;
		uint32_t n;

		for (n = 0, x = sources.begin(); x != sources.end(); ++x, ++n) {

			just_one.clear ();
			just_one.push_back (*x);

			boost::shared_ptr<FileSource> fs = boost::dynamic_pointer_cast<FileSource> (*x);

			if (sources.size() > 1 && disposition == ImportDistinctChannels) {

				/* generate a per-channel region name so that things work as
				 * intended
				 */
				
				string path;

				if (fs) {
					region_name = basename_nosuffix (fs->path());
				} else {
					region_name = (*x)->name();
				}
				
				if (sources.size() == 2) {
					if (n == 0) {
						region_name += "-L";
					} else {
						region_name += "-R";
					}
				} else if (sources.size() > 2) {
					region_name += string_compose ("-%1", n+1);
				}
				
				track_names.push_back (region_name);
				
			} else {
				if (fs) {
					region_name = region_name_from_path (fs->path(), false, false, sources.size(), n);
				} else {
					region_name = (*x)->name();
				}

				if (SMFSource::safe_midi_file_extension (paths.front())) {
					string track_name = string_compose ("%1-t%2", PBD::basename_nosuffix (fs->path()), (n + 1));
					track_names.push_back (track_name);
				} else {
					track_names.push_back (PBD::basename_nosuffix (paths[n]));
				}
			}

			PropertyList plist;

			/* Fudge region length to ensure it is non-zero; make it 1 beat at 120bpm
			   for want of a better idea.  It can't be too small, otherwise if this
			   is a MIDI region the conversion from frames -> beats -> frames will
			   round it back down to 0 again.
			*/
			framecnt_t len = (*x)->length (pos);
			if (len == 0) {
				len = (60.0 / 120.0) * _session->frame_rate ();
			}

			plist.add (ARDOUR::Properties::start, 0);
			plist.add (ARDOUR::Properties::length, len);
			plist.add (ARDOUR::Properties::name, region_name);
			plist.add (ARDOUR::Properties::layer, 0);
			plist.add (ARDOUR::Properties::whole_file, true);
			plist.add (ARDOUR::Properties::external, true);

			boost::shared_ptr<Region> r = RegionFactory::create (just_one, plist);

			if (boost::dynamic_pointer_cast<AudioRegion> (r)) {
				boost::dynamic_pointer_cast<AudioRegion> (r)->special_set_position ((*x)->natural_position ());
			}

			regions.push_back (r);
		}
	}

	if (target_regions == 1) {
		input_chan = regions.front()->n_channels();
	} else {
		if (target_tracks == 1) {
			input_chan = regions.size();
		} else {
			input_chan = 1;
		}
	}

	if (Config->get_output_auto_connect() & AutoConnectMaster) {
		output_chan = (_session->master_out() ? _session->master_out()->n_inputs().n_audio() : input_chan);
	} else {
		output_chan = input_chan;
	}

	int n = 0;
	framepos_t rlen = 0;

	begin_reversible_command (Operations::insert_file);

	/* we only use tracks names when importing to new tracks, but we
	 * require that one is defined for every region, just to keep
	 * the API simpler.
	 */
	assert (regions.size() == track_names.size());
	
	for (vector<boost::shared_ptr<Region> >::iterator r = regions.begin(); r != regions.end(); ++r, ++n) {
		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (*r);

		if (use_timestamp) {
                        if (ar) {

                                /* get timestamp for this region */

                                const boost::shared_ptr<Source> s (ar->sources().front());
                                const boost::shared_ptr<AudioSource> as = boost::dynamic_pointer_cast<AudioSource> (s);

                                assert (as);

                                if (as->natural_position() != 0) {
                                        pos = as->natural_position();
                                } else if (target_tracks == 1) {
                                        /* hmm, no timestamp available, put it after the previous region
                                         */
                                        if (n == 0) {
                                                pos = get_preferred_edit_position ();
                                        } else {
                                                pos += rlen;
                                        }
                                } else {
                                        pos = get_preferred_edit_position ();
                                }
                        } else {
                                /* should really get first position in MIDI file, but for now, use edit position*/
                                pos = get_preferred_edit_position ();
                        }
                }
		
		finish_bringing_in_material (*r, input_chan, output_chan, pos, mode, track, track_names[n]);

		rlen = (*r)->length();

		if (target_tracks != 1) {
			track.reset ();
		} else {
			if (!use_timestamp || !ar) {
				/* line each one up right after the other */
				pos += (*r)->length();
			}
		}
	}

	commit_reversible_command ();
	
	/* setup peak file building in another thread */

	for (SourceList::iterator x = sources.begin(); x != sources.end(); ++x) {
		SourceFactory::setup_peakfile (*x, true);
	}

	return 0;
}

int
Editor::finish_bringing_in_material (boost::shared_ptr<Region> region, uint32_t in_chans, uint32_t out_chans, framepos_t& pos,
				     ImportMode mode, boost::shared_ptr<Track>& existing_track, const string& new_track_name)
{
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion>(region);
	boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(region);

	switch (mode) {
	case ImportAsRegion:
		/* relax, its been done */
		break;

	case ImportToTrack:
	{
		if (!existing_track) {

			if (ar) {
				existing_track = get_nth_selected_audio_track (0);
			} else if (mr) {
				existing_track = get_nth_selected_midi_track (0);
			}

			if (!existing_track) {
				return -1;
			}
		}

		boost::shared_ptr<Playlist> playlist = existing_track->playlist();
		boost::shared_ptr<Region> copy (RegionFactory::create (region, region->properties()));
		playlist->clear_changes ();
		playlist->add_region (copy, pos);
		if (Config->get_edit_mode() == Ripple)
			playlist->ripple (pos, copy->length(), copy);

		_session->add_command (new StatefulDiffCommand (playlist));
		break;
	}

	case ImportAsTrack:
	{
		if (!existing_track) {
			if (ar) {
				list<boost::shared_ptr<AudioTrack> > at (_session->new_audio_track (in_chans, out_chans, Normal, 0, 1));

				if (at.empty()) {
					return -1;
				}

				existing_track = at.front();
			} else if (mr) {
				list<boost::shared_ptr<MidiTrack> > mt (_session->new_midi_track (ChanCount (DataType::MIDI, 1),
												  ChanCount (DataType::MIDI, 1),
												  boost::shared_ptr<PluginInfo>(), 
												  Normal, 0, 1));

				if (mt.empty()) {
					return -1;
				}

				existing_track = mt.front();
			}

			if (!new_track_name.empty()) {
				existing_track->set_name (new_track_name);
			} else {
				existing_track->set_name (region->name());
			}
		}

		boost::shared_ptr<Playlist> playlist = existing_track->playlist();
		boost::shared_ptr<Region> copy (RegionFactory::create (region, true));
        // set to copy the same name as original
        copy->set_name (region->name());
		playlist->clear_changes ();
		playlist->add_region (copy, pos);
		_session->add_command (new StatefulDiffCommand (playlist));
		break;
	}

	case ImportAsTapeTrack:
	{
		if (!ar) {
			return -1;
		}

		list<boost::shared_ptr<AudioTrack> > at (_session->new_audio_track (in_chans, out_chans, Destructive));
		if (!at.empty()) {
			boost::shared_ptr<Playlist> playlist = at.front()->playlist();
			boost::shared_ptr<Region> copy (RegionFactory::create (region, true));
			playlist->clear_changes ();
			playlist->add_region (copy, pos);
			_session->add_command (new StatefulDiffCommand (playlist));
		}
		break;
	}
	}

	return 0;
}

void *
Editor::_import_thread (void *arg)
{
	SessionEvent::create_per_thread_pool ("import events", 64);

	Editor *ed = (Editor *) arg;
	return ed->import_thread ();
}

void *
Editor::import_thread ()
{
	_session->import_files (import_status);
	return 0;
}
