#include <application/endpoint/rtspclient.hpp>

namespace libwebstreamer
{
    namespace application
    {
        namespace endpoint
        {
            // namespace mediaservice = ::webstreamer::transport::mediaservice;
            using namespace libwebstreamer;

            RtspClient::RtspClient(const std::string &id, const std::string &type,
                                   const std::shared_ptr<libwebstreamer::framework::Pipeline> pipeline_owner)
                : libwebstreamer::framework::Endpoint(id, type, pipeline_owner)
            {
                // initialize(ep);
            }

            void RtspClient::initialize(const std::string &url)
            {
                rtspsrc_ = gst_element_factory_make("rtspsrc", "rtspsrc");

                // configuration::PortRange port_range;
                // webstreamer::configuration::query_rtspsrc_port_range(port_range);
                // g_object_set(G_OBJECT(rtspsrc), "location", url.c_str(),
                //              "port-range", webstreamer::configuration::to_string(port_range), NULL);
                g_object_set(G_OBJECT(rtspsrc_), "location", url.c_str(), NULL);
            }

            void RtspClient::on_rtspsrc_pad_added(GstElement *src, GstPad *src_pad, gpointer rtspclient)
            {
                RtspClient *rtsp_client = static_cast<RtspClient *>(rtspclient);
                GstCaps *caps = gst_pad_query_caps(src_pad, NULL);
                GstStructure *stru = gst_caps_get_structure(caps, 0);
                const GValue *media_type = gst_structure_get_value(stru, "media");

                if (g_str_equal(g_value_get_string(media_type), "video"))
                {
                    GstPad *sink_pad = gst_element_get_static_pad(GST_ELEMENT_CAST(rtsp_client->rtpdepay_video_), "sink");
                    GstPadLinkReturn ret = gst_pad_link(src_pad, sink_pad);
                    g_warn_if_fail(ret == GST_PAD_LINK_OK);
                    gst_object_unref(sink_pad);
                }
                else if (g_str_equal(g_value_get_string(media_type), "audio"))
                {
                    GstPad *sink_pad = gst_element_get_static_pad(GST_ELEMENT_CAST(rtsp_client->rtpaudiodepay_), "sink");
                    GstPadLinkReturn ret = gst_pad_link(src_pad, sink_pad);
                    g_warn_if_fail(ret == GST_PAD_LINK_OK);
                    gst_object_unref(sink_pad);
                }
                else
                {
                    g_warn_if_reached();
                }
            }

            void RtspClient::add_to_pipeline()
            {
                VideoEncodingType video_codec = get_video_encoding_type(pipeline_owner().lock()->video_encoding());
                switch (video_codec)
                {
                    case VideoEncodingType::H264:
                        rtpdepay_video_ = gst_element_factory_make("rtph264depay", "depay");
                        parse_video_ = gst_element_factory_make("h264parse", "parse");
                        break;
                    case VideoEncodingType::H265:
                        rtpdepay_video_ = gst_element_factory_make("rtph265depay", "depay");
                        parse_video_ = gst_element_factory_make("h265parse", "parse");
                        break;
                    default:
                        g_warn_if_reached();
                        g_message("No Video Codec!");
                        //todo...
                        break;
                }
                g_warn_if_fail(rtpdepay_video_ && parse_video_);

                g_warn_if_fail(pipeline_owner().lock() != NULL);
                g_warn_if_fail(pipeline_owner().lock()->pipeline() != NULL);
                gst_bin_add_many(GST_BIN(pipeline_owner().lock()->pipeline()), rtspsrc_, rtpdepay_video_, parse_video_, NULL);
                g_signal_connect(rtspsrc_, "pad-added", (GCallback)on_rtspsrc_pad_added, this);
                g_warn_if_fail(gst_element_link(rtpdepay_video_, parse_video_));

                AudioEncodingType audio_codec = get_audio_encoding_type(pipeline_owner().lock()->audio_encoding());
                switch (audio_codec)
                {
                    case AudioEncodingType::PCMA:
                        rtpaudiodepay_ = gst_element_factory_make("rtppcmadepay", "audio-depay");
                        break;
                    case AudioEncodingType::PCMU:
                        rtpaudiodepay_ = gst_element_factory_make("rtppcmudepay", "audio-depay");
                        break;
                    case VideoEncodingType::H265:
                        rtpaudiodepay_ = gst_element_factory_make("rtpopusdepay", "audio-depay");
                        break;
                    default:
                        g_warn_if_reached();
                        g_message("No Audio Codec!");
                        //todo...
                        break;
                }

#ifdef ENABLE_AUDIO_CODEC
                alawdec = gst_element_factory_make("alawdec", "alawdec");
                g_warn_if_fail(rtpaudiodepay_ && alawdec);
                gst_bin_add_many(GST_BIN(pipeline_owner().lock()->pipeline()), rtpaudiodepay_, alawdec, NULL);
                g_warn_if_fail(gst_element_link(rtpaudiodepay_, alawdec));
#else
                g_warn_if_fail(rtpaudiodepay_);
                gst_bin_add_many(GST_BIN(pipeline_owner().lock()->pipeline()), rtpaudiodepay_, NULL);
#endif
            }

            void RtspClient::remove_from_pipeline()
            {
                gst_bin_remove_many(GST_BIN(pipeline_owner().lock()->pipeline()), rtspsrc_, rtpdepay_video_, parse_video_, NULL);
                gst_bin_remove_many(GST_BIN(pipeline_owner().lock()->pipeline()), rtpaudiodepay_, NULL);
            }
        }
    }
}