/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include "../../driver.h"

#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../gl_common.h"

#include "ps3_ctx.h"

void gfx_ctx_set_swap_interval(unsigned interval, bool inited)
{
   bool success = false;

   if(inited)
   {
      success = true;

      if (interval)
         glEnable(GL_VSYNC_SCE);
      else
         glDisable(GL_VSYNC_SCE);
   }

   if (!success)
      RARCH_WARN("Failed to set swap interval.\n");
}

void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   *quit = false;
   *resize = false;
   gl_t *gl = driver.video_data;

#ifdef HAVE_SYSUTILS
   cellSysutilCheckCallback();
#endif

   if(gl->quitting)
      *quit = true;

   if(gl->should_resize)
      *resize = true;
}

bool gfx_ctx_window_has_focus(void)
{
   return true;
}
