/*
 * Copyright (C) 2017-2018,2021 Renesas Electronics Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include "gstomxvp9dec.h"
#ifdef HAVE_VP9DEC_EXT
#include "OMXR_Extension_vp9d.h"
#endif
#ifdef HAVE_VIDEODEC_EXT
#include "OMXR_Extension_vdcmn.h"
#endif

GST_DEBUG_CATEGORY_STATIC (gst_omx_vp9_dec_debug_category);
#define GST_CAT_DEFAULT gst_omx_vp9_dec_debug_category

/* prototypes */
static gboolean gst_omx_vp9_dec_is_format_change (GstOMXVideoDec * dec,
    GstOMXPort * port, GstVideoCodecState * state);
static gboolean gst_omx_vp9_dec_set_format (GstOMXVideoDec * dec,
    GstOMXPort * port, GstVideoCodecState * state);
static gboolean gst_omx_vp9_dec_handle_dynamic_change (GstOMXVideoDec
    * self, GstOMXBuffer * buf);

enum
{
  PROP_0
};

/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_vp9_dec_debug_category, "omxvp9dec", 0, \
      "debug category for gst-omx video decoder base class");

G_DEFINE_TYPE_WITH_CODE (GstOMXVP9Dec, gst_omx_vp9_dec,
    GST_TYPE_OMX_VIDEO_DEC, DEBUG_INIT);

static void
gst_omx_vp9_dec_class_init (GstOMXVP9DecClass * klass)
{
  GstOMXVideoDecClass *videodec_class = GST_OMX_VIDEO_DEC_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  videodec_class->is_format_change =
      GST_DEBUG_FUNCPTR (gst_omx_vp9_dec_is_format_change);
  videodec_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_vp9_dec_set_format);
  videodec_class->handle_dynamic_change =
      GST_DEBUG_FUNCPTR (gst_omx_vp9_dec_handle_dynamic_change);

  videodec_class->cdata.default_sink_template_caps = "video/x-vp9, "
      "width=(int) [1,MAX], " "height=(int) [1,MAX]";

  gst_element_class_set_static_metadata (element_class,
      "OpenMAX VP9 Video Decoder",
      "Codec/Decoder/Video",
      "Decode VP9 video streams", "Renesas Electronics Corporation");

  gst_omx_set_default_role (&videodec_class->cdata, "video_decoder.vp9");
}

static void
gst_omx_vp9_dec_init (GstOMXVP9Dec * self)
{
}

static gboolean
gst_omx_vp9_dec_is_format_change (GstOMXVideoDec * dec,
    GstOMXPort * port, GstVideoCodecState * state)
{
  return FALSE;
}

static gboolean
gst_omx_vp9_dec_set_format (GstOMXVideoDec * dec, GstOMXPort * port,
    GstVideoCodecState * state)
{
  gboolean ret;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;

  gst_omx_port_get_port_definition (port, &port_def);
#ifdef HAVE_VP9DEC_EXT
  port_def.format.video.eCompressionFormat = OMXR_MC_VIDEO_CodingVP9;
#endif
  ret = gst_omx_port_update_port_definition (port, &port_def) == OMX_ErrorNone;

  return ret;
}

static gboolean
gst_omx_vp9_dec_handle_dynamic_change (GstOMXVideoDec * dec, GstOMXBuffer * buf)
{
#ifdef HAVE_VIDEODEC_EXT
  OMXR_MC_VIDEO_DECODERESULTTYPE *decode_res =
      (OMXR_MC_VIDEO_DECODERESULTTYPE *) buf->omx_buf->pOutputPortPrivate;

  if (decode_res) {
    dec->dynamic_width = (guint) decode_res->u32PictWidth;

    dec->dynamic_height = (guint) decode_res->u32PictHeight;

  } else {
    /* Keep caps as current caps */
    GstVideoCodecState *state;
    state = gst_video_decoder_get_output_state (GST_VIDEO_DECODER (dec));
    dec->dynamic_width = state->info.width;
    dec->dynamic_height = state->info.height;
    gst_video_codec_state_unref (state);
  }
#endif
  return GST_OMX_VIDEO_DEC_CLASS
      (gst_omx_vp9_dec_parent_class)->handle_dynamic_change (dec, buf);
}
