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

#include "gstomxvp8enc.h"
#ifdef HAVE_VP8ENC_EXT
#include "OMXR_Extension_vp8e.h"
#endif

GST_DEBUG_CATEGORY_STATIC (gst_omx_vp8_enc_debug_category);
#define GST_CAT_DEFAULT gst_omx_vp8_enc_debug_category

/* prototypes */
static gboolean gst_omx_vp8_enc_set_format (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);
static GstCaps *gst_omx_vp8_enc_get_caps (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);
static GstFlowReturn gst_omx_vp8_enc_handle_output_frame (GstOMXVideoEnc *
    self, GstOMXPort * port, GstOMXBuffer * buf, GstVideoCodecFrame * frame);
static void gst_omx_vp8_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_vp8_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

enum
{
  PROP_0,
#ifdef HAVE_VP8ENC_EXT
  PROP_INTERVALOFCODINGINTRAFRAMES,
  PROP_SHARPNESS,
#endif
  PROP_DCTPARTITIONS
};

#ifdef HAVE_VP8ENC_EXT
#define GST_OMX_VP8_VIDEO_ENC_INTERVAL_OF_CODING_INTRA_FRAMES_DEFAULT (0xffffffff)
#define GST_OMX_VP8_VIDEO_ENC_SHARPNESS_DEFAULT 0
#endif
#define GST_OMX_VP8_VIDEO_ENC_DCT_PARTITIONS_DEFAULT 0

/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_vp8_enc_debug_category, "omxvp8enc", 0, \
      "debug category for gst-omx video encoder base class");

#define parent_class gst_omx_vp8_enc_parent_class
G_DEFINE_TYPE_WITH_CODE (GstOMXVP8Enc, gst_omx_vp8_enc,
    GST_TYPE_OMX_VIDEO_ENC, DEBUG_INIT);

static void
gst_omx_vp8_enc_class_init (GstOMXVP8EncClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstOMXVideoEncClass *videoenc_class = GST_OMX_VIDEO_ENC_CLASS (klass);

  videoenc_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_vp8_enc_set_format);
  videoenc_class->get_caps = GST_DEBUG_FUNCPTR (gst_omx_vp8_enc_get_caps);

  gobject_class->set_property = gst_omx_vp8_enc_set_property;
  gobject_class->get_property = gst_omx_vp8_enc_get_property;
#ifdef HAVE_VP8ENC_EXT
  g_object_class_install_property (gobject_class,
      PROP_INTERVALOFCODINGINTRAFRAMES,
      g_param_spec_uint ("interval-intraframes",
          "Interval of coding Intra frames",
          "Interval of coding Intra frames (0xffffffff=component default)", 0,
          G_MAXUINT,
          GST_OMX_VP8_VIDEO_ENC_INTERVAL_OF_CODING_INTRA_FRAMES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_SHARPNESS,
      g_param_spec_uint ("sharpness", "Control the blocking filter",
          "Control the blocking filter", 0,
          7, GST_OMX_VP8_VIDEO_ENC_SHARPNESS_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
#endif
  g_object_class_install_property (gobject_class, PROP_DCTPARTITIONS,
      g_param_spec_uint ("dct-partitions", "DCT residual data partition",
          "DCT residual data partition", 0, 1,
          GST_OMX_VP8_VIDEO_ENC_DCT_PARTITIONS_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  videoenc_class->cdata.default_src_template_caps = "video/x-vp8, "
      "width=(int) [ 80, 1920 ], " "height=(int) [ 80, 1080 ]";
  videoenc_class->handle_output_frame =
      GST_DEBUG_FUNCPTR (gst_omx_vp8_enc_handle_output_frame);

  gst_element_class_set_static_metadata (element_class,
      "OpenMAX VP8 Video Encoder",
      "Codec/Encoder/Video",
      "Encode VP8 video streams", "Renesas Electronics Corporation");

  gst_omx_set_default_role (&videoenc_class->cdata, "video_encoder.vp8");
}

static void
gst_omx_vp8_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOMXVP8Enc *self = GST_OMX_VP8_ENC (object);

  switch (prop_id) {
#ifdef HAVE_VP8ENC_EXT
    case PROP_INTERVALOFCODINGINTRAFRAMES:
      self->interval_intraframes = g_value_get_uint (value);
      break;
    case PROP_SHARPNESS:
      self->sharpness = g_value_get_uint (value);
      break;
#endif
    case PROP_DCTPARTITIONS:
      self->dct_partitions = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_vp8_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOMXVP8Enc *self = GST_OMX_VP8_ENC (object);

  switch (prop_id) {
#ifdef HAVE_VP8ENC_EXT
    case PROP_INTERVALOFCODINGINTRAFRAMES:
      g_value_set_uint (value, self->interval_intraframes);
      break;
    case PROP_SHARPNESS:
      g_value_set_uint (value, self->sharpness);
      break;
#endif
    case PROP_DCTPARTITIONS:
      g_value_set_uint (value, self->dct_partitions);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_vp8_enc_init (GstOMXVP8Enc * self)
{
#ifdef HAVE_VP8ENC_EXT
  self->interval_intraframes =
      GST_OMX_VP8_VIDEO_ENC_INTERVAL_OF_CODING_INTRA_FRAMES_DEFAULT;
  self->sharpness = GST_OMX_VP8_VIDEO_ENC_SHARPNESS_DEFAULT;
#endif
  self->dct_partitions = GST_OMX_VP8_VIDEO_ENC_DCT_PARTITIONS_DEFAULT;
}

static gboolean
gst_omx_vp8_enc_set_format (GstOMXVideoEnc * enc, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstOMXVP8Enc *self = GST_OMX_VP8_ENC (enc);
  GstCaps *peercaps;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  OMX_VIDEO_PARAM_PROFILELEVELTYPE param_profilelevel;
  OMX_VIDEO_PARAM_VP8TYPE param_type;
  OMX_ERRORTYPE err;
  const gchar *profile_string, *level_string;

#ifdef HAVE_VP8ENC_EXT
  if ((self->interval_intraframes !=
          GST_OMX_VP8_VIDEO_ENC_INTERVAL_OF_CODING_INTRA_FRAMES_DEFAULT) ||
      (self->sharpness != GST_OMX_VP8_VIDEO_ENC_SHARPNESS_DEFAULT)) {

    OMXR_MC_VIDEO_PARAM_VP8_MISCELLANEOUS param_miscellaneous;

    GST_OMX_INIT_STRUCT (&param_miscellaneous);
    param_miscellaneous.nPortIndex =
        GST_OMX_VIDEO_ENC (self)->enc_out_port->index;
    err =
        gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
        OMXR_MC_IndexParamVideoVP8Miscellaneous, &param_miscellaneous);

    if (err != OMX_ErrorNone) {
      GST_ERROR_OBJECT (self,
          "can't get OMXR_MC_IndexParamVideoVP8Miscellaneous %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }

    GST_DEBUG_OBJECT (self, "default nPFrames:%u nSharpness:%u",
        (guint) param_miscellaneous.nPFrames,
        (guint) param_miscellaneous.nSharpness);

    if (self->interval_intraframes !=
        GST_OMX_VP8_VIDEO_ENC_INTERVAL_OF_CODING_INTRA_FRAMES_DEFAULT)
      param_miscellaneous.nPFrames = self->interval_intraframes;
    if (self->sharpness != GST_OMX_VP8_VIDEO_ENC_SHARPNESS_DEFAULT)
      param_miscellaneous.nSharpness = self->sharpness;

    err =
        gst_omx_component_set_parameter (GST_OMX_VIDEO_ENC (self)->enc,
        OMXR_MC_IndexParamVideoVP8Miscellaneous, &param_miscellaneous);

    if (err != OMX_ErrorNone) {
      GST_ERROR_OBJECT (self,
          "can't set OMXR_MC_IndexParamVideoVP8Miscellaneous %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }

    err =
        gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
        OMXR_MC_IndexParamVideoVP8Miscellaneous, &param_miscellaneous);

    if (err == OMX_ErrorNone)
      GST_INFO_OBJECT (self, "updated nPFrames:%u nSharpness:%u",
          (guint) param_miscellaneous.nPFrames,
          (guint) param_miscellaneous.nSharpness);
    else
      GST_INFO_OBJECT (self,
          "Can't get updated OMXR_MC_IndexParamVideoVP8Miscellaneous %s (0x%08x)",
          gst_omx_error_to_string (err), err);
  }
#endif

  if (self->dct_partitions != GST_OMX_VP8_VIDEO_ENC_DCT_PARTITIONS_DEFAULT) {

    GST_OMX_INIT_STRUCT (&param_type);
    param_type.nPortIndex = GST_OMX_VIDEO_ENC (self)->enc_out_port->index;
    err =
        gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
        OMX_IndexParamVideoVp8, &param_type);

    if (err != OMX_ErrorNone) {
      GST_ERROR_OBJECT (self, "can't get OMX_IndexParamVideoVp8 %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }

    GST_DEBUG_OBJECT (self, "default nDCTPartitions:%u",
        (guint) param_type.nDCTPartitions);

    param_type.nDCTPartitions = self->dct_partitions;

    err =
        gst_omx_component_set_parameter (GST_OMX_VIDEO_ENC (self)->enc,
        OMX_IndexParamVideoVp8, &param_type);

    if (err != OMX_ErrorNone) {
      GST_ERROR_OBJECT (self, "can't set OMX_IndexParamVideoVp8 %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }

    err =
        gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
        OMX_IndexParamVideoVp8, &param_type);

    if (err == OMX_ErrorNone)
      GST_INFO_OBJECT (self, "updated nDCTPartitions:%u",
          (guint) param_type.nDCTPartitions);
    else
      GST_INFO_OBJECT (self,
          "Can't get updated OMX_IndexParamVideoVp8 %s (0x%08x)",
          gst_omx_error_to_string (err), err);
  }

  gst_omx_port_get_port_definition (GST_OMX_VIDEO_ENC (self)->enc_out_port,
      &port_def);
  port_def.format.video.eCompressionFormat = OMX_VIDEO_CodingVP8;
  err =
      gst_omx_port_update_port_definition (GST_OMX_VIDEO_ENC
      (self)->enc_out_port, &port_def);
  if (err != OMX_ErrorNone)
    return FALSE;

  GST_OMX_INIT_STRUCT (&param_profilelevel);
  param_profilelevel.nPortIndex = GST_OMX_VIDEO_ENC (self)->enc_out_port->index;

  err =
      gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
      OMX_IndexParamVideoProfileLevelCurrent, &param_profilelevel);
  if (err != OMX_ErrorNone) {
    GST_WARNING_OBJECT (self,
        "Setting profile/level not supported by component");
    return TRUE;
  }

  peercaps = gst_pad_peer_query_caps (GST_VIDEO_ENCODER_SRC_PAD (enc),
      gst_pad_get_pad_template_caps (GST_VIDEO_ENCODER_SRC_PAD (enc)));
  if (peercaps) {
    GstStructure *s;

    if (gst_caps_is_empty (peercaps)) {
      gst_caps_unref (peercaps);
      GST_ERROR_OBJECT (self, "Empty caps");
      return FALSE;
    }

    s = gst_caps_get_structure (peercaps, 0);
    profile_string = gst_structure_get_string (s, "profile");
    if (profile_string) {
      if (g_str_equal (profile_string, "main")) {
        param_profilelevel.eProfile = OMX_VIDEO_VP8ProfileMain;
      } else {
        goto unsupported_profile;
      }
    }
    level_string = gst_structure_get_string (s, "level");
    if (level_string) {
      if (g_str_equal (level_string, "0")) {
        param_profilelevel.eLevel = OMX_VIDEO_VP8Level_Version0;
      } else if (g_str_equal (level_string, "1")) {
        param_profilelevel.eLevel = OMX_VIDEO_VP8Level_Version1;
      } else if (g_str_equal (level_string, "2")) {
        param_profilelevel.eLevel = OMX_VIDEO_VP8Level_Version2;
      } else if (g_str_equal (level_string, "3")) {
        param_profilelevel.eLevel = OMX_VIDEO_VP8Level_Version3;
      } else {
        goto unsupported_level;
      }
    }
    gst_caps_unref (peercaps);
  }

  err =
      gst_omx_component_set_parameter (GST_OMX_VIDEO_ENC (self)->enc,
      OMX_IndexParamVideoProfileLevelCurrent, &param_profilelevel);
  if (err == OMX_ErrorUnsupportedIndex) {
    GST_WARNING_OBJECT (self,
        "Setting profile/level not supported by component");
  } else if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "Error setting profile %u and level %u: %s (0x%08x)",
        (guint) param_profilelevel.eProfile, (guint) param_profilelevel.eLevel,
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  return TRUE;

unsupported_profile:
  GST_ERROR_OBJECT (self, "Unsupported profile %s", profile_string);
  gst_caps_unref (peercaps);
  return FALSE;

unsupported_level:
  GST_ERROR_OBJECT (self, "Unsupported level %s", level_string);
  gst_caps_unref (peercaps);
  return FALSE;
}

static GstCaps *
gst_omx_vp8_enc_get_caps (GstOMXVideoEnc * enc, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstOMXVP8Enc *self = GST_OMX_VP8_ENC (enc);
  GstCaps *caps;
  OMX_ERRORTYPE err;
  OMX_VIDEO_PARAM_PROFILELEVELTYPE param_profilelevel;
  const gchar *profile, *level;

  caps = gst_caps_new_simple ("video/x-vp8", NULL, NULL);

  GST_OMX_INIT_STRUCT (&param_profilelevel);
  param_profilelevel.nPortIndex = GST_OMX_VIDEO_ENC (self)->enc_out_port->index;

  err =
      gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
      OMX_IndexParamVideoProfileLevelCurrent, &param_profilelevel);
  if (err != OMX_ErrorNone && err != OMX_ErrorUnsupportedIndex)
    return NULL;

  if (err == OMX_ErrorNone) {
    switch (param_profilelevel.eProfile) {
      case OMX_VIDEO_VP8ProfileMain:
        profile = "main";
        break;
      default:
        g_assert_not_reached ();
        return NULL;
    }

    switch (param_profilelevel.eLevel) {
      case OMX_VIDEO_VP8Level_Version0:
        level = "0";
        break;
      case OMX_VIDEO_VP8Level_Version1:
        level = "1";
        break;
      case OMX_VIDEO_VP8Level_Version2:
        level = "2";
        break;
      case OMX_VIDEO_VP8Level_Version3:
        level = "3";
        break;
      default:
        g_assert_not_reached ();
        return NULL;
    }
    gst_caps_set_simple (caps,
        "profile", G_TYPE_STRING, profile, "level", G_TYPE_STRING, level, NULL);
  }

  return caps;
}

static GstFlowReturn
gst_omx_vp8_enc_handle_output_frame (GstOMXVideoEnc * enc, GstOMXPort * port,
    GstOMXBuffer * buf, GstVideoCodecFrame * frame)
{
  return
      GST_OMX_VIDEO_ENC_CLASS
      (gst_omx_vp8_enc_parent_class)->handle_output_frame (enc, port, buf,
      frame);
}
