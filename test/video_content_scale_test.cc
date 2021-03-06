/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

/** @file  test/video_content_scale_test.cc
 *  @brief Test VideoContentScale
 *  @ingroup selfcontained
 */

#include "lib/ffmpeg_content.h"
#include "lib/ratio.h"
#include "lib/video_content.h"
#include <dcp/raw_convert.h>
#include <boost/test/unit_test.hpp>

using std::list;
using std::string;
using std::cerr;
using boost::shared_ptr;
using boost::optional;
using dcp::raw_convert;

static
void
test (dcp::Size content_size, dcp::Size display_size, dcp::Size film_size, Crop crop, Ratio const * ratio, bool scale, dcp::Size correct)
{
	shared_ptr<Film> film;
	string s = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<Content>"
		"<Type>FFmpeg</Type>"
		"<Path>/home/c.hetherington/DCP/prophet_clip.mkv</Path>"
		"<Digest>f3f23663da5bef6d2cbaa0db066f3351314142710</Digest>"
		"<Position>0</Position>"
		"<TrimStart>0</TrimStart>"
		"<TrimEnd>0</TrimEnd>"
		"<VideoLength>2879</VideoLength>"
		"<VideoWidth>" + raw_convert<string>(content_size.width) + "</VideoWidth>"
		"<VideoHeight>" + raw_convert<string>(content_size.height) + "</VideoHeight>"
		"<VideoFrameRate>23.97602462768555</VideoFrameRate>"
		"<OriginalVideoFrameRate>23.97602462768555</OriginalVideoFrameRate>"
		"<VideoFrameType>0</VideoFrameType>"
		"<SampleAspectRatio>1</SampleAspectRatio>"
		"<BitsPerPixel>12</BitsPerPixel>"
		"<LeftCrop>" + raw_convert<string>(crop.left) + "</LeftCrop>"
		"<RightCrop>" + raw_convert<string>(crop.right) + "</RightCrop>"
		"<TopCrop>" + raw_convert<string>(crop.top) + "</TopCrop>"
		"<BottomCrop>" + raw_convert<string>(crop.bottom) + "</BottomCrop>"
		"<Scale>";

	if (ratio) {
		s += "<Ratio>" + ratio->id() + "</Ratio>";
	} else {
		s += "<Scale>" + string(scale ? "1" : "0") + "</Scale>";
	}

	s += "</Scale>"
		"<ColourConversion>"
		"<InputGamma>2.4</InputGamma>"
		"<InputGammaLinearised>1</InputGammaLinearised>"
		"<Matrix i=\"0\" j=\"0\">0.4124564</Matrix>"
		"<Matrix i=\"0\" j=\"1\">0.3575761</Matrix>"
		"<Matrix i=\"0\" j=\"2\">0.1804375</Matrix>"
		"<Matrix i=\"1\" j=\"0\">0.2126729</Matrix>"
		"<Matrix i=\"1\" j=\"1\">0.7151522</Matrix>"
		"<Matrix i=\"1\" j=\"2\">0.072175</Matrix>"
		"<Matrix i=\"2\" j=\"0\">0.0193339</Matrix>"
		"<Matrix i=\"2\" j=\"1\">0.119192</Matrix>"
		"<Matrix i=\"2\" j=\"2\">0.9503041</Matrix>"
		"<OutputGamma>2.6</OutputGamma>"
		"</ColourConversion>"
		"<AudioGain>0</AudioGain>"
		"<AudioDelay>0</AudioDelay>"
		"<SubtitleXOffset>0</SubtitleXOffset>"
		"<SubtitleYOffset>0</SubtitleYOffset>"
		"<SubtitleXScale>0</SubtitleXScale>"
		"<SubtitleYScale>0</SubtitleYScale>"
		"</Content>";

	shared_ptr<cxml::Document> doc (new cxml::Document ());
	doc->read_string (s);

	list<string> notes;
	shared_ptr<FFmpegContent> vc (new FFmpegContent (film, doc, 10, notes));

	optional<VideoContentScale> sc;
	if (ratio) {
		sc = VideoContentScale (ratio);
	} else {
		sc = VideoContentScale (scale);
	}

	dcp::Size answer = sc.get().size (vc->video, display_size, film_size);
	if (answer != correct) {
		cerr << "Testing " << vc->video->size().width << "x" << vc->video->size().height << "\n";
		cerr << "Testing " << display_size.width << "x" << display_size.height << "\n";
		cerr << answer.width << "x" << answer.height << " instead of " << correct.width << "x" << correct.height << "\n";
	}
	BOOST_CHECK (answer == correct);
}

/* Test scale and stretch to specified ratio */
BOOST_AUTO_TEST_CASE (video_content_scale_test_to_ratio)
{
	/* To DCP */

	// Flat in flat container
	test (
		dcp::Size (400, 200),
		dcp::Size (1998, 1080),
		dcp::Size (1998, 1080),
		Crop (0, 0, 0, 0),
		Ratio::from_id ("185"),
		true,
		dcp::Size (1998, 1080)
		);

	// Scope in flat container
	test (
		dcp::Size (400, 200),
		dcp::Size (1998, 1080),
		dcp::Size (1998, 1080),
		Crop (0, 0, 0, 0),
		Ratio::from_id ("239"),
		true,
		dcp::Size (1998, 837)
		);

	// Flat in scope container
	test (
		dcp::Size (400, 200),
		dcp::Size (2048, 858),
		dcp::Size (2048, 858),
		Crop (0, 0, 0, 0),
		Ratio::from_id ("185"),
		true,
		dcp::Size (1587, 858)
		);


	/* To player */

	// Flat in flat container
	test (
		dcp::Size (400, 200),
		dcp::Size (185, 100),
		dcp::Size (1998, 1080),
		Crop (0, 0, 0, 0),
		Ratio::from_id ("185"),
		true,
		dcp::Size (185, 100)
		);

	// Scope in flat container
	test (
		dcp::Size (400, 200),
		dcp::Size (185, 100),
		dcp::Size (1998, 1080),
		Crop (0, 0, 0, 0),
		Ratio::from_id ("239"),
		true,
		dcp::Size (185, 78)
		);

	// Flat in scope container
	test (
		dcp::Size (400, 200),
		dcp::Size (239, 100),
		dcp::Size (2048, 858),
		Crop (0, 0, 0, 0),
		Ratio::from_id ("185"),
		true,
		dcp::Size (185, 100)
		);
}

/* Test no scale */
BOOST_AUTO_TEST_CASE (video_content_scale_no_scale)
{
       /* No scale where the content is bigger than even the film container */
       test (
               dcp::Size (1920, 1080),
               dcp::Size (887, 371),
               dcp::Size (2048, 858),
               Crop (),
               0,
               false,
               dcp::Size (659, 371)
               );
}
