/*
 * Copyright (C) 2017, Renesas Electronics Corporation
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

#include "gstomxwmadec.h"
#ifdef HAVE_AUDIOR_EXT
#include "OMXR_Extension_audio.h"
#endif

GST_DEBUG_CATEGORY_STATIC (gst_omx_wma_dec_debug_category);
#define GST_CAT_DEFAULT gst_omx_wma_dec_debug_category

/* prototypes */
static gboolean gst_omx_wma_dec_set_format (GstOMXAudioDec * dec,
    GstOMXPort * port, GstCaps * caps);
static gboolean gst_omx_wma_dec_is_format_change (GstOMXAudioDec * dec,
    GstOMXPort * port, GstCaps * caps);
static gint gst_omx_wma_dec_get_samples_per_frame (GstOMXAudioDec * dec,
    GstOMXPort * port);
static gboolean gst_omx_wma_dec_get_channel_positions (GstOMXAudioDec * dec,
    GstOMXPort * port, GstAudioChannelPosition position[OMX_AUDIO_MAXCHANNELS]);

/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_wma_dec_debug_category, "omxwmadec", 0, \
      "debug category for gst-omx wma audio decoder");

G_DEFINE_TYPE_WITH_CODE (GstOMXWMADec, gst_omx_wma_dec,
    GST_TYPE_OMX_AUDIO_DEC, DEBUG_INIT);


static void
gst_omx_wma_dec_class_init (GstOMXWMADecClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstOMXAudioDecClass *audiodec_class = GST_OMX_AUDIO_DEC_CLASS (klass);

  audiodec_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_wma_dec_set_format);
  audiodec_class->is_format_change =
      GST_DEBUG_FUNCPTR (gst_omx_wma_dec_is_format_change);
  audiodec_class->get_samples_per_frame =
      GST_DEBUG_FUNCPTR (gst_omx_wma_dec_get_samples_per_frame);
  audiodec_class->get_channel_positions =
      GST_DEBUG_FUNCPTR (gst_omx_wma_dec_get_channel_positions);

  audiodec_class->cdata.default_sink_template_caps = "audio/x-wma, "
      "wmaversion=(int)[1,3],"
      "rate=(int)[8000,48000], "
      "channels=(int)[1,2], "
      "block_align=(int)[0, 2147483647], " "bitrate=(int)[0, 2147483647]";

  gst_element_class_set_static_metadata (element_class,
      "OpenMAX WMA Audio Decoder",
      "Codec/Decoder/Audio",
      "Decode WMA audio streams", "Renesas Electronics Corporation");

  gst_omx_set_default_role (&audiodec_class->cdata, "audio_decoder.wma");
}

static void
gst_omx_wma_dec_init (GstOMXWMADec * self)
{
  self->spf = -1;
}

static gboolean
gst_omx_wma_dec_set_format (GstOMXAudioDec * dec, GstOMXPort * port,
    GstCaps * caps)
{
  GstOMXWMADec *self = GST_OMX_WMA_DEC (dec);
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  OMX_AUDIO_PARAM_WMATYPE wma_param;
  OMX_ERRORTYPE err;
  GstStructure *s;
  gint wmaversion, block_align, bitrate, rate, channels;
  const GValue *codec_data;
  GstBuffer *buf;
  GstMapInfo info;

  gst_omx_port_get_port_definition (port, &port_def);
  port_def.format.audio.eEncoding = OMX_AUDIO_CodingWMA;
  err = gst_omx_port_update_port_definition (port, &port_def);

  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "Failed to set WMA format on component: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  GST_OMX_INIT_STRUCT (&wma_param);
  wma_param.nPortIndex = port->index;

  err =
      gst_omx_component_get_parameter (dec->dec, OMX_IndexParamAudioWma,
      &wma_param);

  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "Failed to get WMA parameters from component: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  s = gst_caps_get_structure (caps, 0);
  if (!gst_structure_get_int (s, "wmaversion", &wmaversion) ||
      !gst_structure_get_int (s, "rate", &rate) ||
      !gst_structure_get_int (s, "channels", &channels) ||
      !gst_structure_get_int (s, "block_align", &block_align) ||
      !gst_structure_get_int (s, "bitrate", &bitrate)) {
    GST_ERROR_OBJECT (self, "Incomplete caps");
    return FALSE;
  }

  wma_param.nChannels = channels;
  wma_param.nBitRate = bitrate;
  /* wma_param.eFormat not supported */
  /* wma_param.eProfile not supported */
  wma_param.nSamplingRate = rate;
  wma_param.nBlockAlign = block_align;

  codec_data = gst_structure_get_value (s, "codec_data");
  if (codec_data) {
    buf = gst_value_get_buffer (codec_data);
    gst_buffer_map (buf, &info, GST_MAP_READ);
    if (info.size >= 10) {
      guint *puint32data;
      guint16 *puint16data;
      guint16 wEncodeOptions;
      guint32 dwSuperBlockAlign;

      puint16data = (guint16 *) (info.data + 4);
      wEncodeOptions = *puint16data;

      puint32data = (guint *) (info.data + 6);
      dwSuperBlockAlign = *puint32data;
      wma_param.nEncodeOptions = wEncodeOptions;
      wma_param.nSuperBlockAlign = dwSuperBlockAlign;
    }
    gst_buffer_unmap (buf, &info);
  }

  err =
      gst_omx_component_set_parameter (dec->dec, OMX_IndexParamAudioWma,
      &wma_param);
  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self, "Error setting WMA parameters: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }
#ifdef HAVE_AUDIOR_EXT
  /* Set output pcm unit */
  {
    OMXR_MC_AUDIO_PARAM_OUTPUTUNITTYPE unit;
    GST_OMX_INIT_STRUCT (&unit);
    unit.nPortIndex = dec->dec_out_port->index;
    unit.eUnit = OMXR_MC_AUDIO_UnitPayload;
    err = gst_omx_component_set_parameter (dec->dec,
        OMXR_MC_IndexParamAudioOutputUnit, &unit);

    if (err != OMX_ErrorNone) {
      GST_ERROR_OBJECT (self, "Error setting Unit parameters: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }
  }
#endif
  return TRUE;
}

static gboolean
gst_omx_wma_dec_is_format_change (GstOMXAudioDec * dec, GstOMXPort * port,
    GstCaps * caps)
{
  GstOMXWMADec *self = GST_OMX_WMA_DEC (dec);
  OMX_AUDIO_PARAM_WMATYPE wma_param;
  OMX_ERRORTYPE err;
  GstStructure *s;
  gint wmaversion, block_align, bitrate, rate, channels;

  GST_OMX_INIT_STRUCT (&wma_param);
  wma_param.nPortIndex = port->index;

  err =
      gst_omx_component_get_parameter (dec->dec, OMX_IndexParamAudioWma,
      &wma_param);
  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "Failed to get WMA parameters from component: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  s = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_int (s, "wmaversion", &wmaversion) ||
      !gst_structure_get_int (s, "rate", &rate) ||
      !gst_structure_get_int (s, "channels", &channels) ||
      !gst_structure_get_int (s, "block_align", &block_align) ||
      !gst_structure_get_int (s, "bitrate", &bitrate)) {
    GST_ERROR_OBJECT (self, "Incomplete caps");
    return FALSE;
  }

  if (wma_param.nChannels != channels)
    return TRUE;

  if (wma_param.nBitRate != bitrate)
    return TRUE;

  if (wma_param.nSamplingRate != rate)
    return TRUE;

  if (wma_param.nBlockAlign != block_align)
    return TRUE;

  return FALSE;
}

static gint
gst_omx_wma_dec_get_samples_per_frame (GstOMXAudioDec * dec, GstOMXPort * port)
{
  return GST_OMX_WMA_DEC (dec)->spf;
}

static gboolean
gst_omx_wma_dec_get_channel_positions (GstOMXAudioDec * dec,
    GstOMXPort * port, GstAudioChannelPosition position[OMX_AUDIO_MAXCHANNELS])
{
  OMX_AUDIO_PARAM_PCMMODETYPE pcm_param;
  OMX_ERRORTYPE err;

  GST_OMX_INIT_STRUCT (&pcm_param);
  pcm_param.nPortIndex = port->index;
  err =
      gst_omx_component_get_parameter (dec->dec, OMX_IndexParamAudioPcm,
      &pcm_param);
  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (dec, "Failed to get PCM parameters: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  switch (pcm_param.nChannels) {
    case 1:
      position[0] = GST_AUDIO_CHANNEL_POSITION_MONO;
      break;
    case 2:
      position[0] = GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT;
      position[1] = GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT;
      break;
    default:
      return FALSE;
  }

  return TRUE;
}
