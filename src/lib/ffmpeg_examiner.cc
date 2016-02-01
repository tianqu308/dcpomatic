/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
}
#include "ffmpeg_examiner.h"
#include "ffmpeg_content.h"
#include "job.h"
#include "ffmpeg_audio_stream.h"
#include "ffmpeg_subtitle_stream.h"
#include "util.h"
#include "safe_stringstream.h"
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using std::max;
using boost::shared_ptr;
using boost::optional;

/** @param job job that the examiner is operating in, or 0 */
FFmpegExaminer::FFmpegExaminer (shared_ptr<const FFmpegContent> c, shared_ptr<Job> job)
	: FFmpeg (c)
	, _video_length (0)
	, _need_video_length (false)
{
	/* Find audio and subtitle streams */

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		AVStream* s = _format_context->streams[i];
		if (s->codec->codec_type == AVMEDIA_TYPE_AUDIO) {

			/* This is a hack; sometimes it seems that _audio_codec_context->channel_layout isn't set up,
			   so bodge it here.  No idea why we should have to do this.
			*/

			if (s->codec->channel_layout == 0) {
				s->codec->channel_layout = av_get_default_channel_layout (s->codec->channels);
			}

			_audio_streams.push_back (
				shared_ptr<FFmpegAudioStream> (
					new FFmpegAudioStream (audio_stream_name (s), s->id, s->codec->sample_rate, s->codec->channels)
					)
				);

		} else if (s->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
			_subtitle_streams.push_back (shared_ptr<FFmpegSubtitleStream> (new FFmpegSubtitleStream (subtitle_stream_name (s), s->id)));
		}
	}

	/* See if the header has duration information in it */
	_need_video_length = _format_context->duration == AV_NOPTS_VALUE;
	if (!_need_video_length) {
		_video_length = (double (_format_context->duration) / AV_TIME_BASE) * video_frame_rate().get ();
	}

	if (job) {
		if (_need_video_length) {
			job->sub (_("Finding length and subtitles"));
		} else {
			job->sub (_("Finding subtitles"));
		}
	}

	/* Run through until we find:
	 *   - the first video.
	 *   - the first audio for each stream.
	 *   - the subtitle periods for each stream.
	 *
	 * We have to note subtitle periods as otherwise we have no way of knowing
	 * where we should look for subtitles (video and audio are always present,
	 * so they are ok).
	 */

	int64_t const len = _file_group.length ();
	while (true) {
		int r = av_read_frame (_format_context, &_packet);
		if (r < 0) {
			break;
		}

		if (job) {
			if (len > 0) {
				job->set_progress (float (_format_context->pb->pos) / len);
			} else {
				job->set_progress_unknown ();
			}
		}

		AVCodecContext* context = _format_context->streams[_packet.stream_index]->codec;

		if (_packet.stream_index == _video_stream) {
			video_packet (context);
		}

		bool got_all_audio = true;

		for (size_t i = 0; i < _audio_streams.size(); ++i) {
			if (_audio_streams[i]->uses_index (_format_context, _packet.stream_index)) {
				audio_packet (context, _audio_streams[i]);
			}
			if (!_audio_streams[i]->first_audio) {
				got_all_audio = false;
			}
		}

		for (size_t i = 0; i < _subtitle_streams.size(); ++i) {
			if (_subtitle_streams[i]->uses_index (_format_context, _packet.stream_index)) {
				subtitle_packet (context, _subtitle_streams[i]);
			}
		}

		av_free_packet (&_packet);

		if (_first_video && got_all_audio && _subtitle_streams.empty ()) {
			/* All done */
			break;
		}
	}

	/* Finish off any hanging subtitles at the end */
	for (LastSubtitleMap::const_iterator i = _last_subtitle_start.begin(); i != _last_subtitle_start.end(); ++i) {
		if (i->second) {
			i->first->add_subtitle (
				i->second->id,
				ContentTimePeriod (
					i->second->time,
					ContentTime::from_frames (video_length(), video_frame_rate().get_value_or (24))
					)
				);
		}
	}

	/* We just added subtitles to our streams without taking the PTS offset into account;
	   this is because we might not know the PTS offset when the first subtitle is seen.
	   Now we know the PTS offset so we can apply it to those subtitles.
	*/
	if (video_frame_rate()) {
		BOOST_FOREACH (shared_ptr<FFmpegSubtitleStream> i, _subtitle_streams) {
			i->add_offset (pts_offset (_audio_streams, _first_video, video_frame_rate().get()));
		}
	}
}

void
FFmpegExaminer::video_packet (AVCodecContext* context)
{
	if (_first_video && !_need_video_length) {
		return;
	}

	int frame_finished;
	if (avcodec_decode_video2 (context, _frame, &frame_finished, &_packet) >= 0 && frame_finished) {
		if (!_first_video) {
			_first_video = frame_time (_format_context->streams[_video_stream]);
		}
		if (_need_video_length) {
			_video_length = frame_time (
				_format_context->streams[_video_stream]
				).get_value_or (ContentTime ()).frames_round (video_frame_rate().get ());
		}
	}
}

void
FFmpegExaminer::audio_packet (AVCodecContext* context, shared_ptr<FFmpegAudioStream> stream)
{
	if (stream->first_audio) {
		return;
	}

	int frame_finished;
	if (avcodec_decode_audio4 (context, _frame, &frame_finished, &_packet) >= 0 && frame_finished) {
		stream->first_audio = frame_time (stream->stream (_format_context));
	}
}

void
FFmpegExaminer::subtitle_packet (AVCodecContext* context, shared_ptr<FFmpegSubtitleStream> stream)
{
	int frame_finished;
	AVSubtitle sub;
	if (avcodec_decode_subtitle2 (context, &sub, &frame_finished, &_packet) >= 0 && frame_finished) {
		string id = subtitle_id (sub);
		FFmpegSubtitlePeriod const period = subtitle_period (sub);
		LastSubtitleMap::iterator last = _last_subtitle_start.find (stream);
		if (last != _last_subtitle_start.end() && last->second) {
			/* We have seen the start of a subtitle but not yet the end.  Whatever this is
			   finishes the previous subtitle, so add it */
			stream->add_subtitle (last->second->id, ContentTimePeriod (last->second->time, period.from));
			if (sub.num_rects == 0) {
				/* This is a `proper' end-of-subtitle */
				_last_subtitle_start[stream] = optional<SubtitleStart> ();
			} else {
				/* This is just another subtitle, so we start again */
				_last_subtitle_start[stream] = SubtitleStart (id, period.from);
			}
		} else if (sub.num_rects == 1) {
			if (period.to) {
				stream->add_subtitle (id, ContentTimePeriod (period.from, period.to.get ()));
			} else {
				_last_subtitle_start[stream] = SubtitleStart (id, period.from);
			}
		}
		avsubtitle_free (&sub);
	}
}

optional<ContentTime>
FFmpegExaminer::frame_time (AVStream* s) const
{
	optional<ContentTime> t;

	int64_t const bet = av_frame_get_best_effort_timestamp (_frame);
	if (bet != AV_NOPTS_VALUE) {
		t = ContentTime::from_seconds (bet * av_q2d (s->time_base));
	}

	return t;
}

optional<double>
FFmpegExaminer::video_frame_rate () const
{
	/* This use of r_frame_rate is debateable; there's a few different
	 * frame rates in the format context, but this one seems to be the most
	 * reliable.
	 */
	return av_q2d (av_stream_get_r_frame_rate (_format_context->streams[_video_stream]));
}

dcp::Size
FFmpegExaminer::video_size () const
{
	return dcp::Size (video_codec_context()->width, video_codec_context()->height);
}

/** @return Length according to our content's header */
Frame
FFmpegExaminer::video_length () const
{
	return max (Frame (1), _video_length);
}

optional<double>
FFmpegExaminer::sample_aspect_ratio () const
{
	AVRational sar = av_guess_sample_aspect_ratio (_format_context, _format_context->streams[_video_stream], 0);
	if (sar.num == 0) {
		/* I assume this means that we don't know */
		return optional<double> ();
	}
	return double (sar.num) / sar.den;
}

string
FFmpegExaminer::audio_stream_name (AVStream* s) const
{
	SafeStringStream n;

	n << stream_name (s);

	if (!n.str().empty()) {
		n << "; ";
	}

	n << s->codec->channels << " channels";

	return n.str ();
}

string
FFmpegExaminer::subtitle_stream_name (AVStream* s) const
{
	SafeStringStream n;

	n << stream_name (s);

	if (n.str().empty()) {
		n << _("unknown");
	}

	return n.str ();
}

string
FFmpegExaminer::stream_name (AVStream* s) const
{
	SafeStringStream n;

	if (s->metadata) {
		AVDictionaryEntry const * lang = av_dict_get (s->metadata, "language", 0, 0);
		if (lang) {
			n << lang->value;
		}

		AVDictionaryEntry const * title = av_dict_get (s->metadata, "title", 0, 0);
		if (title) {
			if (!n.str().empty()) {
				n << " ";
			}
			n << title->value;
		}
	}

	return n.str ();
}

int
FFmpegExaminer::bits_per_pixel () const
{
	return av_get_bits_per_pixel (av_pix_fmt_desc_get (video_codec_context()->pix_fmt));
}

bool
FFmpegExaminer::yuv () const
{
	switch (video_codec_context()->pix_fmt) {
	case AV_PIX_FMT_YUV420P:
	case AV_PIX_FMT_YUYV422:
	case AV_PIX_FMT_YUV422P:
	case AV_PIX_FMT_YUV444P:
	case AV_PIX_FMT_YUV410P:
	case AV_PIX_FMT_YUV411P:
	case AV_PIX_FMT_YUVJ420P:
	case AV_PIX_FMT_YUVJ422P:
	case AV_PIX_FMT_YUVJ444P:
	case AV_PIX_FMT_UYVY422:
	case AV_PIX_FMT_UYYVYY411:
	case AV_PIX_FMT_NV12:
	case AV_PIX_FMT_NV21:
	case AV_PIX_FMT_YUV440P:
	case AV_PIX_FMT_YUVJ440P:
	case AV_PIX_FMT_YUVA420P:
	case AV_PIX_FMT_YUV420P16LE:
	case AV_PIX_FMT_YUV420P16BE:
	case AV_PIX_FMT_YUV422P16LE:
	case AV_PIX_FMT_YUV422P16BE:
	case AV_PIX_FMT_YUV444P16LE:
	case AV_PIX_FMT_YUV444P16BE:
	case AV_PIX_FMT_YUV420P9BE:
	case AV_PIX_FMT_YUV420P9LE:
	case AV_PIX_FMT_YUV420P10BE:
	case AV_PIX_FMT_YUV420P10LE:
	case AV_PIX_FMT_YUV422P10BE:
	case AV_PIX_FMT_YUV422P10LE:
	case AV_PIX_FMT_YUV444P9BE:
	case AV_PIX_FMT_YUV444P9LE:
	case AV_PIX_FMT_YUV444P10BE:
	case AV_PIX_FMT_YUV444P10LE:
	case AV_PIX_FMT_YUV422P9BE:
	case AV_PIX_FMT_YUV422P9LE:
	case AV_PIX_FMT_YUVA422P_LIBAV:
	case AV_PIX_FMT_YUVA444P_LIBAV:
	case AV_PIX_FMT_YUVA420P9BE:
	case AV_PIX_FMT_YUVA420P9LE:
	case AV_PIX_FMT_YUVA422P9BE:
	case AV_PIX_FMT_YUVA422P9LE:
	case AV_PIX_FMT_YUVA444P9BE:
	case AV_PIX_FMT_YUVA444P9LE:
	case AV_PIX_FMT_YUVA420P10BE:
	case AV_PIX_FMT_YUVA420P10LE:
	case AV_PIX_FMT_YUVA422P10BE:
	case AV_PIX_FMT_YUVA422P10LE:
	case AV_PIX_FMT_YUVA444P10BE:
	case AV_PIX_FMT_YUVA444P10LE:
	case AV_PIX_FMT_YUVA420P16BE:
	case AV_PIX_FMT_YUVA420P16LE:
	case AV_PIX_FMT_YUVA422P16BE:
	case AV_PIX_FMT_YUVA422P16LE:
	case AV_PIX_FMT_YUVA444P16BE:
	case AV_PIX_FMT_YUVA444P16LE:
	case AV_PIX_FMT_NV16:
	case AV_PIX_FMT_NV20LE:
	case AV_PIX_FMT_NV20BE:
	case AV_PIX_FMT_YVYU422:
	case AV_PIX_FMT_YUVA444P:
	case AV_PIX_FMT_YUVA422P:
	case AV_PIX_FMT_YUV420P12BE:
	case AV_PIX_FMT_YUV420P12LE:
	case AV_PIX_FMT_YUV420P14BE:
	case AV_PIX_FMT_YUV420P14LE:
	case AV_PIX_FMT_YUV422P12BE:
	case AV_PIX_FMT_YUV422P12LE:
	case AV_PIX_FMT_YUV422P14BE:
	case AV_PIX_FMT_YUV422P14LE:
	case AV_PIX_FMT_YUV444P12BE:
	case AV_PIX_FMT_YUV444P12LE:
	case AV_PIX_FMT_YUV444P14BE:
	case AV_PIX_FMT_YUV444P14LE:
	case AV_PIX_FMT_YUVJ411P:
		return true;
	default:
		return false;
	}
}
