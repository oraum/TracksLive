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

#ifndef __WAVES_UI_H__
#define __WAVES_UI_H__

#include <string>
#include <fstream>
#include <map>

#include <boost/algorithm/string.hpp>

#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/layout.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include "gtkmm2ext/fader.h"
#include "gtkmm2ext/focus_entry.h"

#include "pbd/xml++.h"

#include "waves_grid.h"
#include "waves_icon.h"
#include "waves_button.h"
#include "waves_icon_button.h"
#include "waves_dropdown.h"

class WavesUI : public std::map<std::string, Gtk::Object*> {

  public:
	WavesUI (const std::string& layout_script_file, Gtk::Container& root);
	virtual ~WavesUI (); 

	Gtk::Widget& get_widget (const char* id);
	Gtk::Adjustment& get_adjustment (const char* id);
	Gtk::Container& get_container (const char* id);
	Gtk::EventBox& get_event_box (const char* id);
	Gtk::Box& get_box (const char* id);
	Gtk::VBox& get_v_box (const char* id);
	Gtk::HBox& get_h_box (const char* id);
	Gtk::Fixed& get_fixed (const char* id);
	WavesGrid& get_waves_grid (const char* id);
	WavesDropdown& get_waves_dropdown (const char* id);
	Gtk::Paned& get_paned (const char* id);
	Gtk::HPaned& get_h_paned (const char* id);
	Gtk::VPaned& get_v_paned (const char* id);
	Gtk::Table& get_table (const char* id);
	Gtk::Layout& get_layout (const char* id);
	Gtk::Label& get_label (const char* id);
	WavesIcon& get_waves_icon (const char* id);
	Gtk::MenuItem& get_menu_item (const char* id);
	Gtk::RadioMenuItem& get_radio_menu_item (const char* id);
	Gtk::CheckMenuItem& get_check_menu_item (const char* id);
	Gtk::ComboBoxText& get_combo_box_text (const char* id);
	Gtk::CheckButton& get_check_button (const char* id);
	Gtk::Entry& get_entry (const char* id);
	Gtk::Scrollbar& get_scrollbar (const char* id);
	Gtk::HScrollbar& get_h_scrollbar (const char* id);
	Gtk::VScrollbar& get_v_scrollbar (const char* id);
	Gtkmm2ext::FocusEntry& get_focus_entry (const char* id);
    Gtk::SpinButton& get_spin_button (const char* id);
	WavesButton& get_waves_button (const char* id);
	Gtkmm2ext::Fader& get_fader (const char* id);
	const XMLTree* xml_tree () { return _xml_tree; }
	Gtk::Container& root () { return _root_container; }
    Gtk::ProgressBar& get_progressbar (const char* id);
	Gtk::TreeView& get_tree_view (const char* id);
	Gtk::ScrolledWindow& get_scrolled_window (const char* id);

	typedef std::map<std::string, XMLNode*> XMLNodeMap;

	void get_styles(const XMLTree& layout, XMLNodeMap &styles);

	double xml_property (const XMLNode& node, const char* prop_name, const XMLNodeMap& styles, double default_value);
	double xml_property (const XMLNode& node, const char* prop_name, double default_value);
	int32_t xml_property (const XMLNode& node, const char* prop_name, const XMLNodeMap& styles, int32_t default_value);
	int32_t xml_property (const XMLNode& node, const char* prop_name, int32_t default_value);
	uint32_t xml_property (const XMLNode& node, const char* prop_name, const XMLNodeMap& styles, uint32_t default_value);
	uint32_t xml_property (const XMLNode& node, const char* prop_name, uint32_t default_value);

	bool xml_property (const XMLNode& node, const char* prop_name, const XMLNodeMap& styles, bool default_value);
	bool xml_property (const XMLNode& node, const char* prop_name, bool default_value);

	std::string xml_property (const XMLNode& node, const char* prop_name, const XMLNodeMap& styles, const std::string& default_value);
	std::string xml_property (const XMLNode& node, const char* prop_name, const std::string& default_value);
	std::string xml_property (const XMLNode& node, const char* prop_name, const XMLNodeMap& styles, const char* default_value);
	std::string xml_property (const XMLNode& node, const char* prop_name, const char* default_value);
		
	std::string	xml_nodetype(const XMLNode& node);
	std::string	xml_id(const XMLNode& node);
	double xml_x (const XMLNode& node, const XMLNodeMap& styles, double default_value = 0);
	double xml_y (const XMLNode& node, const XMLNodeMap& styles, double default_value = 0);

	Pango::Alignment xml_text_alignment (const XMLNode& node, const XMLNodeMap& styles, Pango::Alignment default_value = Pango::ALIGN_LEFT);
	
  protected:
	void set_attributes (Gtk::Widget& widget, const XMLNode& definition, const XMLNodeMap& styles);

  private:
	static std::map<std::string, const XMLTree*> __xml_tree_cache;
	std::list<Gtk::Object*> _orphan_objects;
	const XMLTree* _xml_tree;
	const std::string _script_file_name;
	Gtk::Container& _root_container;

	Gtk::Object* get_object (const char *id);
	const XMLTree* load_layout (const std::string& xml_file_name);
	void create_ui (const XMLTree& layout, Gtk::Container& root);
	void create_ui (const XMLNodeList& definition, const XMLNodeMap& styles, Gtk::Container& root);
	Gtk::Widget* create_widget (const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (Gtk::Box& parent, const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (Gtk::Fixed& parent, const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (WavesGrid& parent, const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (Gtk::Paned& parent, const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (Gtk::Table& parent, const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (Gtk::ScrolledWindow& parent, const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (Gtk::Window& parent, const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (Gtk::Layout& parent, const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (Gtk::Container& parent, const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (Gtk::EventBox& parent, const XMLNode& definition, const XMLNodeMap& styles);
	void add_dropdown_items (WavesDropdown &dropdown, const XMLNodeList& definition, const XMLNodeMap& styles);

};

#endif //__WAVES_UI_H__
