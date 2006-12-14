/*
  RGBAImage.h
  Copyright (C) 2006 Yangli Hector Yee

  This program is free software; you can redistribute it and/or modify it under the terms of the
  GNU General Public License as published by the Free Software Foundation; either version 2 of the License,
  or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with this program;
  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef _RGAIMAGE_H
#define _RGBAIMAGE_H

#include<string>
#include<cairo.h>

// assumes data is in the ABGR format
class RGBAImage
{
public:
    RGBAImage() { Width = 0; Height = 0; Data = 0; }
    RGBAImage(int w, int h, const char *name = 0)
	{
	    Width = w;
	    Height = h;
	    if (name) Name = name;
	    Data = new unsigned int[w * h];
	};
    ~RGBAImage() { if (Data) delete[] Data; }
    virtual unsigned char Get_Red(unsigned int i) { return (Data[i] & 0xFF); }
    virtual unsigned char Get_Green(unsigned int i) { return ((Data[i]>>8) & 0xFF); }
    virtual unsigned char Get_Blue(unsigned int i) { return ((Data[i]>>16) & 0xFF); }
    virtual unsigned char Get_Alpha(unsigned int i) { return ((Data[i]>>24) & 0xFF); }
    virtual void Set(unsigned char r, unsigned char g, unsigned char b, unsigned char a, unsigned int i)
	{ Data[i] = r | (g << 8) | (b << 16) | (a << 24); }
    int Get_Width(void) const { return Width; }
    int Get_Height(void) const { return Height; }
    virtual void Set(int x, int y, unsigned int d) { Data[x + y * Width] = d; }
    virtual unsigned int Get(int x, int y) const { return Data[x + y * Width]; }
    virtual unsigned int Get(int i) const { return Data[i]; }
    const std::string &Get_Name(void) const { return Name; }

    bool WritePPM();
    static RGBAImage* ReadTiff(char *filename);
    static RGBAImage* ReadPNG(char *filename);
protected:
    int Width;
    int Height;
    std::string Name;
    unsigned int *Data;
};

class RGBACairoImage : public RGBAImage
{
public:
    RGBACairoImage (cairo_surface_t *surface)
	{
	    Width = cairo_image_surface_get_width (surface);
	    Height = cairo_image_surface_get_height (surface);
	    Data = (unsigned int *) cairo_image_surface_get_data (surface);
	    if (cairo_image_surface_get_stride (surface) != 4 * Width) {
		fprintf (stderr, "Error: Currently only support images where stride == 4 * width\n");
		exit (1);
	    }
	}
    ~RGBACairoImage() { }

    unsigned int ARGB_to_ABGR(unsigned int pixel) const {
	unsigned int new_pixel;
	new_pixel  =  pixel & 0xff00ff00;
	new_pixel |= (pixel & 0x00ff0000) >> 16;
	new_pixel |= (pixel & 0x000000ff) << 16;
	return new_pixel;
    }
    unsigned int Get_Unpremultiply(unsigned int i) const {
	uint32_t pixel;
	uint8_t alpha;

	pixel = Data[i];

	alpha = (pixel & 0xff000000) >> 24;

	if (alpha == 0)
	    return 0;

	return (alpha << 24) |
	    ((((pixel & 0xff0000) >> 16) * 255 + alpha / 2) / alpha) << 16 |
	    ((((pixel & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha) <<  8 |
	    ((((pixel & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha) <<  0;
    }
    unsigned char Get_Alpha(unsigned int i) { return (Get_Unpremultiply(i) & 0xff000000) >> 24;}
    unsigned char Get_Red(unsigned int i)   { return (Get_Unpremultiply(i) & 0x00ff0000) >> 16;}
    unsigned char Get_Green(unsigned int i) { return (Get_Unpremultiply(i) & 0x0000ff00) >>  8;}
    unsigned char Get_Blue(unsigned int i)  { return (Get_Unpremultiply(i) & 0x000000ff) >>  0;}
    unsigned int Get(int x, int y) const { return Get(Width * y + x); }
    unsigned int Get(int i) const { return ARGB_to_ABGR(Get_Unpremultiply(i)); }
};

#endif
