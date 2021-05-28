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
#include "gstomxrcarmemory.h"
#include "gstomx.h"
#include "mmngr_buf_user_public.h"
#include "OMXR_Extension_vdcmn.h"

#include "gstomxallocator.h"
#include <gst/allocators/gstdmabuf.h>

#include <unistd.h>

#define GST_OMX_RCAR_MEMORY_QUARK gst_omx_rcar_memory_quark ()
GQuark
gst_omx_rcar_memory_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_static_string ("GstOMXRcarMemory");

  return quark;
}

static void
gst_omx_rcar_mem_export_destroy (GstOMXRcarMemory * rcar_mem)
{
  mmngr_export_end_in_user_ext (rcar_mem->dmabuf_id);
  g_slice_free (GstOMXRcarMemory, rcar_mem);
}

GstMemory *
gst_omx_rcar_memory_alloc (GstAllocator * dmabuf_allocator,
    GstOMXBuffer * omx_buf)
{
  GstOMXRcarMemory *rcar_mem;
  GstMemory *mem;
  gint dmabuf_id;
  gint dmabuf_fd;

  long page_size = sysconf (_SC_PAGESIZE);
  gsize size = GST_ROUND_UP_N (omx_buf->omx_buf->nAllocLen, page_size);

  OMXR_MC_VIDEO_DECODERESULTTYPE *decode_res =
      (OMXR_MC_VIDEO_DECODERESULTTYPE *) omx_buf->omx_buf->pOutputPortPrivate;

  guint phys_addr = (guintptr) decode_res->pvPhysImageAddressY;
  gint res = mmngr_export_start_in_user_ext (&dmabuf_id,
      size, phys_addr, &dmabuf_fd, NULL);

  if (res != R_MM_OK) {
    GST_ERROR_OBJECT (dmabuf_allocator,
        "mmngr_export_start_in_user failed (phys_addr:0x%08x)", phys_addr);
    return NULL;
  }

  GST_DEBUG_OBJECT (dmabuf_allocator,
      "Export dmabuf:%d id_export:%d (phys_addr:0x%08x)", dmabuf_fd,
      dmabuf_id, phys_addr);

  mem = gst_dmabuf_allocator_alloc (dmabuf_allocator, dmabuf_fd, size);

  if (!mem) {
    mmngr_export_end_in_user_ext (dmabuf_id);
    return NULL;
  }

  rcar_mem = g_slice_new0 (GstOMXRcarMemory);
  rcar_mem->dmabuf_id = dmabuf_id;
  rcar_mem->omx_buf = omx_buf;

  gst_mini_object_set_qdata (GST_MINI_OBJECT (mem),
      GST_OMX_RCAR_MEMORY_QUARK, rcar_mem,
      (GDestroyNotify) gst_omx_rcar_mem_export_destroy);

  return mem;
}

static void
gst_omx_rcar_mem_import_destroy (GstOMXRcarMemory * rcar_mem)
{
  mmngr_import_end_in_user_ext (rcar_mem->dmabuf_id);
  g_slice_free (GstOMXRcarMemory, rcar_mem);
}

static gboolean
gst_omx_rcar_import_dmabuf (GstOMXBuffer * buf, GstBuffer * input_buffer)
{
  guint n_mem;
  gint i;
  gint ret;
  OMXR_MC_VIDEO_EXTEND_ADDRESSTYPE *ext_addr;

  ext_addr = (OMXR_MC_VIDEO_EXTEND_ADDRESSTYPE *) buf->omx_buf->pBuffer;

  GstVideoMeta *vmeta = gst_buffer_get_video_meta (input_buffer);
  if (vmeta)
    n_mem = vmeta->n_planes;
  else
    n_mem = gst_buffer_n_memory (input_buffer);


  for (i = 0; i < n_mem; i++) {
    GstOMXRcarMemory *rcar_mem;
    GstMemory *mem;
    gint fd;
    gint dmabuf_id;
    guint phys_addr;
    gsize skip;
    gsize size;
    guint mem_idx, length;

    if (vmeta) {
      guint offset = vmeta->offset[i];
      if (!gst_buffer_find_memory (input_buffer, offset, 1, &mem_idx, &length,
              &skip)) {
        return FALSE;
      }
    } else {
      mem_idx = i;
      skip = 0;
    }

    mem = gst_buffer_peek_memory (input_buffer, mem_idx);
    fd = gst_dmabuf_memory_get_fd (mem);

    ret =
        mmngr_import_start_in_user_ext (&dmabuf_id, &size, &phys_addr, fd,
        NULL);
    if (ret != R_MM_OK) {
      GST_ERROR ("Failed to mmngr_import_dmabuf");
      return FALSE;
    }

    rcar_mem = g_slice_new0 (GstOMXRcarMemory);
    rcar_mem->dmabuf_id = dmabuf_id;
    rcar_mem->omx_buf = buf;
    ext_addr->u32HwipAddr[i] = phys_addr + mem->offset + skip;

    gst_mini_object_set_qdata (GST_MINI_OBJECT (mem),
        GST_OMX_RCAR_MEMORY_QUARK, rcar_mem,
        (GDestroyNotify) gst_omx_rcar_mem_import_destroy);
  }

  return TRUE;
}

gboolean
gst_omx_rcar_compare_buffers (GstOMXBuffer * buf, GstBuffer * input_buffer)
{
  GstMemory *mem;
  GstOMXRcarMemory *rcar_mem;
  mem = gst_buffer_peek_memory (input_buffer, 0);
  rcar_mem =
      gst_mini_object_get_qdata (GST_MINI_OBJECT (mem),
      GST_OMX_RCAR_MEMORY_QUARK);

  if (!rcar_mem) {
    if (!gst_omx_rcar_import_dmabuf (buf, input_buffer)) {
      GST_ERROR ("R-Car dmabuf import failed");
      return FALSE;
    }
    rcar_mem =
        gst_mini_object_get_qdata (GST_MINI_OBJECT (mem),
        GST_OMX_RCAR_MEMORY_QUARK);
  }
  return rcar_mem->omx_buf == buf;
}
