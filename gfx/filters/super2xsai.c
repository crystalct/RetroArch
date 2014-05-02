/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2010-2014 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

// Compile: gcc -o supertwoxsai.so -shared supertwoxsai.c -std=c99 -O3 -Wall -pedantic -fPIC

#include "softfilter.h"
#include "softfilter_prototypes.h"
#include <stdlib.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation supertwoxsai_get_implementation
#endif

#define SUPERTWOXSAI_SCALE 2

static unsigned supertwoxsai_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565 | SOFTFILTER_FMT_XRGB8888;
}

static unsigned supertwoxsai_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned supertwoxsai_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void *supertwoxsai_generic_create(unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd)
{
   (void)simd;

   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   if (!filt)
      return NULL;
   filt->workers = (struct softfilter_thread_data*)calloc(threads, sizeof(struct softfilter_thread_data));
   filt->threads = threads;
   filt->in_fmt  = in_fmt;
   if (!filt->workers)
   {
      free(filt);
      return NULL;
   }
   return filt;
}

static void supertwoxsai_generic_output(void *data, unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width = width * SUPERTWOXSAI_SCALE;
   *out_height = height * SUPERTWOXSAI_SCALE;
}

static void supertwoxsai_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   free(filt->workers);
   free(filt);
}

#define supertwoxsai_interpolate_xrgb8888(A, B) ((((A) & 0xFEFEFEFE) >> 1) + (((B) & 0xFEFEFEFE) >> 1) + ((A) & (B) & 0x01010101))

#define supertwoxsai_interpolate2_xrgb8888(A, B, C, D) ((((A) & 0xFCFCFCFC) >> 2) + (((B) & 0xFCFCFCFC) >> 2) + (((C) & 0xFCFCFCFC) >> 2) + (((D) & 0xFCFCFCFC) >> 2) + (((((A) & 0x03030303) + ((B) & 0x03030303) + ((C) & 0x03030303) + ((D) & 0x03030303)) >> 2) & 0x03030303))

#define supertwoxsai_interpolate_rgb565(A, B) ((((A) & 0xF7DE) >> 1) + (((B) & 0xF7DE) >> 1) + ((A) & (B) & 0x0821));

#define supertwoxsai_interpolate2_rgb565(A, B, C, D) ((((A) & 0xE79C) >> 2) + (((B) & 0xE79C) >> 2) + (((C) & 0xE79C) >> 2) + (((D) & 0xE79C) >> 2)  + (((((A) & 0x1863) + ((B) & 0x1863) + ((C) & 0x1863) + ((D) & 0x1863)) >> 2) & 0x1863))

#define supertwoxsai_result(A, B, C, D) (((A) != (C) || (A) != (D)) - ((B) != (C) || (B) != (D)))

#ifndef supertwoxsai_declare_variables
#define supertwoxsai_declare_variables(typename_t, in, nextline) \
         typename_t product1a, product1b, product2a, product2b; \
         const typename_t colorB0 = *(in - nextline - 1); \
         const typename_t colorB1 = *(in - nextline + 0); \
         const typename_t colorB2 = *(in - nextline + 1); \
         const typename_t colorB3 = *(in - nextline + 2); \
         const typename_t color4  = *(in - 1); \
         const typename_t color5  = *(in + 0); \
         const typename_t color6  = *(in + 1); \
         const typename_t colorS2 = *(in + 2); \
         const typename_t color1  = *(in + nextline - 1); \
         const typename_t color2  = *(in + nextline + 0); \
         const typename_t color3  = *(in + nextline + 1); \
         const typename_t colorS1 = *(in + nextline + 2); \
         const typename_t colorA0 = *(in + nextline + nextline - 1); \
         const typename_t colorA1 = *(in + nextline + nextline + 0); \
         const typename_t colorA2 = *(in + nextline + nextline + 1); \
         const typename_t colorA3 = *(in + nextline + nextline + 2)
#endif

#ifndef supertwoxsai_function
#define supertwoxsai_function(result_cb, interpolate_cb, interpolate2_cb) \
         if (color2 == color6 && color5 != color3) \
            product2b = product1b = color2; \
         else if (color5 == color3 && color2 != color6) \
            product2b = product1b = color5; \
         else if (color5 == color3 && color2 == color6) \
         { \
            int r = 0; \
            r += result_cb(color6, color5, color1, colorA1); \
            r += result_cb(color6, color5, color4, colorB1); \
            r += result_cb(color6, color5, colorA2, colorS1); \
            r += result_cb(color6, color5, colorB2, colorS2); \
            if (r > 0) \
               product2b = product1b = color6; \
            else if (r < 0) \
               product2b = product1b = color5; \
            else \
               product2b = product1b = interpolate_cb(color5, color6); \
         } \
         else \
         { \
            if (color6 == color3 && color3 == colorA1 && color2 != colorA2 && color3 != colorA0) \
               product2b = interpolate2_cb(color3, color3, color3, color2); \
            else if ((color5 == color2 && color2 == colorA2) & (colorA1 != color3 && color2 != colorA3)) \
               product2b = interpolate2_cb(color2, color2, color2, color3); \
            else \
               product2b = interpolate_cb(color2, color3); \
            if (color6 == color3 && color6 == colorB1 && color5 != colorB2 && color6 != colorB0) \
               product1b = interpolate2_cb(color6, color6, color6, color5); \
            else if (color5 == color2 && color5 == colorB2 && colorB1 != color6 && color5 != colorB3) \
               product1b = interpolate2_cb(color6, color5, color5, color5); \
            else \
               product1b = interpolate_cb(color5, color6); \
         } \
         if (color5 == color3 && color2 != color6 && color4 == color5 && color5 != colorA2) \
         { \
            product2a = interpolate_cb(color2, color5); \
         } \
         else if (color5 == color1 && color6 == color5 && color4 != color2 && color5 != colorA0) \
         { \
            product2a = interpolate_cb(color2, color5); \
         } \
         else \
            product2a = color2; \
         if (color2 == color6 && color5 != color3 && color1 == color2 && color2 != colorB2) \
         { \
            product1a = interpolate_cb(color2, color5); \
         } \
         else if (color4 == color2 && color3 == color2 && color1 != color5 && color2 != colorB0) \
         { \
            product1a = interpolate_cb(color2, color5); \
         } \
         else \
            product1a = color5; \
         out[0] = product1a; \
         out[1] = product1b; \
         out[dst_stride] = product2a; \
         out[dst_stride + 1] = product2b; \
         ++in; \
         out += 2
#endif

static void supertwoxsai_generic_xrgb8888(unsigned width, unsigned height,
      int first, int last, uint32_t *src, 
      unsigned src_stride, uint32_t *dst, unsigned dst_stride)
{
   unsigned nextline, finish;
   nextline = (last) ? 0 : src_stride;

   for (; height; height--)
   {
      uint32_t *in  = (uint32_t*)src;
      uint32_t *out = (uint32_t*)dst;

      for (finish = width; finish; finish -= 1)
      {
         supertwoxsai_declare_variables(uint32_t, in, nextline);

         //---------------------------    B1 B2
         //                             4  5  6 S2
         //                             1  2  3 S1
         //                               A1 A2
         //--------------------------------------
         
         supertwoxsai_function(supertwoxsai_result, supertwoxsai_interpolate_xrgb8888, supertwoxsai_interpolate2_xrgb8888);
      }

      src += src_stride;
      dst += 2 * dst_stride;
   }
}

static void supertwoxsai_generic_rgb565(unsigned width, unsigned height,
      int first, int last, uint16_t *src, 
      unsigned src_stride, uint16_t *dst, unsigned dst_stride)
{
   unsigned nextline, finish;
   nextline = (last) ? 0 : src_stride;

   for (; height; height--)
   {
      uint16_t *in  = (uint16_t*)src;
      uint16_t *out = (uint16_t*)dst;

      for (finish = width; finish; finish -= 1)
      {
         supertwoxsai_declare_variables(uint16_t, in, nextline);

         //---------------------------    B1 B2
         //                             4  5  6 S2
         //                             1  2  3 S1
         //                               A1 A2
         //--------------------------------------
         
         supertwoxsai_function(supertwoxsai_result, supertwoxsai_interpolate_rgb565, supertwoxsai_interpolate2_rgb565);
      }

      src += src_stride;
      dst += 2 * dst_stride;
   }
}

static void supertwoxsai_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   uint16_t *input = (uint16_t*)thr->in_data;
   uint16_t *output = (uint16_t*)thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;

   supertwoxsai_generic_rgb565(width, height,
         thr->first, thr->last, input, thr->in_pitch / SOFTFILTER_BPP_RGB565, output, thr->out_pitch / SOFTFILTER_BPP_RGB565);
}

static void supertwoxsai_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   uint32_t *input = (uint32_t*)thr->in_data;
   uint32_t *output = (uint32_t*)thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;

   supertwoxsai_generic_xrgb8888(width, height,
         thr->first, thr->last, input, thr->in_pitch / SOFTFILTER_BPP_XRGB8888, output, thr->out_pitch / SOFTFILTER_BPP_XRGB8888);
}

static void supertwoxsai_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   struct filter_data *filt = (struct filter_data*)data;
   unsigned i;
   for (i = 0; i < filt->threads; i++)
   {
      struct softfilter_thread_data *thr = (struct softfilter_thread_data*)&filt->workers[i];

      unsigned y_start = (height * i) / filt->threads;
      unsigned y_end = (height * (i + 1)) / filt->threads;
      thr->out_data = (uint8_t*)output + y_start * SUPERTWOXSAI_SCALE * output_stride;
      thr->in_data = (const uint8_t*)input + y_start * input_stride;
      thr->out_pitch = output_stride;
      thr->in_pitch = input_stride;
      thr->width = width;
      thr->height = y_end - y_start;

      // Workers need to know if they can access pixels outside their given buffer.
      thr->first = y_start;
      thr->last = y_end == height;

      if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
         packets[i].work = supertwoxsai_work_cb_rgb565;
      else if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
         packets[i].work = supertwoxsai_work_cb_xrgb8888;
      packets[i].thread_data = thr;
   }
}

static const struct softfilter_implementation supertwoxsai_generic = {
   supertwoxsai_generic_input_fmts,
   supertwoxsai_generic_output_fmts,

   supertwoxsai_generic_create,
   supertwoxsai_generic_destroy,

   supertwoxsai_generic_threads,
   supertwoxsai_generic_output,
   supertwoxsai_generic_packets,
   "Super2xSaI",
   SOFTFILTER_API_VERSION,
};

const struct softfilter_implementation *softfilter_get_implementation(softfilter_simd_mask_t simd)
{
   (void)simd;
   return &supertwoxsai_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#endif
