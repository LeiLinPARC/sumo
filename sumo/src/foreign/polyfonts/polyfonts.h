#ifndef _POLYFONTS_H_
#define _POLYFONTS_H_

/*
  Polyfonts is a polygon font drawing library for use with SDL. Any
  TTF font can be converted for use with this library. Contact the
  author for details.

  Copyright (C) 2003 Bob Pendleton

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public License
  as published by the Free Software Foundation, either version 3.1
  of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free
  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  02111-1307 USA

  If you do not wish to comply with the terms of the LGPL please
  contact the author as other terms are available for a fee.

  Bob Pendleton
  Bob@Pendleton.com
*/

//
#ifdef _MSC_VER
#include <windows_config.h>
#else
#include <config.h>
#endif

//#include "SDL.h"
#include "polyfonttypes.h"

#ifdef __cplusplus
extern "C"
{
#endif

    int     pfSetFont(pffont *f);
#ifdef POLYFONTS_WANTS_IO
    pffont *pfLoadFont(char *fileName);
#endif
    void    pfUnloadFont(pffont *f);
    char   *pfGetFontName();
    pffont *pfGetCurrentFont();
    int     pfGetFontBBox(double *minx, double *miny, double *maxx, double *maxy);
    double   pfGetFontHeight();
    double   pfGetFontWidth();
    double   pfGetFontAscent();
    double   pfGetFontDescent();

    int     pfGetFontNumGlyphs();
    wchar_t pfGetChar(int glyph);

    void    pfSetScale(double s);
    void    pfSetScaleXY(double sx, double sy);
    int     pfSetScaleBox(char *c, double w, double h);
    int     pfSetScaleBoxW(wchar_t *c, double w, double h);

    void    pfSetPosition(double x, double y);
    void    pfGetPosition(double *x, double *y);

    void    pfSetSkew(double s);
    void    pfSetWeight(int w);
    void    pfSetAngleR(double a);
    void    pfSetAngleD(double a);
    void    pfSetCenter(int onOff);

    int     pfGetCharBBox(wchar_t c, double *minx, double *miny, double *maxx, double *maxy);
    double   pfGetCharAdvance(wchar_t c);
    double   pfGetCharHeight(wchar_t c);
    double   pfGetCharWidth(wchar_t c);
    double   pfGetCharAscent(wchar_t c);
    double   pfGetCharDescent(wchar_t c);

    double pfdkGetStringWidth(const char *c);

    int     pfDrawChar(/*SDL_Surface *s,*/ wchar_t c);

    int     pfDrawString(/*SDL_Surface *s,*/ const char *c);
    int     pfDrawStringW(/*SDL_Surface *s,*/ wchar_t *c);

#ifdef __cplusplus
}
#endif

#endif
