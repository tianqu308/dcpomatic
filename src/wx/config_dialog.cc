/*
    Copyright (C) 2012-2017 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

/** @file src/config_dialog.cc
 *  @brief A dialogue to edit DCP-o-matic configuration.
 */

#include "config_dialog.h"
#include "wx_util.h"
#include "editable_list.h"
#include "filter_dialog.h"
#include "dir_picker_ctrl.h"
#include "file_picker_ctrl.h"
#include "isdcf_metadata_dialog.h"
#include "server_dialog.h"
#include "make_chain_dialog.h"
#include "email_dialog.h"
#include "name_format_editor.h"
#include "nag_dialog.h"
#include "lib/config.h"
#include "lib/ratio.h"
#include "lib/filter.h"
#include "lib/dcp_content_type.h"
#include "lib/log.h"
#include "lib/util.h"
#include "lib/cross.h"
#include "lib/exceptions.h"
#include <dcp/locale_convert.h>
#include <dcp/exceptions.h>
#include <dcp/certificate_chain.h>
#include <wx/stdpaths.h>
#include <wx/preferences.h>
#include <wx/spinctrl.h>
#include <wx/filepicker.h>
#include <RtAudio.h>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <iostream>

using std::vector;
using std::string;
using std::list;
using std::cout;
using std::pair;
using std::make_pair;
using std::map;
using boost::bind;
using boost::shared_ptr;
using boost::function;
using boost::optional;
using dcp::locale_convert;

static
void
do_nothing ()
{

}

class Page
{
public:
	Page (wxSize panel_size, int border)
		: _border (border)
		, _panel (0)
		, _panel_size (panel_size)
		, _window_exists (false)
	{
		_config_connection = Config::instance()->Changed.connect (boost::bind (&Page::config_changed_wrapper, this));
	}

	virtual ~Page () {}

protected:
	wxWindow* create_window (wxWindow* parent)
	{
		_panel = new wxPanel (parent, wxID_ANY, wxDefaultPosition, _panel_size);
		wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
		_panel->SetSizer (s);

		setup ();
		_window_exists = true;
		config_changed ();

		_panel->Bind (wxEVT_DESTROY, boost::bind (&Page::window_destroyed, this));

		return _panel;
	}

	int _border;
	wxPanel* _panel;

private:
	virtual void config_changed () = 0;
	virtual void setup () = 0;

	void config_changed_wrapper ()
	{
		if (_window_exists) {
			config_changed ();
		}
	}

	void window_destroyed ()
	{
		_window_exists = false;
	}

	wxSize _panel_size;
	boost::signals2::scoped_connection _config_connection;
	bool _window_exists;
};

class StockPage : public wxStockPreferencesPage, public Page
{
public:
	StockPage (Kind kind, wxSize panel_size, int border)
		: wxStockPreferencesPage (kind)
		, Page (panel_size, border)
	{}

	wxWindow* CreateWindow (wxWindow* parent)
	{
		return create_window (parent);
	}
};

class StandardPage : public wxPreferencesPage, public Page
{
public:
	StandardPage (wxSize panel_size, int border)
		: Page (panel_size, border)
	{}

	wxWindow* CreateWindow (wxWindow* parent)
	{
		return create_window (parent);
	}
};

class GeneralPage : public StockPage
{
public:
	GeneralPage (wxSize panel_size, int border)
		: StockPage (Kind_General, panel_size, border)
	{}

private:
	void setup ()
	{
		wxGridBagSizer* table = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

		int r = 0;
		_set_language = new wxCheckBox (_panel, wxID_ANY, _("Set language"));
		table->Add (_set_language, wxGBPosition (r, 0));
		_language = new wxChoice (_panel, wxID_ANY);
		vector<pair<string, string> > languages;
		languages.push_back (make_pair ("Čeština", "cs_CZ"));
		languages.push_back (make_pair ("汉语/漢語", "zh_CN"));
		languages.push_back (make_pair ("Dansk", "da_DK"));
		languages.push_back (make_pair ("Deutsch", "de_DE"));
		languages.push_back (make_pair ("English", "en_GB"));
		languages.push_back (make_pair ("Español", "es_ES"));
		languages.push_back (make_pair ("Français", "fr_FR"));
		languages.push_back (make_pair ("Italiano", "it_IT"));
		languages.push_back (make_pair ("Nederlands", "nl_NL"));
		languages.push_back (make_pair ("Русский", "ru_RU"));
		languages.push_back (make_pair ("Polski", "pl_PL"));
		languages.push_back (make_pair ("Português europeu", "pt_PT"));
		languages.push_back (make_pair ("Português do Brasil", "pt_BR"));
		languages.push_back (make_pair ("Svenska", "sv_SE"));
		languages.push_back (make_pair ("Slovenský jazyk", "sk_SK"));
		languages.push_back (make_pair ("українська мова", "uk_UA"));
		checked_set (_language, languages);
		table->Add (_language, wxGBPosition (r, 1));
		++r;

		wxStaticText* restart = add_label_to_sizer (
			table, _panel, _("(restart DCP-o-matic to see language changes)"), false, wxGBPosition (r, 0), wxGBSpan (1, 2)
			);
		wxFont font = restart->GetFont();
		font.SetStyle (wxFONTSTYLE_ITALIC);
		font.SetPointSize (font.GetPointSize() - 1);
		restart->SetFont (font);
		++r;

		add_label_to_sizer (table, _panel, _("Number of threads DCP-o-matic should use"), true, wxGBPosition (r, 0));
		_master_encoding_threads = new wxSpinCtrl (_panel);
		table->Add (_master_encoding_threads, wxGBPosition (r, 1));
		++r;

		add_label_to_sizer (table, _panel, _("Number of threads DCP-o-matic encode server should use"), true, wxGBPosition (r, 0));
		_server_encoding_threads = new wxSpinCtrl (_panel);
		table->Add (_server_encoding_threads, wxGBPosition (r, 1));
		++r;

		add_label_to_sizer (table, _panel, _("Cinema and screen database file"), true, wxGBPosition (r, 0));
		_cinemas_file = new FilePickerCtrl (_panel, _("Select cinema and screen database file"), "*.xml", true);
		table->Add (_cinemas_file, wxGBPosition (r, 1));
		++r;

		_preview_sound = new wxCheckBox (_panel, wxID_ANY, _("Play sound in the preview via"));
		table->Add (_preview_sound, wxGBPosition (r, 0));
                _preview_sound_output = new wxChoice (_panel, wxID_ANY);
                table->Add (_preview_sound_output, wxGBPosition (r, 1));
                ++r;

#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
		_analyse_ebur128 = new wxCheckBox (_panel, wxID_ANY, _("Find integrated loudness, true peak and loudness range when analysing audio"));
		table->Add (_analyse_ebur128, wxGBPosition (r, 0), wxGBSpan (1, 2));
		++r;
#endif

		_automatic_audio_analysis = new wxCheckBox (_panel, wxID_ANY, _("Automatically analyse content audio"));
		table->Add (_automatic_audio_analysis, wxGBPosition (r, 0), wxGBSpan (1, 2));
		++r;

		_check_for_updates = new wxCheckBox (_panel, wxID_ANY, _("Check for updates on startup"));
		table->Add (_check_for_updates, wxGBPosition (r, 0), wxGBSpan (1, 2));
		++r;

		_check_for_test_updates = new wxCheckBox (_panel, wxID_ANY, _("Check for testing updates on startup"));
		table->Add (_check_for_test_updates, wxGBPosition (r, 0), wxGBSpan (1, 2));
		++r;

		wxFlexGridSizer* bottom_table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		bottom_table->AddGrowableCol (1, 1);

		add_label_to_sizer (bottom_table, _panel, _("Issuer"), true);
		_issuer = new wxTextCtrl (_panel, wxID_ANY);
		bottom_table->Add (_issuer, 1, wxALL | wxEXPAND);

		add_label_to_sizer (bottom_table, _panel, _("Creator"), true);
		_creator = new wxTextCtrl (_panel, wxID_ANY);
		bottom_table->Add (_creator, 1, wxALL | wxEXPAND);

		table->Add (bottom_table, wxGBPosition (r, 0), wxGBSpan (2, 2), wxEXPAND);
		++r;

                RtAudio audio (DCPOMATIC_RTAUDIO_API);
                for (unsigned int i = 0; i < audio.getDeviceCount(); ++i) {
                        RtAudio::DeviceInfo dev = audio.getDeviceInfo (i);
                        if (dev.probed && dev.outputChannels > 0) {
                                _preview_sound_output->Append (std_to_wx (dev.name));
                        }
                }

		_set_language->Bind         (wxEVT_CHECKBOX,           boost::bind (&GeneralPage::set_language_changed,  this));
		_language->Bind             (wxEVT_CHOICE,             boost::bind (&GeneralPage::language_changed,      this));
		_cinemas_file->Bind         (wxEVT_FILEPICKER_CHANGED, boost::bind (&GeneralPage::cinemas_file_changed,  this));
		_preview_sound->Bind        (wxEVT_CHECKBOX,           boost::bind (&GeneralPage::preview_sound_changed, this));
		_preview_sound_output->Bind (wxEVT_CHOICE,             boost::bind (&GeneralPage::preview_sound_output_changed, this));

		_master_encoding_threads->SetRange (1, 128);
		_master_encoding_threads->Bind (wxEVT_SPINCTRL, boost::bind (&GeneralPage::master_encoding_threads_changed, this));
		_server_encoding_threads->SetRange (1, 128);
		_server_encoding_threads->Bind (wxEVT_SPINCTRL, boost::bind (&GeneralPage::server_encoding_threads_changed, this));

#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
		_analyse_ebur128->Bind (wxEVT_CHECKBOX, boost::bind (&GeneralPage::analyse_ebur128_changed, this));
#endif
		_automatic_audio_analysis->Bind (wxEVT_CHECKBOX, boost::bind (&GeneralPage::automatic_audio_analysis_changed, this));
		_check_for_updates->Bind (wxEVT_CHECKBOX, boost::bind (&GeneralPage::check_for_updates_changed, this));
		_check_for_test_updates->Bind (wxEVT_CHECKBOX, boost::bind (&GeneralPage::check_for_test_updates_changed, this));

		_issuer->Bind (wxEVT_TEXT, boost::bind (&GeneralPage::issuer_changed, this));
		_creator->Bind (wxEVT_TEXT, boost::bind (&GeneralPage::creator_changed, this));
	}

	void config_changed ()
	{
		Config* config = Config::instance ();

		checked_set (_set_language, static_cast<bool>(config->language()));

		/* Backwards compatibility of config file */

		map<string, string> compat_map;
		compat_map["fr"] = "fr_FR";
		compat_map["it"] = "it_IT";
		compat_map["es"] = "es_ES";
		compat_map["sv"] = "sv_SE";
		compat_map["de"] = "de_DE";
		compat_map["nl"] = "nl_NL";
		compat_map["ru"] = "ru_RU";
		compat_map["pl"] = "pl_PL";
		compat_map["da"] = "da_DK";
		compat_map["pt"] = "pt_PT";
		compat_map["sk"] = "sk_SK";
		compat_map["cs"] = "cs_CZ";
		compat_map["uk"] = "uk_UA";

		string lang = config->language().get_value_or ("en_GB");
		if (compat_map.find (lang) != compat_map.end ()) {
			lang = compat_map[lang];
		}

		checked_set (_language, lang);

		checked_set (_master_encoding_threads, config->master_encoding_threads ());
		checked_set (_server_encoding_threads, config->server_encoding_threads ());
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
		checked_set (_analyse_ebur128, config->analyse_ebur128 ());
#endif
		checked_set (_automatic_audio_analysis, config->automatic_audio_analysis ());
		checked_set (_check_for_updates, config->check_for_updates ());
		checked_set (_check_for_test_updates, config->check_for_test_updates ());
		checked_set (_issuer, config->dcp_issuer ());
		checked_set (_creator, config->dcp_creator ());
		checked_set (_cinemas_file, config->cinemas_file());
		checked_set (_preview_sound, config->preview_sound());

                optional<string> const current_so = get_preview_sound_output ();
                optional<string> configured_so;

                if (config->preview_sound_output()) {
                        configured_so = config->preview_sound_output().get();
                } else {
                        /* No configured output means we should use the default */
                        RtAudio audio (DCPOMATIC_RTAUDIO_API);
			try {
				configured_so = audio.getDeviceInfo(audio.getDefaultOutputDevice()).name;
			} catch (RtAudioError& e) {
				/* Probably no audio devices at all */
			}
                }

                if (configured_so && current_so != configured_so) {
                        /* Update _preview_sound_output with the configured value */
                        unsigned int i = 0;
                        while (i < _preview_sound_output->GetCount()) {
                                if (_preview_sound_output->GetString(i) == std_to_wx(*configured_so)) {
                                        _preview_sound_output->SetSelection (i);
                                        break;
                                }
                                ++i;
                        }
                }

		setup_sensitivity ();
	}

        /** @return Currently-selected preview sound output in the dialogue */
        optional<string> get_preview_sound_output ()
        {
                int const sel = _preview_sound_output->GetSelection ();
                if (sel == wxNOT_FOUND) {
                        return optional<string> ();
                }

                return wx_to_std (_preview_sound_output->GetString (sel));
        }

	void setup_sensitivity ()
	{
		_language->Enable (_set_language->GetValue ());
		_check_for_test_updates->Enable (_check_for_updates->GetValue ());
		_preview_sound_output->Enable (_preview_sound->GetValue ());
	}

	void set_language_changed ()
	{
		setup_sensitivity ();
		if (_set_language->GetValue ()) {
			language_changed ();
		} else {
			Config::instance()->unset_language ();
		}
	}

	void language_changed ()
	{
		int const sel = _language->GetSelection ();
		if (sel != -1) {
			Config::instance()->set_language (string_client_data (_language->GetClientObject (sel)));
		} else {
			Config::instance()->unset_language ();
		}
	}

#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	void analyse_ebur128_changed ()
	{
		Config::instance()->set_analyse_ebur128 (_analyse_ebur128->GetValue ());
	}
#endif

	void automatic_audio_analysis_changed ()
	{
		Config::instance()->set_automatic_audio_analysis (_automatic_audio_analysis->GetValue ());
	}

	void check_for_updates_changed ()
	{
		Config::instance()->set_check_for_updates (_check_for_updates->GetValue ());
	}

	void check_for_test_updates_changed ()
	{
		Config::instance()->set_check_for_test_updates (_check_for_test_updates->GetValue ());
	}

	void master_encoding_threads_changed ()
	{
		Config::instance()->set_master_encoding_threads (_master_encoding_threads->GetValue ());
	}

	void server_encoding_threads_changed ()
	{
		Config::instance()->set_server_encoding_threads (_server_encoding_threads->GetValue ());
	}

	void issuer_changed ()
	{
		Config::instance()->set_dcp_issuer (wx_to_std (_issuer->GetValue ()));
	}

	void creator_changed ()
	{
		Config::instance()->set_dcp_creator (wx_to_std (_creator->GetValue ()));
	}

	void cinemas_file_changed ()
	{
		Config::instance()->set_cinemas_file (wx_to_std (_cinemas_file->GetPath ()));
	}

	void preview_sound_changed ()
	{
		Config::instance()->set_preview_sound (_preview_sound->GetValue ());
	}

        void preview_sound_output_changed ()
        {
                RtAudio audio (DCPOMATIC_RTAUDIO_API);
                optional<string> const so = get_preview_sound_output();
                if (!so || *so == audio.getDeviceInfo(audio.getDefaultOutputDevice()).name) {
                        Config::instance()->unset_preview_sound_output ();
                } else {
                        Config::instance()->set_preview_sound_output (*so);
                }
        }

	wxCheckBox* _set_language;
	wxChoice* _language;
	wxSpinCtrl* _master_encoding_threads;
	wxSpinCtrl* _server_encoding_threads;
	FilePickerCtrl* _cinemas_file;
	wxCheckBox* _preview_sound;
	wxChoice* _preview_sound_output;
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	wxCheckBox* _analyse_ebur128;
#endif
	wxCheckBox* _automatic_audio_analysis;
	wxCheckBox* _check_for_updates;
	wxCheckBox* _check_for_test_updates;
	wxTextCtrl* _issuer;
	wxTextCtrl* _creator;
};

class DefaultsPage : public StandardPage
{
public:
	DefaultsPage (wxSize panel_size, int border)
		: StandardPage (panel_size, border)
	{}

	wxString GetName () const
	{
		return _("Defaults");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const
	{
		return wxBitmap ("defaults", wxBITMAP_TYPE_PNG_RESOURCE);
	}
#endif

private:
	void setup ()
	{
		wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol (1, 1);
		_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

		{
			add_label_to_sizer (table, _panel, _("Default duration of still images"), true);
			wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
			_still_length = new wxSpinCtrl (_panel);
			s->Add (_still_length);
			add_label_to_sizer (s, _panel, _("s"), false);
			table->Add (s, 1);
		}

		add_label_to_sizer (table, _panel, _("Default directory for new films"), true);
#ifdef DCPOMATIC_USE_OWN_PICKER
		_directory = new DirPickerCtrl (_panel);
#else
		_directory = new wxDirPickerCtrl (_panel, wxDD_DIR_MUST_EXIST);
#endif
		table->Add (_directory, 1, wxEXPAND);

		add_label_to_sizer (table, _panel, _("Default ISDCF name details"), true);
		_isdcf_metadata_button = new wxButton (_panel, wxID_ANY, _("Edit..."));
		table->Add (_isdcf_metadata_button);

		add_label_to_sizer (table, _panel, _("Default container"), true);
		_container = new wxChoice (_panel, wxID_ANY);
		table->Add (_container);

		add_label_to_sizer (table, _panel, _("Default scale-to"), true);
		_scale_to = new wxChoice (_panel, wxID_ANY);
		table->Add (_scale_to);

		add_label_to_sizer (table, _panel, _("Default content type"), true);
		_dcp_content_type = new wxChoice (_panel, wxID_ANY);
		table->Add (_dcp_content_type);

		add_label_to_sizer (table, _panel, _("Default DCP audio channels"), true);
		_dcp_audio_channels = new wxChoice (_panel, wxID_ANY);
		table->Add (_dcp_audio_channels);

		{
			add_label_to_sizer (table, _panel, _("Default JPEG2000 bandwidth"), true);
			wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
			_j2k_bandwidth = new wxSpinCtrl (_panel);
			s->Add (_j2k_bandwidth);
			add_label_to_sizer (s, _panel, _("Mbit/s"), false);
			table->Add (s, 1);
		}

		{
			add_label_to_sizer (table, _panel, _("Default audio delay"), true);
			wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
			_audio_delay = new wxSpinCtrl (_panel);
			s->Add (_audio_delay);
			add_label_to_sizer (s, _panel, _("ms"), false);
			table->Add (s, 1);
		}

		add_label_to_sizer (table, _panel, _("Default standard"), true);
		_standard = new wxChoice (_panel, wxID_ANY);
		table->Add (_standard);

		add_label_to_sizer (table, _panel, _("Default KDM directory"), true);
#ifdef DCPOMATIC_USE_OWN_PICKER
		_kdm_directory = new DirPickerCtrl (_panel);
#else
		_kdm_directory = new wxDirPickerCtrl (_panel, wxDD_DIR_MUST_EXIST);
#endif
		table->Add (_kdm_directory, 1, wxEXPAND);

		_still_length->SetRange (1, 3600);
		_still_length->Bind (wxEVT_SPINCTRL, boost::bind (&DefaultsPage::still_length_changed, this));

		_directory->Bind (wxEVT_DIRPICKER_CHANGED, boost::bind (&DefaultsPage::directory_changed, this));
		_kdm_directory->Bind (wxEVT_DIRPICKER_CHANGED, boost::bind (&DefaultsPage::kdm_directory_changed, this));

		_isdcf_metadata_button->Bind (wxEVT_BUTTON, boost::bind (&DefaultsPage::edit_isdcf_metadata_clicked, this));

		BOOST_FOREACH (Ratio const * i, Ratio::containers()) {
			_container->Append (std_to_wx(i->container_nickname()));
		}

		_container->Bind (wxEVT_CHOICE, boost::bind (&DefaultsPage::container_changed, this));

		_scale_to->Append (_("Guess from content"));

		BOOST_FOREACH (Ratio const * i, Ratio::all()) {
			_scale_to->Append (std_to_wx(i->image_nickname()));
		}

		_scale_to->Bind (wxEVT_CHOICE, boost::bind (&DefaultsPage::scale_to_changed, this));

		BOOST_FOREACH (DCPContentType const * i, DCPContentType::all()) {
			_dcp_content_type->Append (std_to_wx (i->pretty_name ()));
		}

		setup_audio_channels_choice (_dcp_audio_channels, 2);

		_dcp_content_type->Bind (wxEVT_CHOICE, boost::bind (&DefaultsPage::dcp_content_type_changed, this));
		_dcp_audio_channels->Bind (wxEVT_CHOICE, boost::bind (&DefaultsPage::dcp_audio_channels_changed, this));

		_j2k_bandwidth->SetRange (50, 250);
		_j2k_bandwidth->Bind (wxEVT_SPINCTRL, boost::bind (&DefaultsPage::j2k_bandwidth_changed, this));

		_audio_delay->SetRange (-1000, 1000);
		_audio_delay->Bind (wxEVT_SPINCTRL, boost::bind (&DefaultsPage::audio_delay_changed, this));

		_standard->Append (_("SMPTE"));
		_standard->Append (_("Interop"));
		_standard->Bind (wxEVT_CHOICE, boost::bind (&DefaultsPage::standard_changed, this));
	}

	void config_changed ()
	{
		Config* config = Config::instance ();

		vector<Ratio const *> containers = Ratio::containers ();
		for (size_t i = 0; i < containers.size(); ++i) {
			if (containers[i] == config->default_container ()) {
				_container->SetSelection (i);
			}
		}

		vector<Ratio const *> ratios = Ratio::all ();
		for (size_t i = 0; i < ratios.size(); ++i) {
			if (ratios[i] == config->default_scale_to ()) {
				_scale_to->SetSelection (i + 1);
			}
		}

		if (!config->default_scale_to()) {
			_scale_to->SetSelection (0);
		}

		vector<DCPContentType const *> const ct = DCPContentType::all ();
		for (size_t i = 0; i < ct.size(); ++i) {
			if (ct[i] == config->default_dcp_content_type ()) {
				_dcp_content_type->SetSelection (i);
			}
		}

		checked_set (_still_length, config->default_still_length ());
		_directory->SetPath (std_to_wx (config->default_directory_or (wx_to_std (wxStandardPaths::Get().GetDocumentsDir())).string ()));
		_kdm_directory->SetPath (std_to_wx (config->default_kdm_directory_or (wx_to_std (wxStandardPaths::Get().GetDocumentsDir())).string ()));
		checked_set (_j2k_bandwidth, config->default_j2k_bandwidth() / 1000000);
		_j2k_bandwidth->SetRange (50, config->maximum_j2k_bandwidth() / 1000000);
		checked_set (_dcp_audio_channels, locale_convert<string> (config->default_dcp_audio_channels()));
		checked_set (_audio_delay, config->default_audio_delay ());
		checked_set (_standard, config->default_interop() ? 1 : 0);
	}

	void j2k_bandwidth_changed ()
	{
		Config::instance()->set_default_j2k_bandwidth (_j2k_bandwidth->GetValue() * 1000000);
	}

	void audio_delay_changed ()
	{
		Config::instance()->set_default_audio_delay (_audio_delay->GetValue());
	}

	void dcp_audio_channels_changed ()
	{
		int const s = _dcp_audio_channels->GetSelection ();
		if (s != wxNOT_FOUND) {
			Config::instance()->set_default_dcp_audio_channels (
				locale_convert<int> (string_client_data (_dcp_audio_channels->GetClientObject (s)))
				);
		}
	}

	void directory_changed ()
	{
		Config::instance()->set_default_directory (wx_to_std (_directory->GetPath ()));
	}

	void kdm_directory_changed ()
	{
		Config::instance()->set_default_kdm_directory (wx_to_std (_kdm_directory->GetPath ()));
	}

	void edit_isdcf_metadata_clicked ()
	{
		ISDCFMetadataDialog* d = new ISDCFMetadataDialog (_panel, Config::instance()->default_isdcf_metadata (), false);
		d->ShowModal ();
		Config::instance()->set_default_isdcf_metadata (d->isdcf_metadata ());
		d->Destroy ();
	}

	void still_length_changed ()
	{
		Config::instance()->set_default_still_length (_still_length->GetValue ());
	}

	void container_changed ()
	{
		vector<Ratio const *> ratio = Ratio::containers ();
		Config::instance()->set_default_container (ratio[_container->GetSelection()]);
	}

	void scale_to_changed ()
	{
		int const s = _scale_to->GetSelection ();
		if (s == 0) {
			Config::instance()->set_default_scale_to (0);
		} else {
			vector<Ratio const *> ratio = Ratio::all ();
			Config::instance()->set_default_scale_to (ratio[s - 1]);
		}
	}

	void dcp_content_type_changed ()
	{
		vector<DCPContentType const *> ct = DCPContentType::all ();
		Config::instance()->set_default_dcp_content_type (ct[_dcp_content_type->GetSelection()]);
	}

	void standard_changed ()
	{
		Config::instance()->set_default_interop (_standard->GetSelection() == 1);
	}

	wxSpinCtrl* _j2k_bandwidth;
	wxSpinCtrl* _audio_delay;
	wxButton* _isdcf_metadata_button;
	wxSpinCtrl* _still_length;
#ifdef DCPOMATIC_USE_OWN_PICKER
	DirPickerCtrl* _directory;
	DirPickerCtrl* _kdm_directory;
#else
	wxDirPickerCtrl* _directory;
	wxDirPickerCtrl* _kdm_directory;
#endif
	wxChoice* _container;
	wxChoice* _scale_to;
	wxChoice* _dcp_content_type;
	wxChoice* _dcp_audio_channels;
	wxChoice* _standard;
};

class EncodingServersPage : public StandardPage
{
public:
	EncodingServersPage (wxSize panel_size, int border)
		: StandardPage (panel_size, border)
	{}

	wxString GetName () const
	{
		return _("Servers");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const
	{
		return wxBitmap ("servers", wxBITMAP_TYPE_PNG_RESOURCE);
	}
#endif

private:
	void setup ()
	{
		_use_any_servers = new wxCheckBox (_panel, wxID_ANY, _("Search network for servers"));
		_panel->GetSizer()->Add (_use_any_servers, 0, wxALL, _border);

		vector<string> columns;
		columns.push_back (wx_to_std (_("IP address / host name")));
		_servers_list = new EditableList<string, ServerDialog> (
			_panel,
			columns,
			boost::bind (&Config::servers, Config::instance()),
			boost::bind (&Config::set_servers, Config::instance(), _1),
			boost::bind (&EncodingServersPage::server_column, this, _1)
			);

		_panel->GetSizer()->Add (_servers_list, 1, wxEXPAND | wxALL, _border);

		_use_any_servers->Bind (wxEVT_CHECKBOX, boost::bind (&EncodingServersPage::use_any_servers_changed, this));
	}

	void config_changed ()
	{
		checked_set (_use_any_servers, Config::instance()->use_any_servers ());
		_servers_list->refresh ();
	}

	void use_any_servers_changed ()
	{
		Config::instance()->set_use_any_servers (_use_any_servers->GetValue ());
	}

	string server_column (string s)
	{
		return s;
	}

	wxCheckBox* _use_any_servers;
	EditableList<string, ServerDialog>* _servers_list;
};

class CertificateChainEditor : public wxPanel
{
public:
	CertificateChainEditor (
		wxWindow* parent,
		wxString title,
		int border,
		function<void (shared_ptr<dcp::CertificateChain>)> set,
		function<shared_ptr<const dcp::CertificateChain> (void)> get,
		function<void (void)> nag_remake
		)
		: wxPanel (parent)
		, _set (set)
		, _get (get)
		, _nag_remake (nag_remake)
	{
		wxFont subheading_font (*wxNORMAL_FONT);
		subheading_font.SetWeight (wxFONTWEIGHT_BOLD);

		_sizer = new wxBoxSizer (wxVERTICAL);

		{
			wxStaticText* m = new wxStaticText (this, wxID_ANY, title);
			m->SetFont (subheading_font);
			_sizer->Add (m, 0, wxALL, border);
		}

		wxBoxSizer* certificates_sizer = new wxBoxSizer (wxHORIZONTAL);
		_sizer->Add (certificates_sizer, 0, wxLEFT | wxRIGHT, border);

		_certificates = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxSize (440, 150), wxLC_REPORT | wxLC_SINGLE_SEL);

		{
			wxListItem ip;
			ip.SetId (0);
			ip.SetText (_("Type"));
			ip.SetWidth (100);
			_certificates->InsertColumn (0, ip);
		}

		{
			wxListItem ip;
			ip.SetId (1);
			ip.SetText (_("Thumbprint"));
			ip.SetWidth (340);

			wxFont font = ip.GetFont ();
			font.SetFamily (wxFONTFAMILY_TELETYPE);
			ip.SetFont (font);

			_certificates->InsertColumn (1, ip);
		}

		certificates_sizer->Add (_certificates, 1, wxEXPAND);

		{
			wxSizer* s = new wxBoxSizer (wxVERTICAL);
			_add_certificate = new wxButton (this, wxID_ANY, _("Add..."));
			s->Add (_add_certificate, 0, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
			_remove_certificate = new wxButton (this, wxID_ANY, _("Remove"));
			s->Add (_remove_certificate, 0, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
			_export_certificate = new wxButton (this, wxID_ANY, _("Export"));
			s->Add (_export_certificate, 0, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
			certificates_sizer->Add (s, 0, wxLEFT, DCPOMATIC_SIZER_X_GAP);
		}

		wxGridBagSizer* table = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		_sizer->Add (table, 1, wxALL | wxEXPAND, border);
		int r = 0;

		add_label_to_sizer (table, this, _("Leaf private key"), true, wxGBPosition (r, 0));
		_private_key = new wxStaticText (this, wxID_ANY, wxT (""));
		wxFont font = _private_key->GetFont ();
		font.SetFamily (wxFONTFAMILY_TELETYPE);
		_private_key->SetFont (font);
		table->Add (_private_key, wxGBPosition (r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
		_load_private_key = new wxButton (this, wxID_ANY, _("Load..."));
		table->Add (_load_private_key, wxGBPosition (r, 2));
		_export_private_key = new wxButton (this, wxID_ANY, _("Export..."));
		table->Add (_export_private_key, wxGBPosition (r, 3));
		++r;

		_button_sizer = new wxBoxSizer (wxHORIZONTAL);
		_remake_certificates = new wxButton (this, wxID_ANY, _("Re-make certificates\nand key..."));
		_button_sizer->Add (_remake_certificates, 1, wxRIGHT, border);
		table->Add (_button_sizer, wxGBPosition (r, 0), wxGBSpan (1, 4));
		++r;

		_private_key_bad = new wxStaticText (this, wxID_ANY, _("Leaf private key does not match leaf certificate!"));
	        font = *wxSMALL_FONT;
		font.SetWeight (wxFONTWEIGHT_BOLD);
		_private_key_bad->SetFont (font);
		table->Add (_private_key_bad, wxGBPosition (r, 0), wxGBSpan (1, 3));
		++r;

		_add_certificate->Bind     (wxEVT_BUTTON,       boost::bind (&CertificateChainEditor::add_certificate, this));
		_remove_certificate->Bind  (wxEVT_BUTTON,       boost::bind (&CertificateChainEditor::remove_certificate, this));
		_export_certificate->Bind  (wxEVT_BUTTON,       boost::bind (&CertificateChainEditor::export_certificate, this));
		_certificates->Bind        (wxEVT_LIST_ITEM_SELECTED,   boost::bind (&CertificateChainEditor::update_sensitivity, this));
		_certificates->Bind        (wxEVT_LIST_ITEM_DESELECTED, boost::bind (&CertificateChainEditor::update_sensitivity, this));
		_remake_certificates->Bind (wxEVT_BUTTON,       boost::bind (&CertificateChainEditor::remake_certificates, this));
		_load_private_key->Bind    (wxEVT_BUTTON,       boost::bind (&CertificateChainEditor::load_private_key, this));
		_export_private_key->Bind  (wxEVT_BUTTON,       boost::bind (&CertificateChainEditor::export_private_key, this));

		SetSizerAndFit (_sizer);
	}

	void config_changed ()
	{
		_chain.reset (new dcp::CertificateChain (*_get().get ()));

		update_certificate_list ();
		update_private_key ();
		update_sensitivity ();
	}

	void add_button (wxWindow* button)
	{
		_button_sizer->Add (button, 0, wxLEFT | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
		_sizer->Layout ();
	}

private:
	void add_certificate ()
	{
		wxFileDialog* d = new wxFileDialog (this, _("Select Certificate File"));

		if (d->ShowModal() == wxID_OK) {
			try {
				dcp::Certificate c;
				string extra;
				try {
					extra = c.read_string (dcp::file_to_string (wx_to_std (d->GetPath ())));
				} catch (boost::filesystem::filesystem_error& e) {
					error_dialog (this, wxString::Format (_("Could not load certificate (%s)"), d->GetPath().data()));
					d->Destroy ();
					return;
				}

				if (!extra.empty ()) {
					message_dialog (
						this,
						_("This file contains other certificates (or other data) after its first certificate. "
						  "Only the first certificate will be used.")
						);
				}
				_chain->add (c);
				if (!_chain->chain_valid ()) {
					error_dialog (
						this,
						_("Adding this certificate would make the chain inconsistent, so it will not be added. "
						  "Add certificates in order from root to intermediate to leaf.")
						);
					_chain->remove (c);
				} else {
					_set (_chain);
					update_certificate_list ();
				}
			} catch (dcp::MiscError& e) {
				error_dialog (this, wxString::Format (_("Could not read certificate file (%s)"), e.what ()));
			}
		}

		d->Destroy ();

		update_sensitivity ();
	}

	void remove_certificate ()
	{
		int i = _certificates->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (i == -1) {
			return;
		}

		_certificates->DeleteItem (i);
		_chain->remove (i);
		_set (_chain);

		update_sensitivity ();
	}

	void export_certificate ()
	{
		int i = _certificates->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (i == -1) {
			return;
		}

		wxFileDialog* d = new wxFileDialog (
			this, _("Select Certificate File"), wxEmptyString, wxEmptyString, wxT ("PEM files (*.pem)|*.pem"),
			wxFD_SAVE | wxFD_OVERWRITE_PROMPT
			);

		dcp::CertificateChain::List all = _chain->root_to_leaf ();
		dcp::CertificateChain::List::iterator j = all.begin ();
		for (int k = 0; k < i; ++k) {
			++j;
		}

		if (d->ShowModal () == wxID_OK) {
			FILE* f = fopen_boost (wx_to_std (d->GetPath ()), "w");
			if (!f) {
				throw OpenFileError (wx_to_std (d->GetPath ()), errno, false);
			}

			string const s = j->certificate (true);
			fwrite (s.c_str(), 1, s.length(), f);
			fclose (f);
		}
		d->Destroy ();
	}

	void update_certificate_list ()
	{
		_certificates->DeleteAllItems ();
		size_t n = 0;
		dcp::CertificateChain::List certs = _chain->root_to_leaf ();
		BOOST_FOREACH (dcp::Certificate const & i, certs) {
			wxListItem item;
			item.SetId (n);
			_certificates->InsertItem (item);
			_certificates->SetItem (n, 1, std_to_wx (i.thumbprint ()));

			if (n == 0) {
				_certificates->SetItem (n, 0, _("Root"));
			} else if (n == (certs.size() - 1)) {
				_certificates->SetItem (n, 0, _("Leaf"));
			} else {
				_certificates->SetItem (n, 0, _("Intermediate"));
			}

			++n;
		}

		static wxColour normal = _private_key_bad->GetForegroundColour ();

		if (_chain->private_key_valid ()) {
			_private_key_bad->Hide ();
			_private_key_bad->SetForegroundColour (normal);
		} else {
			_private_key_bad->Show ();
			_private_key_bad->SetForegroundColour (wxColour (255, 0, 0));
		}
	}

	void remake_certificates ()
	{
		shared_ptr<const dcp::CertificateChain> chain = _get ();

		string subject_organization_name;
		string subject_organizational_unit_name;
		string root_common_name;
		string intermediate_common_name;
		string leaf_common_name;

		dcp::CertificateChain::List all = chain->root_to_leaf ();

		if (all.size() >= 1) {
			/* Have a root */
			subject_organization_name = chain->root().subject_organization_name ();
			subject_organizational_unit_name = chain->root().subject_organizational_unit_name ();
			root_common_name = chain->root().subject_common_name ();
		}

		if (all.size() >= 2) {
			/* Have a leaf */
			leaf_common_name = chain->leaf().subject_common_name ();
		}

		if (all.size() >= 3) {
			/* Have an intermediate */
			dcp::CertificateChain::List::iterator i = all.begin ();
			++i;
			intermediate_common_name = i->subject_common_name ();
		}

		_nag_remake ();

		MakeChainDialog* d = new MakeChainDialog (
			this,
			subject_organization_name,
			subject_organizational_unit_name,
			root_common_name,
			intermediate_common_name,
			leaf_common_name
			);

		if (d->ShowModal () == wxID_OK) {
			_chain.reset (
				new dcp::CertificateChain (
					openssl_path (),
					d->organisation (),
					d->organisational_unit (),
					d->root_common_name (),
					d->intermediate_common_name (),
					d->leaf_common_name ()
					)
				);

			_set (_chain);
			update_certificate_list ();
			update_private_key ();
		}

		d->Destroy ();
	}

	void update_sensitivity ()
	{
		/* We can only remove the leaf certificate */
		_remove_certificate->Enable (_certificates->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) == (_certificates->GetItemCount() - 1));
		_export_certificate->Enable (_certificates->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) != -1);
	}

	void update_private_key ()
	{
		checked_set (_private_key, dcp::private_key_fingerprint (_chain->key().get ()));
		_sizer->Layout ();
	}

	void load_private_key ()
	{
		wxFileDialog* d = new wxFileDialog (this, _("Select Key File"));

		if (d->ShowModal() == wxID_OK) {
			try {
				boost::filesystem::path p (wx_to_std (d->GetPath ()));
				if (boost::filesystem::file_size (p) > 8192) {
					error_dialog (
						this,
						wxString::Format (_("Could not read key file; file is too long (%s)"), std_to_wx (p.string ()))
						);
					return;
				}

				_chain->set_key (dcp::file_to_string (p));
				_set (_chain);
				update_private_key ();
			} catch (dcp::MiscError& e) {
				error_dialog (this, wxString::Format (_("Could not read certificate file (%s)"), e.what ()));
			}
		}

		d->Destroy ();

		update_sensitivity ();
	}

	void export_private_key ()
	{
		optional<string> key = _chain->key ();
		if (!key) {
			return;
		}

		wxFileDialog* d = new wxFileDialog (
			this, _("Select Key File"), wxEmptyString, wxEmptyString, wxT ("PEM files (*.pem)|*.pem"),
			wxFD_SAVE | wxFD_OVERWRITE_PROMPT
			);

		if (d->ShowModal () == wxID_OK) {
			FILE* f = fopen_boost (wx_to_std (d->GetPath ()), "w");
			if (!f) {
				throw OpenFileError (wx_to_std (d->GetPath ()), errno, false);
			}

			string const s = _chain->key().get ();
			fwrite (s.c_str(), 1, s.length(), f);
			fclose (f);
		}
		d->Destroy ();
	}

	wxListCtrl* _certificates;
	wxButton* _add_certificate;
	wxButton* _export_certificate;
	wxButton* _remove_certificate;
	wxButton* _remake_certificates;
	wxStaticText* _private_key;
	wxButton* _load_private_key;
	wxButton* _export_private_key;
	wxStaticText* _private_key_bad;
	wxSizer* _sizer;
	wxBoxSizer* _button_sizer;
	shared_ptr<dcp::CertificateChain> _chain;
	boost::function<void (shared_ptr<dcp::CertificateChain>)> _set;
	boost::function<shared_ptr<const dcp::CertificateChain> (void)> _get;
	boost::function<void (void)> _nag_remake;
};

class KeysPage : public StandardPage
{
public:
	KeysPage (wxSize panel_size, int border)
		: StandardPage (panel_size, border)
	{}

	wxString GetName () const
	{
		return _("Keys");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const
	{
		return wxBitmap ("keys", wxBITMAP_TYPE_PNG_RESOURCE);
	}
#endif

private:

	void setup ()
	{
		_signer = new CertificateChainEditor (
			_panel, _("Signing DCPs and KDMs"), _border,
			boost::bind (&Config::set_signer_chain, Config::instance (), _1),
			boost::bind (&Config::signer_chain, Config::instance ()),
			boost::bind (&do_nothing)
			);

		_panel->GetSizer()->Add (_signer);

		_decryption = new CertificateChainEditor (
			_panel, _("Decrypting KDMs"), _border,
			boost::bind (&Config::set_decryption_chain, Config::instance (), _1),
			boost::bind (&Config::decryption_chain, Config::instance ()),
			boost::bind (&KeysPage::nag_remake_decryption_chain, this)
			);

		_panel->GetSizer()->Add (_decryption);

		_export_decryption_certificate = new wxButton (_decryption, wxID_ANY, _("Export KDM decryption\ncertificate..."));
		_decryption->add_button (_export_decryption_certificate);
		_export_decryption_chain = new wxButton (_decryption, wxID_ANY, _("Export KDM decryption\nchain..."));
		_decryption->add_button (_export_decryption_chain);

		_export_decryption_certificate->Bind (wxEVT_BUTTON, boost::bind (&KeysPage::export_decryption_certificate, this));
		_export_decryption_chain->Bind (wxEVT_BUTTON, boost::bind (&KeysPage::export_decryption_chain, this));
	}

	void export_decryption_certificate ()
	{
		wxFileDialog* d = new wxFileDialog (
			_panel, _("Select Certificate File"), wxEmptyString, wxEmptyString, wxT ("PEM files (*.pem)|*.pem"),
			wxFD_SAVE | wxFD_OVERWRITE_PROMPT
			);

		if (d->ShowModal () == wxID_OK) {
			FILE* f = fopen_boost (wx_to_std (d->GetPath ()), "w");
			if (!f) {
				throw OpenFileError (wx_to_std (d->GetPath ()), errno, false);
			}

			string const s = Config::instance()->decryption_chain()->leaf().certificate (true);
			fwrite (s.c_str(), 1, s.length(), f);
			fclose (f);
		}
		d->Destroy ();
	}

	void export_decryption_chain ()
	{
		wxFileDialog* d = new wxFileDialog (
			_panel, _("Select Chain File"), wxEmptyString, wxEmptyString, wxT ("PEM files (*.pem)|*.pem"),
			wxFD_SAVE | wxFD_OVERWRITE_PROMPT
			);

		if (d->ShowModal () == wxID_OK) {
			FILE* f = fopen_boost (wx_to_std (d->GetPath ()), "w");
			if (!f) {
				throw OpenFileError (wx_to_std (d->GetPath ()), errno, false);
			}

			string const s = Config::instance()->decryption_chain()->chain();
			fwrite (s.c_str(), 1, s.length(), f);
			fclose (f);
		}
		d->Destroy ();
	}

	void config_changed ()
	{
		_signer->config_changed ();
		_decryption->config_changed ();
	}

	void nag_remake_decryption_chain ()
	{
		NagDialog::maybe_nag (
			_panel,
			Config::NAG_REMAKE_DECRYPTION_CHAIN,
			_("If you continue with this operation you will no longer be able to use any DKDMs that you have created.  Also, any KDMs that have been sent to you will become useless.  Proceed with caution!")
			);
	}

	CertificateChainEditor* _signer;
	CertificateChainEditor* _decryption;
	wxButton* _export_decryption_certificate;
	wxButton* _export_decryption_chain;
};

class TMSPage : public StandardPage
{
public:
	TMSPage (wxSize panel_size, int border)
		: StandardPage (panel_size, border)
	{}

	wxString GetName () const
	{
		return _("TMS");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const
	{
		return wxBitmap ("tms", wxBITMAP_TYPE_PNG_RESOURCE);
	}
#endif

private:
	void setup ()
	{
		wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol (1, 1);
		_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

		add_label_to_sizer (table, _panel, _("Protocol"), true);
		_tms_protocol = new wxChoice (_panel, wxID_ANY);
		table->Add (_tms_protocol, 1, wxEXPAND);

		add_label_to_sizer (table, _panel, _("IP address"), true);
		_tms_ip = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_tms_ip, 1, wxEXPAND);

		add_label_to_sizer (table, _panel, _("Target path"), true);
		_tms_path = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_tms_path, 1, wxEXPAND);

		add_label_to_sizer (table, _panel, _("User name"), true);
		_tms_user = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_tms_user, 1, wxEXPAND);

		add_label_to_sizer (table, _panel, _("Password"), true);
		_tms_password = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_tms_password, 1, wxEXPAND);

		_tms_protocol->Append (_("SCP (for AAM and Doremi)"));
		_tms_protocol->Append (_("FTP (for Dolby)"));

		_tms_protocol->Bind (wxEVT_CHOICE, boost::bind (&TMSPage::tms_protocol_changed, this));
		_tms_ip->Bind (wxEVT_TEXT, boost::bind (&TMSPage::tms_ip_changed, this));
		_tms_path->Bind (wxEVT_TEXT, boost::bind (&TMSPage::tms_path_changed, this));
		_tms_user->Bind (wxEVT_TEXT, boost::bind (&TMSPage::tms_user_changed, this));
		_tms_password->Bind (wxEVT_TEXT, boost::bind (&TMSPage::tms_password_changed, this));
	}

	void config_changed ()
	{
		Config* config = Config::instance ();

		checked_set (_tms_protocol, config->tms_protocol ());
		checked_set (_tms_ip, config->tms_ip ());
		checked_set (_tms_path, config->tms_path ());
		checked_set (_tms_user, config->tms_user ());
		checked_set (_tms_password, config->tms_password ());
	}

	void tms_protocol_changed ()
	{
		Config::instance()->set_tms_protocol (static_cast<Protocol> (_tms_protocol->GetSelection ()));
	}

	void tms_ip_changed ()
	{
		Config::instance()->set_tms_ip (wx_to_std (_tms_ip->GetValue ()));
	}

	void tms_path_changed ()
	{
		Config::instance()->set_tms_path (wx_to_std (_tms_path->GetValue ()));
	}

	void tms_user_changed ()
	{
		Config::instance()->set_tms_user (wx_to_std (_tms_user->GetValue ()));
	}

	void tms_password_changed ()
	{
		Config::instance()->set_tms_password (wx_to_std (_tms_password->GetValue ()));
	}

	wxChoice* _tms_protocol;
	wxTextCtrl* _tms_ip;
	wxTextCtrl* _tms_path;
	wxTextCtrl* _tms_user;
	wxTextCtrl* _tms_password;
};

static string
column (string s)
{
	return s;
}

class KDMEmailPage : public StandardPage
{
public:

	KDMEmailPage (wxSize panel_size, int border)
#ifdef DCPOMATIC_OSX
		/* We have to force both width and height of this one */
		: StandardPage (wxSize (480, 128), border)
#else
		: StandardPage (panel_size, border)
#endif
	{}

	wxString GetName () const
	{
		return _("KDM Email");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const
	{
		return wxBitmap ("kdm_email", wxBITMAP_TYPE_PNG_RESOURCE);
	}
#endif

private:
	void setup ()
	{
		wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol (1, 1);
		_panel->GetSizer()->Add (table, 1, wxEXPAND | wxALL, _border);

		add_label_to_sizer (table, _panel, _("Outgoing mail server"), true);
		{
			wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
			_mail_server = new wxTextCtrl (_panel, wxID_ANY);
			s->Add (_mail_server, 1, wxEXPAND | wxALL);
			add_label_to_sizer (s, _panel, _("port"), false);
			_mail_port = new wxSpinCtrl (_panel, wxID_ANY);
			_mail_port->SetRange (0, 65535);
			s->Add (_mail_port);
			table->Add (s, 1, wxEXPAND | wxALL);
		}

		add_label_to_sizer (table, _panel, _("Mail user name"), true);
		_mail_user = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_mail_user, 1, wxEXPAND | wxALL);

		add_label_to_sizer (table, _panel, _("Mail password"), true);
		_mail_password = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_mail_password, 1, wxEXPAND | wxALL);

		add_label_to_sizer (table, _panel, _("Subject"), true);
		_kdm_subject = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_kdm_subject, 1, wxEXPAND | wxALL);

		add_label_to_sizer (table, _panel, _("From address"), true);
		_kdm_from = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_kdm_from, 1, wxEXPAND | wxALL);

		vector<string> columns;
		columns.push_back (wx_to_std (_("Address")));
		add_label_to_sizer (table, _panel, _("CC addresses"), true);
		_kdm_cc = new EditableList<string, EmailDialog> (
			_panel,
			columns,
			bind (&Config::kdm_cc, Config::instance()),
			bind (&Config::set_kdm_cc, Config::instance(), _1),
			bind (&column, _1)
			);
		table->Add (_kdm_cc, 1, wxEXPAND | wxALL);

		add_label_to_sizer (table, _panel, _("BCC address"), true);
		_kdm_bcc = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_kdm_bcc, 1, wxEXPAND | wxALL);

		_kdm_email = new wxTextCtrl (_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (-1, 200), wxTE_MULTILINE);
		_panel->GetSizer()->Add (_kdm_email, 0, wxEXPAND | wxALL, _border);

		_reset_kdm_email = new wxButton (_panel, wxID_ANY, _("Reset to default subject and text"));
		_panel->GetSizer()->Add (_reset_kdm_email, 0, wxEXPAND | wxALL, _border);

		_kdm_cc->layout ();

		_mail_server->Bind (wxEVT_TEXT, boost::bind (&KDMEmailPage::mail_server_changed, this));
		_mail_port->Bind (wxEVT_SPINCTRL, boost::bind (&KDMEmailPage::mail_port_changed, this));
		_mail_user->Bind (wxEVT_TEXT, boost::bind (&KDMEmailPage::mail_user_changed, this));
		_mail_password->Bind (wxEVT_TEXT, boost::bind (&KDMEmailPage::mail_password_changed, this));
		_kdm_subject->Bind (wxEVT_TEXT, boost::bind (&KDMEmailPage::kdm_subject_changed, this));
		_kdm_from->Bind (wxEVT_TEXT, boost::bind (&KDMEmailPage::kdm_from_changed, this));
		_kdm_bcc->Bind (wxEVT_TEXT, boost::bind (&KDMEmailPage::kdm_bcc_changed, this));
		_kdm_email->Bind (wxEVT_TEXT, boost::bind (&KDMEmailPage::kdm_email_changed, this));
		_reset_kdm_email->Bind (wxEVT_BUTTON, boost::bind (&KDMEmailPage::reset_kdm_email, this));
	}

	void config_changed ()
	{
		Config* config = Config::instance ();

		checked_set (_mail_server, config->mail_server ());
		checked_set (_mail_port, config->mail_port ());
		checked_set (_mail_user, config->mail_user ());
		checked_set (_mail_password, config->mail_password ());
		checked_set (_kdm_subject, config->kdm_subject ());
		checked_set (_kdm_from, config->kdm_from ());
		checked_set (_kdm_bcc, config->kdm_bcc ());
		checked_set (_kdm_email, Config::instance()->kdm_email ());
	}

	void mail_server_changed ()
	{
		Config::instance()->set_mail_server (wx_to_std (_mail_server->GetValue ()));
	}

	void mail_port_changed ()
	{
		Config::instance()->set_mail_port (_mail_port->GetValue ());
	}

	void mail_user_changed ()
	{
		Config::instance()->set_mail_user (wx_to_std (_mail_user->GetValue ()));
	}

	void mail_password_changed ()
	{
		Config::instance()->set_mail_password (wx_to_std (_mail_password->GetValue ()));
	}

	void kdm_subject_changed ()
	{
		Config::instance()->set_kdm_subject (wx_to_std (_kdm_subject->GetValue ()));
	}

	void kdm_from_changed ()
	{
		Config::instance()->set_kdm_from (wx_to_std (_kdm_from->GetValue ()));
	}

	void kdm_bcc_changed ()
	{
		Config::instance()->set_kdm_bcc (wx_to_std (_kdm_bcc->GetValue ()));
	}

	void kdm_email_changed ()
	{
		if (_kdm_email->GetValue().IsEmpty ()) {
			/* Sometimes we get sent an erroneous notification that the email
			   is empty; I don't know why.
			*/
			return;
		}
		Config::instance()->set_kdm_email (wx_to_std (_kdm_email->GetValue ()));
	}

	void reset_kdm_email ()
	{
		Config::instance()->reset_kdm_email ();
		checked_set (_kdm_email, Config::instance()->kdm_email ());
	}

	wxTextCtrl* _mail_server;
	wxSpinCtrl* _mail_port;
	wxTextCtrl* _mail_user;
	wxTextCtrl* _mail_password;
	wxTextCtrl* _kdm_subject;
	wxTextCtrl* _kdm_from;
	EditableList<string, EmailDialog>* _kdm_cc;
	wxTextCtrl* _kdm_bcc;
	wxTextCtrl* _kdm_email;
	wxButton* _reset_kdm_email;
};

class CoverSheetPage : public StandardPage
{
public:

	CoverSheetPage (wxSize panel_size, int border)
#ifdef DCPOMATIC_OSX
		/* We have to force both width and height of this one */
		: StandardPage (wxSize (480, 128), border)
#else
		: StandardPage (panel_size, border)
#endif
	{}

	wxString GetName () const
	{
		return _("Cover Sheet");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const
	{
		return wxBitmap ("cover_sheet", wxBITMAP_TYPE_PNG_RESOURCE);
	}
#endif

private:
	void setup ()
	{
		_cover_sheet = new wxTextCtrl (_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (-1, 200), wxTE_MULTILINE);
		_panel->GetSizer()->Add (_cover_sheet, 0, wxEXPAND | wxALL, _border);

		_reset_cover_sheet = new wxButton (_panel, wxID_ANY, _("Reset to default text"));
		_panel->GetSizer()->Add (_reset_cover_sheet, 0, wxEXPAND | wxALL, _border);

		_cover_sheet->Bind (wxEVT_TEXT, boost::bind (&CoverSheetPage::cover_sheet_changed, this));
		_reset_cover_sheet->Bind (wxEVT_BUTTON, boost::bind (&CoverSheetPage::reset_cover_sheet, this));
	}

	void config_changed ()
	{
		checked_set (_cover_sheet, Config::instance()->cover_sheet ());
	}

	void cover_sheet_changed ()
	{
		if (_cover_sheet->GetValue().IsEmpty ()) {
			/* Sometimes we get sent an erroneous notification that the cover sheet
			   is empty; I don't know why.
			*/
			return;
		}
		Config::instance()->set_cover_sheet (wx_to_std (_cover_sheet->GetValue ()));
	}

	void reset_cover_sheet ()
	{
		Config::instance()->reset_cover_sheet ();
		checked_set (_cover_sheet, Config::instance()->cover_sheet ());
	}

	wxTextCtrl* _cover_sheet;
	wxButton* _reset_cover_sheet;
};


/** @class AdvancedPage
 *  @brief Advanced page of the preferences dialog.
 */
class AdvancedPage : public StockPage
{
public:
	AdvancedPage (wxSize panel_size, int border)
		: StockPage (Kind_Advanced, panel_size, border)
		, _maximum_j2k_bandwidth (0)
		, _allow_any_dcp_frame_rate (0)
		, _only_servers_encode (0)
		, _log_general (0)
		, _log_warning (0)
		, _log_error (0)
		, _log_timing (0)
		, _log_debug_decode (0)
		, _log_debug_encode (0)
		, _log_debug_email (0)
	{}

private:
	void add_top_aligned_label_to_sizer (wxSizer* table, wxWindow* parent, wxString text)
	{
		int flags = wxALIGN_TOP | wxTOP | wxLEFT;
#ifdef __WXOSX__
		flags |= wxALIGN_RIGHT;
		text += wxT (":");
#endif
		wxStaticText* m = new wxStaticText (parent, wxID_ANY, text);
		table->Add (m, 0, flags, DCPOMATIC_SIZER_Y_GAP);
	}

	void setup ()
	{
		wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol (1, 1);
		_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

		{
			add_label_to_sizer (table, _panel, _("Maximum JPEG2000 bandwidth"), true);
			wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
			_maximum_j2k_bandwidth = new wxSpinCtrl (_panel);
			s->Add (_maximum_j2k_bandwidth, 1);
			add_label_to_sizer (s, _panel, _("Mbit/s"), false);
			table->Add (s, 1);
		}

		_allow_any_dcp_frame_rate = new wxCheckBox (_panel, wxID_ANY, _("Allow any DCP frame rate"));
		table->Add (_allow_any_dcp_frame_rate, 1, wxEXPAND | wxALL);
		table->AddSpacer (0);

		_only_servers_encode = new wxCheckBox (_panel, wxID_ANY, _("Only servers encode"));
		table->Add (_only_servers_encode, 1, wxEXPAND | wxALL);
		table->AddSpacer (0);

		{
			add_top_aligned_label_to_sizer (table, _panel, _("DCP metadata filename format"));
			dcp::NameFormat::Map titles;
			titles['t'] = "type (cpl/pkl)";
			dcp::NameFormat::Map examples;
			examples['t'] = "cpl";
			_dcp_metadata_filename_format = new NameFormatEditor (
				_panel, Config::instance()->dcp_metadata_filename_format(), titles, examples, "_eb1c112c-ca3c-4ae6-9263-c6714ff05d64.xml"
				);
			table->Add (_dcp_metadata_filename_format->panel(), 1, wxEXPAND | wxALL);
		}

		{
			add_top_aligned_label_to_sizer (table, _panel, _("DCP asset filename format"));
			dcp::NameFormat::Map titles;
			titles['t'] = "type (j2c/pcm/sub)";
			titles['r'] = "reel number";
			titles['n'] = "number of reels";
			titles['c'] = "content filename";
			dcp::NameFormat::Map examples;
			examples['t'] = "j2c";
			examples['r'] = "1";
			examples['n'] = "4";
			examples['c'] = "myfile.mp4";
			_dcp_asset_filename_format = new NameFormatEditor (
				_panel, Config::instance()->dcp_asset_filename_format(), titles, examples, "_eb1c112c-ca3c-4ae6-9263-c6714ff05d64.mxf"
				);
			table->Add (_dcp_asset_filename_format->panel(), 1, wxEXPAND | wxALL);
		}

		{
			add_top_aligned_label_to_sizer (table, _panel, _("Log"));
			wxBoxSizer* t = new wxBoxSizer (wxVERTICAL);
			_log_general = new wxCheckBox (_panel, wxID_ANY, _("General"));
			t->Add (_log_general, 1, wxEXPAND | wxALL);
			_log_warning = new wxCheckBox (_panel, wxID_ANY, _("Warnings"));
			t->Add (_log_warning, 1, wxEXPAND | wxALL);
			_log_error = new wxCheckBox (_panel, wxID_ANY, _("Errors"));
			t->Add (_log_error, 1, wxEXPAND | wxALL);
			/// TRANSLATORS: translate the word "Timing" here; do not include the "Config|" prefix
			_log_timing = new wxCheckBox (_panel, wxID_ANY, S_("Config|Timing"));
			t->Add (_log_timing, 1, wxEXPAND | wxALL);
			_log_debug_decode = new wxCheckBox (_panel, wxID_ANY, _("Debug: decode"));
			t->Add (_log_debug_decode, 1, wxEXPAND | wxALL);
			_log_debug_encode = new wxCheckBox (_panel, wxID_ANY, _("Debug: encode"));
			t->Add (_log_debug_encode, 1, wxEXPAND | wxALL);
			_log_debug_email = new wxCheckBox (_panel, wxID_ANY, _("Debug: email sending"));
			t->Add (_log_debug_email, 1, wxEXPAND | wxALL);
			table->Add (t, 0, wxALL, 6);
		}

#ifdef DCPOMATIC_WINDOWS
		_win32_console = new wxCheckBox (_panel, wxID_ANY, _("Open console window"));
		table->Add (_win32_console, 1, wxEXPAND | wxALL);
		table->AddSpacer (0);
#endif

		_maximum_j2k_bandwidth->SetRange (1, 1000);
		_maximum_j2k_bandwidth->Bind (wxEVT_SPINCTRL, boost::bind (&AdvancedPage::maximum_j2k_bandwidth_changed, this));
		_allow_any_dcp_frame_rate->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::allow_any_dcp_frame_rate_changed, this));
		_only_servers_encode->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::only_servers_encode_changed, this));
		_dcp_metadata_filename_format->Changed.connect (boost::bind (&AdvancedPage::dcp_metadata_filename_format_changed, this));
		_dcp_asset_filename_format->Changed.connect (boost::bind (&AdvancedPage::dcp_asset_filename_format_changed, this));
		_log_general->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::log_changed, this));
		_log_warning->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::log_changed, this));
		_log_error->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::log_changed, this));
		_log_timing->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::log_changed, this));
		_log_debug_decode->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::log_changed, this));
		_log_debug_encode->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::log_changed, this));
		_log_debug_email->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::log_changed, this));
#ifdef DCPOMATIC_WINDOWS
		_win32_console->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::win32_console_changed, this));
#endif
	}

	void config_changed ()
	{
		Config* config = Config::instance ();

		checked_set (_maximum_j2k_bandwidth, config->maximum_j2k_bandwidth() / 1000000);
		checked_set (_allow_any_dcp_frame_rate, config->allow_any_dcp_frame_rate ());
		checked_set (_only_servers_encode, config->only_servers_encode ());
		checked_set (_log_general, config->log_types() & LogEntry::TYPE_GENERAL);
		checked_set (_log_warning, config->log_types() & LogEntry::TYPE_WARNING);
		checked_set (_log_error, config->log_types() & LogEntry::TYPE_ERROR);
		checked_set (_log_timing, config->log_types() & LogEntry::TYPE_TIMING);
		checked_set (_log_debug_decode, config->log_types() & LogEntry::TYPE_DEBUG_DECODE);
		checked_set (_log_debug_encode, config->log_types() & LogEntry::TYPE_DEBUG_ENCODE);
		checked_set (_log_debug_email, config->log_types() & LogEntry::TYPE_DEBUG_EMAIL);
#ifdef DCPOMATIC_WINDOWS
		checked_set (_win32_console, config->win32_console());
#endif
	}

	void maximum_j2k_bandwidth_changed ()
	{
		Config::instance()->set_maximum_j2k_bandwidth (_maximum_j2k_bandwidth->GetValue() * 1000000);
	}

	void allow_any_dcp_frame_rate_changed ()
	{
		Config::instance()->set_allow_any_dcp_frame_rate (_allow_any_dcp_frame_rate->GetValue ());
	}

	void only_servers_encode_changed ()
	{
		Config::instance()->set_only_servers_encode (_only_servers_encode->GetValue ());
	}

	void dcp_metadata_filename_format_changed ()
	{
		Config::instance()->set_dcp_metadata_filename_format (_dcp_metadata_filename_format->get ());
	}

	void dcp_asset_filename_format_changed ()
	{
		Config::instance()->set_dcp_asset_filename_format (_dcp_asset_filename_format->get ());
	}

	void log_changed ()
	{
		int types = 0;
		if (_log_general->GetValue ()) {
			types |= LogEntry::TYPE_GENERAL;
		}
		if (_log_warning->GetValue ()) {
			types |= LogEntry::TYPE_WARNING;
		}
		if (_log_error->GetValue ())  {
			types |= LogEntry::TYPE_ERROR;
		}
		if (_log_timing->GetValue ()) {
			types |= LogEntry::TYPE_TIMING;
		}
		if (_log_debug_decode->GetValue ()) {
			types |= LogEntry::TYPE_DEBUG_DECODE;
		}
		if (_log_debug_encode->GetValue ()) {
			types |= LogEntry::TYPE_DEBUG_ENCODE;
		}
		if (_log_debug_email->GetValue ()) {
			types |= LogEntry::TYPE_DEBUG_EMAIL;
		}
		Config::instance()->set_log_types (types);
	}

#ifdef DCPOMATIC_WINDOWS
	void win32_console_changed ()
	{
		Config::instance()->set_win32_console (_win32_console->GetValue ());
	}
#endif

	wxSpinCtrl* _maximum_j2k_bandwidth;
	wxCheckBox* _allow_any_dcp_frame_rate;
	wxCheckBox* _only_servers_encode;
	NameFormatEditor* _dcp_metadata_filename_format;
	NameFormatEditor* _dcp_asset_filename_format;
	wxCheckBox* _log_general;
	wxCheckBox* _log_warning;
	wxCheckBox* _log_error;
	wxCheckBox* _log_timing;
	wxCheckBox* _log_debug_decode;
	wxCheckBox* _log_debug_encode;
	wxCheckBox* _log_debug_email;
#ifdef DCPOMATIC_WINDOWS
	wxCheckBox* _win32_console;
#endif
};

wxPreferencesEditor*
create_config_dialog ()
{
	wxPreferencesEditor* e = new wxPreferencesEditor ();

#ifdef DCPOMATIC_OSX
	/* Width that we force some of the config panels to be on OSX so that
	   the containing window doesn't shrink too much when we select those panels.
	   This is obviously an unpleasant hack.
	*/
	wxSize ps = wxSize (520, -1);
	int const border = 16;
#else
	wxSize ps = wxSize (-1, -1);
	int const border = 8;
#endif

	e->AddPage (new GeneralPage (ps, border));
	e->AddPage (new DefaultsPage (ps, border));
	e->AddPage (new EncodingServersPage (ps, border));
	e->AddPage (new KeysPage (ps, border));
	e->AddPage (new TMSPage (ps, border));
	e->AddPage (new KDMEmailPage (ps, border));
	e->AddPage (new CoverSheetPage (ps, border));
	e->AddPage (new AdvancedPage (ps, border));
	return e;
}
