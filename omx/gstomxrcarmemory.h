/*
 * Copyright (C) 2021, Renesas Electronics Corporation
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

#ifndef __GST_OMX_RCAR_MEMORY_H__
#define __GST_OMX_RCAR_MEMORY_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include "gstomx.h"


typedef struct _GstOMXRcarMemory GstOMXRcarMemory;
struct _GstOMXRcarMemory
{
  gint dmabuf_id;
  GstOMXBuffer *omx_buf;
};

GstMemory *
gst_omx_rcar_memory_alloc(GstAllocator *dmabuf_allocator, GstOMXBuffer *omx_buf);

#endif /*__GST_OMX_RCAR_MEMORY_H__ */
