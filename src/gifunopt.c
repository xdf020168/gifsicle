/* gifunopt.c - Unoptimization function for the GIF library.
   Copyright (C) 1997 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of the GIF library.

   The GIF library is free software*. It is distributed under the GNU Public
   License, version 2 or later; you can copy, distribute, or alter it at will,
   as long as this notice is kept intact and this source code is made
   available. There is no warranty, express or implied.

   *The LZW compression method used by GIFs is patented. Unisys, the patent
   holder, allows the compression algorithm to be used without a license in
   software distributed at no cost to the user. */

#include "gif.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

#define TRANSPARENT 256

static void
put_image_in_screen(Gif_Stream *gfs, Gif_Image *gfi, u_int16_t *screen)
{
  int transparent = gfi->transparent;
  int x, y;
  int w = gfi->width;
  int h = gfi->height;
  if (gfi->left + w > gfs->screen_width) w = gfs->screen_width - gfi->left;
  if (gfi->top + h > gfs->screen_height) h = gfs->screen_height - gfi->top;
  
  for (y = 0; y < h; y++) {
    u_int16_t *move = screen + gfs->screen_width * (y + gfi->top) + gfi->left;
    byte *line = gfi->img[y];
    for (x = 0; x < w; x++, move++, line++)
      if (*line != transparent)
	*move = *line;
  }
}


static void
put_background_in_screen(Gif_Stream *gfs, Gif_Image *gfi, u_int16_t *screen)
{
  u_int16_t solid;
  int x, y;
  int w = gfi->width;
  int h = gfi->height;
  if (gfi->left + w > gfs->screen_width) w = gfs->screen_width - gfi->left;
  if (gfi->top + h > gfs->screen_height) h = gfs->screen_height - gfi->top;
  
  if (gfi->transparent >= 0)
    solid = TRANSPARENT;
  else if (gfs->images[0]->transparent >= 0)
    solid = TRANSPARENT;
  else
    solid = gfs->background;
  
  for (y = 0; y < h; y++) {
    u_int16_t *move = screen + gfs->screen_width * (y + gfi->top) + gfi->left;
    for (x = 0; x < w; x++, move++)
      *move = solid;
  }
}


static int
create_image_data(Gif_Stream *gfs, Gif_Image *gfi, u_int16_t *screen,
		  byte *new_data)
{
  int have[257];
  int transparent = -1;
  int size = gfs->screen_width * gfs->screen_height;
  u_int16_t *move;
  int i;

  /* mark colors used opaquely in the image */
  assert(TRANSPARENT == 256);
  for (i = 0; i < 257; i++)
    have[i] = 0;
  for (i = 0, move = screen; i < size; i++, move++)
    have[*move] = 1;
  
  /* the new transparent color is a color unused in either */
  if (have[TRANSPARENT]) {
    for (i = 0; i < 256 && transparent < 0; i++)
      if (!have[i])
	transparent = i;
    if (transparent < 0)
      goto error;
    if (transparent >= gfs->global->ncol) {
      Gif_ReArray(gfs->global->col, Gif_Color, 256);
      if (!gfs->global->col) goto error;
      gfs->global->ncol = transparent + 1;
    }
  }
  
  /* map the wide image onto the new data */
  for (i = 0, move = screen; i < size; i++, move++, new_data++)
    if (*move == TRANSPARENT)
      *new_data = transparent;
    else
      *new_data = *move;

  gfi->transparent = transparent;
  return 1;
  
 error:
  return 0;
}


static int
unoptimize_image(Gif_Stream *gfs, Gif_Image *gfi, u_int16_t *screen)
{
  int size = gfs->screen_width * gfs->screen_height;
  byte *new_data = Gif_NewArray(byte, size);
  u_int16_t *new_screen = screen;
  if (!new_data) return 0;
  
  /* Oops! May need to uncompress it */
  Gif_UncompressImage(gfi);
  Gif_ReleaseCompressedImage(gfi);
  
  if (gfi->disposal == GIF_DISPOSAL_PREVIOUS) {
    new_screen = Gif_NewArray(u_int16_t, size);
    if (!new_screen) return 0;
    memcpy(new_screen, screen, size * sizeof(u_int16_t));
  }
  
  put_image_in_screen(gfs, gfi, new_screen);
  if (!create_image_data(gfs, gfi, new_screen, new_data)) {
    Gif_DeleteArray(new_data);
    return 0;
  }
  
  if (gfi->disposal == GIF_DISPOSAL_PREVIOUS)
    Gif_DeleteArray(new_screen);
  else if (gfi->disposal == GIF_DISPOSAL_BACKGROUND)
    put_background_in_screen(gfs, gfi, screen);
  
  gfi->left = 0;
  gfi->top = 0;
  gfi->width = gfs->screen_width;
  gfi->height = gfs->screen_height;
  gfi->disposal = GIF_DISPOSAL_BACKGROUND;
  Gif_SetUncompressedImage(gfi, new_data, Gif_DeleteArrayFunc, 0);
  
  return 1;
}


int
Gif_Unoptimize(Gif_Stream *gfs)
{
  int ok = 1;
  int i, size;
  u_int16_t *screen;
  u_int16_t background;
  Gif_Image *gfi;
  
  if (gfs->nimages <= 1) return 1;
  for (i = 0; i < gfs->nimages; i++)
    if (gfs->images[i]->local)
      return 0;
  if (!gfs->global)
    return 0;
  
  Gif_CalculateScreenSize(gfs, 0);
  size = gfs->screen_width * gfs->screen_height;
  
  screen = Gif_NewArray(u_int16_t, size);
  gfi = gfs->images[0];
  background = gfi->transparent >= 0 ? TRANSPARENT : gfs->background;
  for (i = 0; i < size; i++)
    screen[i] = background;
  
  for (i = 0; i < gfs->nimages; i++)
    if (!unoptimize_image(gfs, gfs->images[i], screen))
      ok = 0;
  
  Gif_DeleteArray(screen);
  return ok;
}


#ifdef __cplusplus
}
#endif
