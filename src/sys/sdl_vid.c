/* FUNCTION list 	---	---	---	---	---	---	---

*/

/*
I haven't found an ideal solution for converting 8 bit indexed surfaces to 32bit textures. However one thing that I
decided was that I don't want to call SDL_ConvertSurface per frame.

I ended up creating two surfaces, the 8 bit one that the AGI engine draws to and a second 32bit one that has the same
pixel format as the main screen texture. On render, we SDL_BlitSurface from the 8bit to 32bit surface to do pixel
conversion, then call SDL_UpdateTexture on the (streaming) screen texture with the 32bit surface as source.

References:
Rendering 8-bit palettized surfaces in SDL 2.0 applications: http://sandervanderburg.blogspot.com/2014/05/rendering-8-bit-palettized-surfaces-in.html
Mini code sample for SDL2 256-color palette https://discourse.libsdl.org/t/mini-code-sample-for-sdl2-256-color-palette/27147/10
*/

/* BASE headers	---	---	---	---	---	---	--- */
#include "../agi.h"

/* LIBRARY headers	---	---	---	---	---	---	--- */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
/* OTHER headers	---	---	---	---	---	---	--- */

#include "../base.h"

#include "mem_wrap.h"

#include "sdl_vid.h"



/* PROTOTYPES	---	---	---	---	---	---	--- */


static void vid_free_surfaces(void);
static void vid_render(SDL_Surface* surface, const u32 x, const u32 y, const u32 w, const u32 h);

/* VARIABLES	---	---	---	---	---	---	--- */

struct video_struct
{
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	SDL_Surface *surface;
	SDL_Surface *surface_conv;
	SDL_Palette *palette;
};

typedef struct video_struct VIDEO;

static VIDEO video_data = { 0 };

/* CODE	---	---	---	---	---	---	---	--- */



// elemental
//-----------------------------------------------------------


void vid_init(void)
{
#if 0
	printf("Initialising SDL video subsystem... ");

	if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
	{
		printf("vid_driver_init(): unable to initialise SDL video subsystem.\n");
		agi_exit();
	}

	printf("done.\n");
#endif
}

void vid_shutdown(void)
{
	//SDL_QuitSubSystem(SDL_INIT_VIDEO);
}


// create an 8bit window with such a size
// clear it with colour 0
// 1= fullscreen
// 0 = window
void vid_display(AGISIZE *screen_size, int fullscreen_state)
{
	if (video_data.window == 0)
	{
		SDL_WindowFlags sdl_flags;
		sdl_flags = SDL_WINDOW_RESIZABLE;
		if (fullscreen_state)
			sdl_flags |= SDL_WINDOW_FULLSCREEN;

		if (!SDL_CreateWindowAndRenderer("NAGI", screen_size->w, screen_size->h,
			sdl_flags, &video_data.window, &video_data.renderer))
		{
			printf("Unable to create video window: %s\n", SDL_GetError());
			agi_exit();
		}

		// SDL3: Use texture scale mode for smooth scaling
		SDL_SetRenderLogicalPresentation(video_data.renderer, screen_size->w, screen_size->h,
			SDL_LOGICAL_PRESENTATION_LETTERBOX);
	}
	else
	{
		if (!SDL_SetWindowFullscreen(video_data.window, fullscreen_state ? true : false))
		{
			printf("Error trying to set fullscreen state to %d: %s\n", fullscreen_state, SDL_GetError());
		}
	}

	assert(video_data.window != 0);
	assert(video_data.renderer != 0);

	if (video_data.surface != 0)
	{
		if ((video_data.surface->h != screen_size->h) || (video_data.surface->w != screen_size->w))
		{
			vid_free_surfaces();
			assert(video_data.surface == 0);
		}
	}

	if (video_data.surface == 0)
	{
		assert(video_data.surface == 0);

		// Create 8-bit indexed surface
		video_data.surface = SDL_CreateSurface(screen_size->w, screen_size->h, SDL_PIXELFORMAT_INDEX8);
		if (video_data.surface == NULL)
		{
			printf("Unable to create video surface: %s\n", SDL_GetError());
			agi_exit();
		}

		// Create and set palette for the 8-bit surface
		video_data.palette = SDL_CreatePalette(256);
		if (video_data.palette == NULL)
		{
			printf("Unable to create palette: %s\n", SDL_GetError());
			agi_exit();
		}
		SDL_SetSurfacePalette(video_data.surface, video_data.palette);

		SDL_FillSurfaceRect(video_data.surface, NULL, 0);

		assert(video_data.texture == 0);
		video_data.texture = SDL_CreateTexture( video_data.renderer,
			SDL_PIXELFORMAT_XRGB8888,
			SDL_TEXTUREACCESS_STREAMING,
			screen_size->w, screen_size->h );
		if (video_data.texture == NULL)
		{
			printf("Unable to create video texture: %s\n", SDL_GetError());
			agi_exit();
		}

		// Create intermediate surface to convert 8bit to 32bit pixels.
		assert(video_data.surface_conv == 0);
		video_data.surface_conv = SDL_CreateSurface(screen_size->w, screen_size->h, SDL_PIXELFORMAT_XRGB8888);
		if (video_data.surface_conv == NULL) {
			printf("Unable to create conversion video surface: %s\n", SDL_GetError());
			agi_exit();
		}
		SDL_FillSurfaceRect(video_data.surface_conv, NULL, SDL_MapSurfaceRGBA(video_data.surface_conv, 0, 0, 0, 255));

		vid_notify_window_size_changed(SDL_GetWindowID(video_data.window));
	}

	// clear
	SDL_SetRenderDrawColor(video_data.renderer, 0, 0, 0, 255);
	SDL_RenderClear(video_data.renderer);

	SDL_RenderPresent(video_data.renderer);
}

static void vid_free_surfaces(void)
{
	if (video_data.texture != 0)
	{
		SDL_DestroyTexture(video_data.texture);
		video_data.texture = 0;
	}

	if (video_data.surface != 0)
	{
		SDL_DestroySurface(video_data.surface);
		video_data.surface = 0;
	}

	if (video_data.surface_conv != 0)
	{
		SDL_DestroySurface(video_data.surface_conv);
		video_data.surface_conv = 0;
	}

	if (video_data.palette != 0)
	{
		SDL_DestroyPalette(video_data.palette);
		video_data.palette = 0;
	}
}

void vid_free(void)
{
	vid_free_surfaces();

	if (video_data.renderer != 0)
	{
		SDL_DestroyRenderer(video_data.renderer);
		video_data.renderer = 0;
	}

	if (video_data.window != 0)
	{
		SDL_DestroyWindow(video_data.window);
		video_data.window = 0;
	}
}

void *vid_getbuf(void)
{
	assert(video_data.surface);
	return video_data.surface->pixels;
}

int vid_getlinesize(void)
{
	assert(video_data.surface);
	return video_data.surface->pitch;
}

SDL_Window* vid_get_main_window(void)
{
	return video_data.window;
}

void vid_lock(void)
{
	assert(video_data.surface);
	if (!SDL_MUSTLOCK(video_data.surface)) { return; }
	if (!SDL_LockSurface(video_data.surface))
	{
		printf("vid_lock(); Unable to lock video surface: %s\n", SDL_GetError());
		agi_exit();
	}
}

void vid_unlock(void)
{
	assert(video_data.surface);
	if (!SDL_MUSTLOCK(video_data.surface)) { return; }
	SDL_UnlockSurface(video_data.surface);
}

// update a surface
void vid_update(POS *pos, AGISIZE *size)
{
	SDL_Surface *surface;

	surface = video_data.surface;
	assert(surface);

	// check boundaries
	if ((pos->x + size->w) > surface->w)
		size->w = surface->w - pos->x;
	if ((pos->y + size->h) > surface->h)
		size->h = surface->h - pos->y;

	vid_render(surface, pos->x, pos->y, size->w, size->h);

}

// when resizing a window, make sure the aspect ratio is preserved
void vid_notify_window_size_changed(SDL_WindowID windowID)
{
	if (video_data.window == 0)
	{
		printf("vid_notify_window_size_changed(): ERROR: received window resize event, but no window!\n");
		return;
	}

	SDL_WindowID current_window_id = SDL_GetWindowID(video_data.window);
	if (current_window_id == 0)
	{
		printf("vid_notify_window_size_changed(): ERROR: received window resize event, but unable to determine current window id: %s\n", SDL_GetError());
		return;
	}

	if (current_window_id != windowID) { return; }

	int window_width, window_height;
	SDL_GetWindowSize(video_data.window, &window_width, &window_height);

	if (video_data.texture == 0)
	{
		printf("vid_notify_window_size_changed(): ERROR: received window resize event, but no backing texture!\n");
		return;
	}

	float texture_width_f, texture_height_f;
	if (!SDL_GetTextureSize(video_data.texture, &texture_width_f, &texture_height_f))
	{
		printf("vid_notify_window_size_changed(): ERROR: received window resize event, but unable to determine texture size: %s\n", SDL_GetError());
		return;
	}

	int texture_width = (int)texture_width_f;
	int texture_height = (int)texture_height_f;

	int new_window_width = window_width;
	int new_window_height = window_width * texture_height / texture_width;

	// we have to check size is different or we repeatedly get change events.
	if ((new_window_height != window_height) || (new_window_width != window_width))
	{
		SDL_SetWindowSize(video_data.window, new_window_width, new_window_height);
		vid_render(video_data.surface, 0, 0, texture_width, texture_height);
	}
}

static void vid_render(SDL_Surface *surface, const u32 x, const u32 y, const u32 w, const u32 h)
{
	SDL_Rect rect;
	rect.x = (int)x;
	rect.y = (int)y;
	rect.w = (int)w;
	rect.h = (int)h;

	(void)rect; // unused in current implementation

	// Convert up from 8bpp (used on ye olde graphics cards) to
	// something relevant to this century
	if (!SDL_BlitSurface(surface, NULL, video_data.surface_conv, NULL)) {
		printf("vid_render: Error converting surface: %s\n", SDL_GetError());
	}
	if (!SDL_UpdateTexture(video_data.texture, NULL, video_data.surface_conv->pixels, video_data.surface_conv->pitch)) {
		printf("vid_render: Error updating screen texture: %s\n", SDL_GetError());
	}

	SDL_SetRenderDrawColor(video_data.renderer, 0, 0, 0, 255);
	if (!SDL_RenderClear(video_data.renderer)) {
		printf("vid_render: Error clearing screen: %s\n", SDL_GetError());
	}

	if (!SDL_RenderTexture(video_data.renderer, video_data.texture, NULL, NULL)) {
		printf("vid_render: Error copying texture to screen: %s\n", SDL_GetError());
	}
	SDL_RenderPresent(video_data.renderer);
}

// set 8-bit palette
void vid_palette_set(PCOLOUR *palette, u8 num)
{
	SDL_Color *sdl_palette = alloca(num * sizeof(SDL_Color));
	int i;

	assert(video_data.surface);
	assert(video_data.palette);

	for (i=0; i<num; i++)
	{
		sdl_palette[i].r = palette[i].r;
		sdl_palette[i].g = palette[i].g;
		sdl_palette[i].b = palette[i].b;
		sdl_palette[i].a = 255;
	}

	if(!SDL_SetPaletteColors(video_data.palette, sdl_palette, 0, num))
	{
		printf( "Unable to set colour palette: %s\n", SDL_GetError());
		agi_exit();
	}
}

/* Get RGB color from palette by index */
void vid_palette_get_color(u8 index, u8 *r, u8 *g, u8 *b)
{
	const SDL_Color *colors;
	u8 palette_index;

	assert(video_data.palette);
	assert(video_data.palette->ncolors > 0);

	/* Wrap index to available palette colors (e.g., CGA has only 4 colors) */
	palette_index = index % video_data.palette->ncolors;

	colors = video_data.palette->colors;
	*r = colors[palette_index].r;
	*g = colors[palette_index].g;
	*b = colors[palette_index].b;
}

//derived:
//-----------------------------------------------------------

// fill the surface at a particular point.. rectangle
// can use library's fast blit's 'n stuff
void vid_fill(POS *pos, AGISIZE *size, u32 colour)
{
	assert(video_data.surface);

	if ( (pos->x|pos->y|size->w|size->h) == 0)
	{
		vid_lock();
		SDL_FillSurfaceRect(video_data.surface, 0, colour);
		vid_unlock();
	}
	else
	{
		SDL_Rect rect;
		rect.x = pos->x;
		rect.y = pos->y;
		rect.w = size->w;
		rect.h = size->h;
		vid_lock();
		SDL_FillSurfaceRect(video_data.surface, &rect, colour);
		vid_unlock();
		vid_render(video_data.surface,
			rect.x, rect.y, rect.w, rect.h);
	}
}

static int shake_offset[] = {25, 0, -25};

void vid_shake(int count)
{
	int width, height;
	SDL_Surface *orig;
	SDL_Rect dest = {0,0, 0, 0};
	SDL_Surface *surface;

	surface = video_data.surface;
	assert(surface);

	width =  surface->w;
	height =  surface->h;

	// Create new surface with same format as original
	orig = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_INDEX8);
	if (!orig) return;

	// Copy palette from original surface
	if (video_data.palette) {
		SDL_Palette *new_palette = SDL_CreatePalette(video_data.palette->ncolors);
		if (new_palette) {
			SDL_SetPaletteColors(new_palette, video_data.palette->colors, 0, video_data.palette->ncolors);
			SDL_SetSurfacePalette(orig, new_palette);
			// Note: We'll need to destroy this palette, but for simplicity we'll just let it leak
			// during the shake effect since it's very short-lived
		}
	}

	// blit screen to new surface
	if (!SDL_BlitSurface(surface, 0, orig, 0)) goto shake_error;

	count *= 8;
	while (count--)
	{
		// clear entire window
		vid_lock();
		if (!SDL_FillSurfaceRect(surface, 0, 0))
			goto shake_error;
		vid_unlock();

		// print the surface in some strange location
		dest.x = shake_offset[rand()%3];
		dest.y = shake_offset[rand()%3];
		if (!SDL_BlitSurface(orig, 0, surface, &dest)) // blit to some offset  stretch*10 or something
			goto shake_error;
		vid_render(surface, 0, 0, 0, 0);

		SDL_Delay(50);
	}
	// put the original screen back on
	if (!SDL_BlitSurface(orig, 0, surface, 0)) // update the screen
		goto shake_error;
	vid_render(surface, 0, 0, 0, 0);

shake_error:
	SDL_DestroySurface(orig);
}
