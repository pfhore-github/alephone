/*

	Copyright (C) 1991-2001 and beyond by Bungie Studios, Inc.
	and the "Aleph One" developers.
 
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	This license is contained in the file "COPYING",
	which is included with this source code; it is available online at
	http://www.gnu.org/licenses/gpl.html

	Font handler
	by Loren Petrich,
	December 17, 2000
	
	This is for specifying and working with text fonts

Dec 25, 2000 (Loren Petrich):
	Added OpenGL-rendering support

Dec 31, 2000 (Loren Petrich):
	Switched to a 32-bit intermediate GWorld, so that text antialiasing
	will work properly.

Jan 12, 2001 (Loren Petrich):
	Fixed MacOS version of TextWidth() -- uses current font
*/

#include "cseries.h"

#ifdef HAVE_OPENGL
#include "OGL_Headers.h"
#include "OGL_Blitter.h"
#include "OGL_Render.h"
#endif

#include <math.h>
#include <string.h>
#include "FontHandler.h"

#include "shape_descriptors.h"
#include "screen_drawing.h"
#include "screen.h"

#ifdef HAVE_OPENGL
std::set<FontSpecifier*> *FontSpecifier::m_font_registry = NULL;
#endif

// MacOS-specific: stuff that gets reused
// static CTabHandle Grays = NULL;

// Font-specifier equality and assignment:

bool FontSpecifier::operator==(FontSpecifier& F)
{
	if (Size != F.Size) return false;
	if (Style != F.Style) return false;
	if (File != F.File) return false;
	return true;
}

FontSpecifier& FontSpecifier::operator=(FontSpecifier& F)
{
	Size = F.Size;
	Style = F.Style;
	File = F.File;
	return *this;
}

FontSpecifier::~FontSpecifier()
{
#ifdef HAVE_OPENGL
	OGL_Reset(false);
#endif
}

// Initializer: call before using because of difficulties in setting up a proper constructor:

void FontSpecifier::Init()
{
	Info = NULL;
	Update();
}

void FontSpecifier::Update()
{
	// Clear away
	if (Info) {
		unload_font(Info);
		Info = NULL;
	}
		
	TextSpec Spec;
	Spec.size = Size;
	Spec.style = Style;
	Spec.adjust_height = -4;
    
	// Simply implements format "#<value>"; may want to generalize this
	if (File[0] == '#') 
	{
		short ID;
		sscanf(File.c_str() +1, "%hd", &ID);
		
		Spec.font = ID;
		if (ID == 4)
		{
			Spec.font = -1;
			Spec.normal = "Monaco";
			Spec.size = Size * 1.34f;
		}
		else if (ID == 22) 
		{
			Spec.font = -1;
			Spec.normal = "Courier Prime";
			Spec.bold = "Courier Prime Bold";
			Spec.oblique = "Courier Prime Italic";
			Spec.bold_oblique = "Courier Prime Bold Italic";
			Spec.adjust_height -= Size * 0.084f;
		}
	}
	else
	{
		Spec.font = -1; // no way to fall back :(
		Spec.normal = File;
	}

	Info = load_font(Spec);
	
	if (Info) {
		Ascent = Info->get_ascent();
		Descent = Info->get_descent();
		Leading = Info->get_leading();
		Height = Ascent + Leading;
		LineSpacing = Ascent + Descent + Leading;
	} else
		Ascent = Descent = Leading = Height = LineSpacing = 0;
}

// Defined in screen_drawing_sdl.cpp
extern int8 char_width(uint8 c, const sdl_font_info *font, uint16 style);

int FontSpecifier::TextWidth(const char *text)
{
	return Info->text_width(text, Style, false);
}
void FontSpecifier::render_text_(const char* str, bool draw) {
	// Put some padding around each glyph so as to avoid clipping i
	const int Pad = 1;
	int ascent_p = Ascent + Pad, descent_p = Descent + Pad;
	int GlyphHeight = ascent_p + descent_p;

	auto found = caches.find(str);
	if (found == caches.end()) {
		OGL_CACHE cache;

		int TotalWidth = TextWidth(str) + Pad * 2;
		cache.Width = TotalWidth;
		cache.TxtrWidth = NextPowerOfTwo(TotalWidth);
		cache.TxtrHeight = NextPowerOfTwo(GlyphHeight);

		// Render the font glyphs into the SDL surface
		SDL_Surface* s = SDL_CreateRGBSurface(SDL_SWSURFACE, cache.TxtrWidth, cache.TxtrHeight, 32, 0xff0000, 0x00ff00, 0x0000ff, 0);
		if (s == NULL)
			return;
		// Set background to black
		SDL_FillRect(s, NULL, SDL_MapRGB(s->format, 0, 0, 0));
		Uint32 White = SDL_MapRGB(s->format, 0xFF, 0xFF, 0xFF);

		// Copy to surface
		::draw_text(s, str, strlen(str), 1, ascent_p, White, Info, Style);
		cache.Texture = new uint8[cache.TxtrWidth * cache.TxtrHeight * 2];
		// Copy the SDL surface into the OpenGL texture
		uint8* PixBase = (uint8*)s->pixels;
		int Stride = s->pitch;

		for (int k = 0; k < cache.TxtrHeight; k++)
		{
			uint8* SrcPxl = PixBase + k * Stride + 1;	// Use one of the middle channels (red or green or blue)
			uint8* DstPxl = cache.Texture + 2 * k * cache.TxtrWidth;
			for (int m = 0; m < cache.TxtrWidth; m++)
			{
				*(DstPxl++) = 0xff;	// Base color: white (will be modified with glColorxxx())
				*(DstPxl++) = *SrcPxl;
				SrcPxl += 4;
			}
		}

		// Clean up
		SDL_FreeSurface(s);

		// OpenGL stuff starts here 	
		// Load texture
		glGenTextures(1, &cache.texId);
		glBindTexture(GL_TEXTURE_2D, cache.texId);

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, NearFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA,
			cache.TxtrWidth, cache.TxtrHeight,
			0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, cache.Texture);

		auto ret = caches.insert(std::make_pair(str, cache));
		found = ret.first;
	}
	else {
		glBindTexture(GL_TEXTURE_2D, found->second.texId);
	}
	if (draw) {

		GLfloat TWidNorm = GLfloat(1) / found->second.TxtrWidth;
		GLfloat THtNorm = GLfloat(1) / found->second.TxtrHeight;
		GLfloat Bottom = (THtNorm * GlyphHeight);
		GLfloat Right = TWidNorm * found->second.Width;

		// Move to the current glyph's (padded) position
		glTranslatef(-Pad, 0, 0);

		// Draw the glyph rectangle
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		OGL_RenderTexturedRect(0, -ascent_p, found->second.Width, descent_p + ascent_p,
			0, 0, Right, Bottom);
		// Move to the next glyph's position
		glTranslated(found->second.Width - Pad, 0, 0);
	}
}

// Reset the OpenGL fonts; its arg indicates whether this is for starting an OpenGL session
// (this is to avoid texture and display-list memory leaks and other such things)
void FontSpecifier::OGL_Reset(bool IsStarting)
{
	if (!IsStarting) {
		for (auto& cache : caches) {
			delete[] cache.second.Texture;
			glDeleteTextures(1, &cache.second.texId);
		}
	}
	else {
		caches.clear();
		for (int ch = 0x20; ch < 0x80; ++ch) {
			char f[] = { (char)ch, 0 };
			render_text_(f, false);
		}
	}
}

#include "converter.h"
// Renders a C-style string in OpenGL.
// assumes screen coordinates and that the left baseline point is at (0,0).
// Alters the modelview matrix so that the next characters will be drawn at the proper place.
// One can surround it with glPushMatrix() and glPopMatrix() to remember the original.
void FontSpecifier::OGL_Render(const char *Text)
{
	glPushAttrib(GL_ENABLE_BIT);
			
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	std::string cv;
	cv.reserve(256);
	while( *Text) {
		auto p = next_utf8(Text);
		if( p.second < 0x80 ) {
			if( ! cv.empty() ) {
				render_text_(cv.c_str(), true);
				cv.clear();
			}
			char cc[] = { (char)p.second, 0};
			render_text_(cc, true);
		} else {
			for(int i = 0; i < p.first; ++i) {
				cv += Text[i];
			}
		}
		Text += p.first;
	}
	if( ! cv.empty() ) {
		render_text_(cv.c_str(), true);
	}
	glPopAttrib();


}

// Renders text a la _draw_screen_text() (see screen_drawing.h), with
// alignment and wrapping. Modelview matrix is unaffected.
void FontSpecifier::OGL_DrawText(const char *text, const screen_rectangle &r, short flags)
{
	// Copy the text to draw
	char text_to_draw[256];
	strncpy(text_to_draw, text, 256);
	text_to_draw[255] = 0;
	int rect_width = RECTANGLE_WIDTH(&r);
	// Check for wrapping, and if it occurs, be recursive
	int t_width = TextWidth(text_to_draw);
	auto len = strlen(text_to_draw);
	if (t_width > rect_width && (flags & _wrap_text)) {
		int last_breaking_pos = 0, text_width = 0;
		unsigned count = 0;
		bool wide = false, begin = true;
		while (count < strlen(text_to_draw) && text_width < rect_width) {
			int now_count = count;
			auto ret = next_utf8(&text_to_draw[count]);
			count += ret.first;
			uint16_t c = ret.second;
			text_width += Info->char_width(c, Style);	
			if( c >= 0x3040 && c <= 0x9fef ) {
				if( begin ) {
					wide = true;
				} else if( ! wide ) {
					last_breaking_pos = now_count;
					wide = true;
				}
			} else {
				if (c == ' ' || wide )
					last_breaking_pos = now_count;
				wide = false;
			}
			begin = false;
		}
		
		if( count != len) {
			char remaining_text_to_draw[256];
			
			// If we ever have to wrap text, we can't also center vertically. Sorry.
			flags &= ~_center_vertical;
			flags |= _top_justified;
			
			// Pass the rest of it back in, recursively, on the next line
			memcpy(remaining_text_to_draw, text_to_draw + last_breaking_pos, strlen(text_to_draw + last_breaking_pos) + 1);
	
			screen_rectangle new_destination = r;
			new_destination.top += LineSpacing;
			new_destination.bottom += LineSpacing;
			OGL_DrawText(remaining_text_to_draw, new_destination, flags);
	
			// Now truncate our text to draw
			text_to_draw[last_breaking_pos] = 0;
		}
	}

	// Truncate text if necessary
	if (t_width > rect_width) {
		int width = 0;
		int num = 0;
		char *p = text_to_draw;
		while ( *p != 0) {
			auto ret = next_utf8(p);
			p += ret.first;
			uint16_t c = ret.second;
			width += Info->char_width(c, Style);
			if (width > rect_width) {
				break;
			}
			num += ret.first;
		}
		text_to_draw[num] = 0;
		t_width = TextWidth(text_to_draw);
	}


	// Horizontal positioning
	int x, y;
	if (flags & _center_horizontal)
		x = r.left + ((rect_width - t_width) / 2);
	else if (flags & _right_justified)
		x = r.right - t_width;
	else
		x = r.left;

	// Vertical positioning
	if (flags & _center_vertical) {
		if (Height > RECTANGLE_HEIGHT(&r))
			y = r.top;
		else {
			y = r.bottom;
			int offset = RECTANGLE_HEIGHT(&r) - Height;
			y -= (offset / 2) + (offset & 1) + 1;
		}
	} else if (flags & _top_justified) {
		if (Height > RECTANGLE_HEIGHT(&r))
			y = r.bottom;
		else
			y = r.top + Height;
	} else
		y = r.bottom;

	// Draw text
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslated(x, y, 0);
	OGL_Render(text_to_draw);
	glPopMatrix();
}

void FontSpecifier::OGL_ResetFonts(bool IsStarting)
{
    if (!m_font_registry)
        return;
    
	std::set<FontSpecifier*>::iterator it;
	if (IsStarting)
	{
		for (it = m_font_registry->begin();
			 it != m_font_registry->end();
			 ++it)
			(*it)->OGL_Reset(IsStarting);
	}
	else
	{
		for (it = m_font_registry->begin();
			 it != m_font_registry->end();
			 it = m_font_registry->begin())
			(*it)->OGL_Reset(IsStarting);
	}
}

void FontSpecifier::OGL_Register(FontSpecifier *F)
{
	if (!m_font_registry)
		m_font_registry = new std::set<FontSpecifier*>;
	m_font_registry->insert(F);
}

void FontSpecifier::OGL_Deregister(FontSpecifier *F)
{
	if (m_font_registry)
		m_font_registry->erase(F);
	// we could delete registry here, but why bother?
}



// Draw text without worrying about OpenGL vs. SDL mode.
int FontSpecifier::DrawText(SDL_Surface *s, const char *text, int x, int y, uint32 pixel, bool)
{
	if (!s)
		return 0;
	if (s == MainScreenSurface() && MainScreenIsOpenGL())
		return draw_text(s, text, x, y, pixel, this->Info, this->Style, true);

#ifdef HAVE_OPENGL
		
	uint8 r, g, b;
	SDL_GetRGB(pixel, s->format, &r, &g, &b);
	glColor4ub(r, g, b, 255);
	
	// draw into both buffers
	for (int i = 0; i < 2; i++)
	{
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glTranslatef(x, y, 0);
		this->OGL_Render(text);
		glPopMatrix();
		MainScreenSwap();
	}
	return 1;
#else
	return 0;
#endif
}
