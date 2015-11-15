/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_DOWNLOAD_CERTIFICATE_PANEL_H
#define DCPOMATIC_DOWNLOAD_CERTIFICATE_PANEL_H

#include <dcp/certificate.h>
#include <wx/wx.h>
#include <boost/optional.hpp>

class DownloadCertificateDialog;

class DownloadCertificatePanel : public wxPanel
{
public:
	DownloadCertificatePanel (wxWindow* parent, DownloadCertificateDialog* dialog);

	/* Do any setup that may take a noticeable amount of time */
	virtual void setup () {}
	virtual bool ready_to_download () const = 0;
	virtual void download (wxStaticText* message) = 0;

	void load (boost::filesystem::path);
	dcp::Certificate certificate () const;

protected:
	void layout ();

	DownloadCertificateDialog* _dialog;
	wxFlexGridSizer* _table;

private:
	wxSizer* _overall_sizer;
	boost::optional<dcp::Certificate> _certificate;
};

#endif
