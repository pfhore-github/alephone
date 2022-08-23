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

*/

/*
 *  sdl_fonts.cpp - SDL font handling
 *
 *  Written in 2000 by Christian Bauer
 */

#include "cseries.h"
#include "sdl_fonts.h"
#include "byte_swapping.h"
#include "resource_manager.h"
#include "FileHandler.h"
#include "Logging.h"

#include <SDL2/SDL_endian.h>
#include <vector>
#include <map>

#include <boost/tokenizer.hpp>
#include <string>

#ifndef NO_STD_NAMESPACE
using std::vector;
using std::pair;
using std::map;
#endif

#include "preferences.h" // smooth_font
#include "AlephSansMono-Bold.h"
#include "ProFontAO.h"

#include "CourierPrime.h"
#include "CourierPrimeBold.h"
#include "CourierPrimeItalic.h"
#include "CourierPrimeBoldItalic.h"

// Global variables
typedef pair<int, int> id_and_size_t;
typedef map<id_and_size_t, sdl_font_info *> font_list_t;
static font_list_t font_list;				// List of all loaded fonts

typedef pair<TTF_Font *, int> ref_counted_ttf_font_t;
typedef map<ttf_font_key_t, ref_counted_ttf_font_t> ttf_font_list_t;
static ttf_font_list_t ttf_font_list;

// From shell_sdl.cpp
extern vector<DirectorySpecifier> data_search_path;


/*
 *  Initialize font management
 */

typedef struct builtin_font
{
	std::string name;
	unsigned char *data;
	unsigned int size;
} builtin_font_t;

static builtin_font_t builtin_fontspecs[1] = {};

#define NUMBER_OF_BUILTIN_FONTS sizeof(builtin_fontspecs) / sizeof(builtin_font)

typedef std::map<std::string, builtin_font_t> builtin_fonts_t;
builtin_fonts_t builtin_fonts;

void initialize_fonts(bool last_chance)
{
	logContext("initializing fonts");
}

/*
 *  Load font from resources and allocate sdl_font_info
 */

sdl_font_info *load_sdl_font(const TextSpec &spec)
{
#if 0
	sdl_font_info *info = NULL;

	// Look for ID/size in list of loaded fonts
	id_and_size_t id_and_size(spec.font, spec.size);
	font_list_t::const_iterator it = font_list.find(id_and_size);
	if (it != font_list.end()) {	// already loaded
		info = it->second;
		info->ref_count++;
		return info;
	}

	// Load font family resource
	LoadedResource fond;
	if (!get_resource(FOUR_CHARS_TO_INT('F', 'O', 'N', 'D'), spec.font, fond)) {
		fprintf(stderr, "Font family resource for font ID %d not found\n", spec.font);
		return NULL;
	}
	SDL_RWops *p = SDL_RWFromMem(fond.GetPointer(), (int)fond.GetLength());
	assert(p);

	// Look for font size in association table
	SDL_RWseek(p, 52, SEEK_SET);
	int num_assoc = SDL_ReadBE16(p) + 1;
	while (num_assoc--) {
		int size = SDL_ReadBE16(p);
		SDL_ReadBE16(p); // skip style
		int id = SDL_ReadBE16(p);
		if (size == spec.size) {

			// Size found, load bitmap font resource
			info = new sdl_font_info;
			if (!get_resource(FOUR_CHARS_TO_INT('N', 'F', 'N', 'T'), id, info->rsrc))
				get_resource(FOUR_CHARS_TO_INT('F', 'O', 'N', 'T'), id, info->rsrc);
			if (info->rsrc.IsLoaded()) {

				// Found, switch stream to font resource
				SDL_RWclose(p);
				p = SDL_RWFromMem(info->rsrc.GetPointer(), (int)info->rsrc.GetLength());
				assert(p);
				void *font_ptr = info->rsrc.GetPointer(true);

				// Read font information
				SDL_RWseek(p, 2, SEEK_CUR);
				info->first_character = static_cast<uint8>(SDL_ReadBE16(p));
				info->last_character = static_cast<uint8>(SDL_ReadBE16(p));
				SDL_RWseek(p, 2, SEEK_CUR);
				info->maximum_kerning = SDL_ReadBE16(p);
				SDL_RWseek(p, 2, SEEK_CUR);
				info->rect_width = SDL_ReadBE16(p);
				info->rect_height = SDL_ReadBE16(p);
				SDL_RWseek(p, 2, SEEK_CUR);
				info->ascent = SDL_ReadBE16(p);
				info->descent = SDL_ReadBE16(p);
				info->leading = SDL_ReadBE16(p);
				int bytes_per_row = SDL_ReadBE16(p) * 2;

				//printf(" first %d, last %d, max_kern %d, rect_w %d, rect_h %d, ascent %d, descent %d, leading %d, bytes_per_row %d\n",
				//	info->first_character, info->last_character, info->maximum_kerning,
				//	info->rect_width, info->rect_height, info->ascent, info->descent, info->leading, bytes_per_row);

				// Convert bitmap to pixmap (1 byte/pixel)
				info->bytes_per_row = bytes_per_row * 8;
				uint8 *src = (uint8 *)font_ptr + SDL_RWtell(p);
				uint8 *dst = info->pixmap = (uint8 *)malloc(info->rect_height * info->bytes_per_row);
				assert(dst);
				for (int y=0; y<info->rect_height; y++) {
					for (int x=0; x<bytes_per_row; x++) {
						uint8 b = *src++;
						*dst++ = (b & 0x80) ? 0xff : 0x00;
						*dst++ = (b & 0x40) ? 0xff : 0x00;
						*dst++ = (b & 0x20) ? 0xff : 0x00;
						*dst++ = (b & 0x10) ? 0xff : 0x00;
						*dst++ = (b & 0x08) ? 0xff : 0x00;
						*dst++ = (b & 0x04) ? 0xff : 0x00;
						*dst++ = (b & 0x02) ? 0xff : 0x00;
						*dst++ = (b & 0x01) ? 0xff : 0x00;
					}
				}
				SDL_RWseek(p, info->rect_height * bytes_per_row, SEEK_CUR);

				// Set table pointers
				int table_size = info->last_character - info->first_character + 3;	// Tables contain 2 additional entries
				info->location_table = (uint16 *)((uint8 *)font_ptr + SDL_RWtell(p));
				byte_swap_memory(info->location_table, _2byte, table_size);
				SDL_RWseek(p, table_size * 2, SEEK_CUR);
				info->width_table = (int8 *)font_ptr + SDL_RWtell(p);

				// Add font information to list of known fonts
				info->ref_count++;
				font_list[id_and_size] = info;

			} else {
				delete info;
				info = NULL;
				fprintf(stderr, "Bitmap font resource ID %d not found\n", id);
			}
		}
	}

	// Free resources
	SDL_RWclose(p);
	return info;
#else
	return nullptr;
#endif
}
#define FONT_PATH "./Fonts.ttf"
static TTF_Font *load_ttf_font(const std::string &path, uint16 style, int16 size)
{
	// already loaded? increment reference counter and return pointer
	ttf_font_key_t search_key(path, style, size);
	ttf_font_list_t::iterator it = ttf_font_list.find(search_key);
	if (it != ttf_font_list.end())
	{
		TTF_Font *font = it->second.first;
		it->second.second++;

		return font;
	}
	TTF_Init();
	TTF_Font *font = 0;
	static const string fontPath[] = { FONT_PATH,
#if defined(__WIN32__)
									   // Path to the below Windows directory
									   // for Windows 7 (Meiryo Bold)
									   "C:/Windows/winsxs/x86_microsoft-windows-f..truetype-meiryobold_31bf3856ad364e35_6.1.7600.16385_none_cd23f5e0d8f9c6fa/meiryob.ttc",
									   "C:/Windows/winsxs/amd64_microsoft-windows-f..truetype-meiryobold_31bf3856ad364e35_6.1.7600.16385_none_2942916491573830/meiryob.ttc",
									   // for Windows Vista
									   "C:/Windows/Fonts/meiryob.ttc",
									   // for less than Windows XP (MS Gothic)
									   "C:/Windows/fonts/msgothic.ttc",
#elif defined(__MACOS__)
									   // for MacOS (Hiragino Kaku Gothic Pro W6)
									   "/System/Library/Fonts/ヒラギノ角ゴ ProN W6.otf",
									   "/System/Library/Fonts/Hiragino Kaku Gothic Pro W6.otf",
									   "/System/Library/Fonts/Cache/HiraginoKakuGothicProNW6.otf" // for iOS
#else
									   // for Linux
									   "/usr/share/fonts/truetype/vlgothic/VL-Gothic-Regular.ttf",
									   "/usr/X11R6/lib/X11/fonts/TrueType/VL-Gothic-Regular.ttf",
									   "/usr/X11/lib/X11/fonts/truetype/VL-Gothic-Regular.ttf",

									   "/usr/share/fonts/truetype/takao/TakaoExGothic.ttf"
									   "/usr/share/fonts/ja-ipafonts/ipag.ttc",

									   "/usr/share/fonts/TrueType/mika.ttf",
									   "/usr/X11R6/lib/X11/fonts/TrueType/mika.ttf",
									   "/usr/X11R6/lib/X11/fonts/truetype/sazanami-gothic.ttf",
									   "/usr/X11/lib/X11/fonts/truetype/sazanami-gothic.ttf",
									   "/usr/share/fonts/TrueType/sazanami-gothic.ttf",
									   "/usr/X11R6/lib/X11/fonts/TrueType/sazanami-gothic.ttf",
									   "/usr/share/fonts/truetype/sazanami-gothic.ttf",
									   "/usr/share/fonts/TrueType/FS-Gothic-gs.ttf",
									   "/usr/X11R6/lib/X11/fonts/TrueType/FS-Gothic.ttf",
									   "/usr/share/fonts/TrueType/gt200001.ttf",
									   "/usr/X11R6/lib/X11/fonts/TrueType/gt200001.ttf",
#endif
	};
	for (int i = 0; i < sizeof(fontPath) / sizeof(fontPath[0]); i++)
	{
		const char *file = fontPath[i].c_str();
		font = TTF_OpenFont(file, size);
		if (!font)
		{
			continue;
		}
		else
		{
			break;
		}
	}
	if (!font)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
								 "cannot open ttf",
								 "TTF is missing.", nullptr);
		exit(1);
	}

	int ttf_style = TTF_STYLE_NORMAL;
	if (style & styleBold)
		ttf_style |= TTF_STYLE_BOLD;
	if (style & styleItalic)
		ttf_style |= TTF_STYLE_ITALIC;
	if (style & styleUnderline)
		ttf_style |= TTF_STYLE_UNDERLINE;

	TTF_SetFontStyle(font, ttf_style);
#ifdef TTF_HINTING_LIGHT
	TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
#endif

	ttf_font_key_t key(path, style, size);
	ref_counted_ttf_font_t value(font, 1);

	ttf_font_list[key] = value;

	return font;	
}

static const char *locate_font(const std::string& path)
{
	builtin_fonts_t::iterator j = builtin_fonts.find(path);
	if (j != builtin_fonts.end() || path == "")
	{
		return path.c_str();
	}
	else
	{
		static FileSpecifier file;
		if (file.SetNameWithPath(path.c_str()))
			return file.GetPath();
		else
			return "";
	}
}
font_info *load_font(const TextSpec &spec)
{
	std::string file;
	file = locate_font(spec.normal);
	ttf_font_info *info = new ttf_font_info;
	for (int i = 0; i < styleMax; ++i)
	{
		TTF_Font *font = load_ttf_font(file, i, spec.size);
		if (font)
		{
			int height;
			TTF_SizeUTF8(font, "あAg", nullptr, &height);

			info->m_line_height = std::max({TTF_FontLineSkip(font),
											TTF_FontHeight(font),
											height});

			info->m_adjust_height = spec.adjust_height;

			info->m_styles[i] = font;
			info->m_keys[i] = ttf_font_key_t(file, i, spec.size);
			for (int c = 32; c < 256; ++c)
			{
				TTF_GlyphMetrics(font, c, nullptr, nullptr, nullptr, nullptr, &info->ascii_width[i][c]);
			}
			TTF_GlyphMetrics(font, u'あ', nullptr, nullptr, nullptr, nullptr, &info->wide_width[i]);
		}
	}
	return info;
}

/*
 *  Unload font, free sdl_font_info
 */

void sdl_font_info::_unload()
{
	// Look for font in list of loaded fonts
	font_list_t::const_iterator i = font_list.begin(), end = font_list.end();
	while (i != end) {
		if (i->second == this) {

			// Found, decrement reference counter and delete
			ref_count--;
			if (ref_count <= 0) {
				delete this; // !
				font_list.erase(i->first);
				return;
			}
		}
		i++;
	}
}

void ttf_font_info::_unload()
{
	for (int i = 0; i < styleMax; ++i)
	{
		ttf_font_list_t::iterator it = ttf_font_list.find(m_keys[i]);
		if (it != ttf_font_list.end())
		{
			--(it->second.second);
			if (it->second.second <= 0)
			{
				TTF_CloseFont(it->second.first);
				ttf_font_list.erase(m_keys[i]);
			}
		}

		m_styles[i] = 0;
	}

	delete this;
}

void unload_font(font_info *info)
{
	info->_unload();
}

int8 sdl_font_info::char_width(uint16 c, uint16 style) const
{
	if (c < first_character || c > last_character)
		return 0;
	int8 width = width_table[(c - first_character) * 2 + 1] + ((style & styleBold) ? 1 : 0);
	if (width == -1)	// non-existant character
		width = width_table[(last_character - first_character + 1) * 2 + 1] + ((style & styleBold) ? 1 : 0);
	return width;
}

uint16 sdl_font_info::_text_width(const char *text, uint16 style, bool) const
{
	int width = 0;
	char c;
	while ((c = *text++) != 0)
		width += char_width(c, style);
	assert(0 <= width);
	assert(width == static_cast<int>(static_cast<uint16>(width)));
	return width;
}

uint16 sdl_font_info::_text_width(const char *text, size_t length, uint16 style, bool) const
{
	int width = 0;
	while (length--)
		width += char_width(*text++, style); 
	assert(0 <= width);
	assert(width == static_cast<int>(static_cast<uint16>(width)));
	return width;
}

int sdl_font_info::_trunc_text(const char *text, int max_width, uint16 style) const
{
	int width = 0;
	int num = 0;
	char c;
	while ((c = *text++) != 0) {
		width += char_width(c, style);
		if (width > max_width)
			break;
		num++;
	}
	return num;
}

// sdl_font_info::_draw_text is in screen_drawing.cpp

int8 ttf_font_info::char_width(uint16 c, uint16 style) const
{
	if (c < 0x100)
	{
		return ascii_width[style][c];
	}
	else if (c >= 0x3040 && c <= 0x9fef)
	{
		return wide_width[style];
	}
	else
	{
		int advance;
		TTF_GlyphMetrics(get_ttf(style), c, 0, 0, 0, 0, &advance);
		return advance;
	}
}
uint16 ttf_font_info::_text_width(const char *text, uint16 style, bool utf8) const
{
	return _text_width(text, strlen(text), style, utf8);
}
#include "converter.h"
uint16 ttf_font_info::_text_width(const char *text, size_t length, uint16 style, bool utf8) const
{
	int width = 0;
	for (const char *c = text; *c;)
	{
		auto ret = next_utf8(c);
		width += char_width(ret.second, style);
		c += ret.first;
	}
	return width;
}

int ttf_font_info::_trunc_text(const char *text, int max_width, uint16 style) const
{
	int width;
	static uint16 temp[1024];
	mac_roman_to_unicode(text, temp, 1024);
	TTF_SizeUNICODE(get_ttf(style), temp, &width, 0);
	if (width < max_width) return strlen(text);

	int num = strlen(text) - 1;

	while (num > 0 && width > max_width)
	{
		num--;
		temp[num] = 0x0;
		TTF_SizeUNICODE(get_ttf(style), temp, &width, 0);
	}

	return num;
}

// ttf_font_info::_draw_text is in screen_drawing.cpp

uint16 font_info::text_width(const char *text, uint16 style, bool utf8) const
{
	if (style & styleShadow)
		return _text_width(text, style, utf8) + 1;
	else
		return _text_width(text, style, utf8);
}

uint16 font_info::text_width(const char *text, size_t length, uint16 style, bool utf8) const
{
	if (style & styleShadow)
		return _text_width(text, length, style, utf8) + 1;
	else
		return _text_width(text, length, style, utf8);
}

static 	inline bool style_code(char c)
{
	switch(tolower(c)) {
	case 'p':
	case 'b':
	case 'i':
	case 'l':
	case 'r':
	case 'c':
	case 's':
		return true;
	default:
		return false;
	}
}

class style_separator
{
public:
	bool operator() (std::string::const_iterator& next, std::string::const_iterator end, std::string& token)
	{
		if (next == end) return false;

		token = std::string();

		// if we start with a token, return it
		if (*next == '|' && next + 1 != end && style_code(*(next + 1)))
		{
			token += *next;
			++next;
			token += *next;
			++next;
			return true;
		}

		token += *next;
		++next;

		// add characters until we hit a token
		for (;next != end && !(*next == '|' && next + 1 != end && style_code(*(next + 1))); ++next)
		{
			token += *next;
		}

		return true;
	}

	void reset() { }

};

static inline bool is_style_token(const std::string& token)
{
	return (token.size() == 2 && token[0] == '|' && style_code(token[1]));
}

static void update_style(uint16& style, const std::string& token)
{
	if (tolower(token[1]) == 'p')
		style &= ~(styleBold | styleItalic);
	else if (tolower(token[1]) == 'b')
	{
		style |= styleBold;
		style &= ~styleItalic;
	}
	else if (tolower(token[1]) == 'i')
	{
		style |= styleItalic;
		style &= ~styleBold;
	}
}


int font_info::draw_styled_text(SDL_Surface *s, const std::string& text, size_t length, int x, int y, uint32 pixel, uint16 style, bool utf8) const 
{
	int width = 0;
	boost::tokenizer<style_separator> tok(text.begin(), text.begin() + length);
	for (boost::tokenizer<style_separator>::iterator it = tok.begin(); it != tok.end(); ++it)
	{
		if (is_style_token(*it))
		{
			update_style(style, *it);
		}
		else
		{
			if (style & styleShadow)
			{
				_draw_text(s, it->c_str(), it->size(), x + width + 1, y + 1, SDL_MapRGB(s->format, 0x0, 0x0, 0x0), style, utf8);
			}
			width += _draw_text(s, it->c_str(), it->size(), x + width, y, pixel, style, utf8);
		}
	}

	return width;
}

int font_info::styled_text_width(const std::string& text, size_t length, uint16 style, bool utf8) const 
{
	int width = 0;
	boost::tokenizer<style_separator> tok(text.begin(), text.begin() + length);
	for (boost::tokenizer<style_separator>::iterator it = tok.begin(); it != tok.end(); ++it)
	{
		if (is_style_token(*it))
		{
			update_style(style, *it);
		}
		else
		{
			width += _text_width(it->c_str(), it->length(), style, utf8);
		}
	}

	if (style & styleShadow)
		return width + 1;
	else
		return width;
}

int font_info::trunc_styled_text(const std::string& text, int max_width, uint16 style) const
{
	if (style & styleShadow)
	{
		max_width -= 1;
		style &= (~styleShadow);
	}

	int length = 0;
	boost::tokenizer<style_separator> tok(text);
	for (boost::tokenizer<style_separator>::iterator it = tok.begin(); it != tok.end(); ++it)
	{
		if (is_style_token(*it))
		{
			update_style(style, *it);
			length += 2;
		}
		else
		{
			int additional_length = _trunc_text(it->c_str(), max_width, style);
			max_width -= _text_width(it->c_str(), additional_length, style);
			length += additional_length;
			if (additional_length < it->size())
				return length;
		}
	}

	return length;
}

std::string font_info::style_at(const std::string& text, std::string::const_iterator pos, uint16 style) const
{
	boost::tokenizer<style_separator> tok(text.begin(), pos);
	for (boost::tokenizer<style_separator>::iterator it = tok.begin(); it != tok.end(); ++it)
	{
		if (is_style_token(*it))
			update_style(style, *it);
	}
	
	if (style & styleBold)
		return string("|b");
	else if (style & styleItalic)
		return string("|i");
	else
		return string();
}

int font_info::draw_text(SDL_Surface *s, const char *text, size_t length, int x, int y, uint32 pixel, uint16 style, bool utf8) const
{
	if (style & styleShadow)
	{
		_draw_text(s, text, length, x + 1, y + 1, SDL_MapRGB(s->format, 0x0, 0x0, 0x0), style, utf8);
	}

	return _draw_text(s, text, length, x, y, pixel, style, utf8);
}

int font_info::trunc_text(const char *text, int max_width, uint16 style) const
{
	if (style & styleShadow)
		return _trunc_text(text, max_width - 1, style);
	else
		return _trunc_text(text, max_width, style);
}
