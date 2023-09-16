/*
 Copyright (C) 2016 Waves Audio Ltd.
 
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

#include "add_marker_dialog.h"

#include <stdio.h>
#include "i18n.h"

#include "utils.h"
#include "pbd/unwind.h"
#include <gtkmm2ext/utils.h>
#include "dbg_msg.h"

AddMarkerDialog::AddMarkerDialog (std::string& marker_name)
  : WavesDialog (_("add_marker_dialog.xml"), true, false)
  , _cancel_button (get_waves_button ("cancel_button"))
  , _ok_button (get_waves_button ("ok_button"))
  , _marker_name_entry (get_entry("marker_name_entry"))
{
    _cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &AddMarkerDialog::on_cancel_button));
    _ok_button.signal_clicked.connect (sigc::mem_fun (*this, &AddMarkerDialog::on_ok_button));
    _marker_name_entry.set_text(marker_name);
}

void
AddMarkerDialog::on_show ()
{
    WavesDialog::on_show ();
    // Text entry should grab focus when dialog appears
    // to allow user enter track number
    //_tracks_counter_entry.grab_focus ();
    //_tracks_counter_entry.select_region (0, -1);
}

void
AddMarkerDialog::on_cancel_button (WavesButton*)
{
    hide();
	response (Gtk::RESPONSE_CANCEL);
}

void
AddMarkerDialog::on_ok_button (WavesButton*)
{
    hide();
	response (WavesDialog::RESPONSE_DEFAULT);
}
