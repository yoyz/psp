/*
 * intraFont.h
 * This file is used to display the PSP's internal font (pgf firmware files)
 * Version 0.1 - Written in 2007 by BenHur
 *
 * Uses parts of pgeFont by InsertWittyName - http://insomniac.0x89.org
 *
 * This work is licensed under the Creative Commons Attribution-Share Alike 3.0 License.
 * See LICENSE for more details.
 *
 */

#ifndef __INTRAFONT_H__
#define __INTRAFONT_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup intraFont Font Library
 *  @{
 */

#define INTRAFONT_HORADVANCE  0x00 //default: advance horizontaly from one char to the next
#define INTRAFONT_VERTADVANCE 0x01
#define INTRAFONT_LEFTALIGN   0x00 //default: left-align the text
#define INTRAFONT_CENTER      0x02
#define INTRAFONT_RIGHTALIGN  0x04
#define INTRAFONT_VARWIDTH    0x00 //default: variable-width
#define INTRAFONT_FIXEDWIDTH  0x08

/** @note The following definitions are used internally by ::intraFont and have no other relevance.*/
#define PGF_BMP_H_ROWS    0x01
#define PGF_BMP_V_ROWS    0x02
#define PGF_NO_EXTRA1     0x04
#define PGF_NO_EXTRA2     0x08
#define PGF_NO_EXTRA3     0x10
#define PGF_CHARGLYPH     0x20
#define PGF_SHADOWGLYPH   0x40 //warning: this flag is not contained in the metric header flags and is only provided for simpler call to intraFontGetGlyph - ONLY check with (flags & PGF_CHARGLYPH)
#define PGF_CACHED        0x80


/**
 * A Glyph struct
 *
 * @note This is used internally by ::intraFont and has no other relevance.
 */
typedef struct {
	unsigned short x;         //in pixels
	unsigned short y;         //in pixels
	unsigned char width;      //in pixels
	unsigned char height;     //in pixels
	char left;                //in pixels
	char top;                 //in pixels
	unsigned char flags;
	unsigned short shadowID;  //to look up in shadowmap
	char advance;             //in quarterpixels
	unsigned long ptr;        //offset
} Glyph;

/**
 * A PGF_Header struct
 *
 * @note This is used internally by ::intraFont and has no other relevance.
 */
typedef struct {
	unsigned short header_start;
	unsigned short header_len;
	char pgf_id[4];
	unsigned long revision;
	unsigned long version;
	unsigned long charmap_len;
	unsigned long charptr_len;
	unsigned long charmap_bpe;
	unsigned long charptr_bpe;
	unsigned char junk00[21];
	unsigned char family[64];
	unsigned char style[64];
	unsigned char junk01[1];
	unsigned short charmap_min;
	unsigned short charmap_max;
	unsigned char junk02[50];
	unsigned long fixedsize[2];
	unsigned char junk03[14];
	unsigned char table1_len;
	unsigned char table2_len;
	unsigned char table3_len;
	unsigned char advance_len;
	unsigned char junk04[102];
	unsigned long shadowmap_len;
	unsigned long shadowmap_bpe;
	unsigned char junk05[4];
	unsigned long shadowscale[2];
	//currently no need ;
} PGF_Header;

/**
 * A Font struct
 */
typedef struct {
	char *filename;
	unsigned char *fontdata;

	unsigned char *texture; /**<  The bitmap data  */
	unsigned int texWidth; /**<  Texture size (power2) */
	unsigned int texHeight; /**<  Texture height (power2) */
	unsigned short texX;
	unsigned short texY;
	unsigned short texYSize;

	unsigned short n_chars;
	char advancex;            //in quarterpixels
	char advancey;            //in quarterpixels
	unsigned char charmap_compr_len; /**<length of compression info*/
	unsigned short *charmap_compr; /**< Compression info on compressed charmap*/
	unsigned short *charmap; /**<  Character map */
	Glyph* glyph; /**<  Character glyphs */

	unsigned short n_shadows;
	unsigned char shadowscale; /**<  shadows in pgf file (width, height, left and top properties as well) are scaled by factor of (shadowscale>>6) */
	Glyph* shadowGlyph; /**<  Shadow glyphs */

	float size;
	unsigned int color;
	unsigned int shadowColor;
	unsigned short options;
} intraFont;


/**
 * Initialise the Font library
 *
 * @returns 1 on success.
 */
int intraFontInit(void);

/**
 * Shutdown the Font library
 */
void intraFontShutdown(void);

/**
 * Load a pgf font.
 *
 * @param filename - Path to the font
 *
 * @param flags - PGF_NOCACHE or 0
 *
 * @returns A ::intraFont struct
 */
intraFont* intraFontLoad(const char *filename);

/**
 * Free the specified font.
 *
 * @param font - A valid ::intraFont
 */
void intraFontUnload(intraFont *font);

/**
 * Activate the specified font.
 *
 * @param font - A valid ::intraFont
 */
void intraFontActivate(intraFont *font);

/**
 * Set font style
 *
 * @param font - A valid ::intraFont
 *
 * @param size - Text size
 *
 * @param color - Text color
 *
 * @param shadowColor - Shadow color (use 0 for no shadow)
 *
 * @param options - INTRAFONT_XXX flags as defined above (ored together)
 */
void intraFontSetStyle(intraFont *font, float size, unsigned int color, unsigned int shadowColor, unsigned short options);

/**
 * Draw UCS-2 encoded text along the baseline starting at x, y.
 *
 * @param font - A valid ::intraFont
 *
 * @param x - X position on screen
 *
 * @param y - Y position on screen
 *
 * @param color - Text color
 *
 * @param shadowColor - Shadow color (use 0 for no shadow)
 *
 * @param text - Text to draw
 *
 * @returns The total width of the text drawn.
 */
int intraFontPrintUCS2(intraFont *font, float x, float y, const unsigned short *text);

/**
 * Draw text along the baseline starting at x, y.
 *
 * @param font - A valid ::intraFont
 *
 * @param x - X position on screen
 *
 * @param y - Y position on screen
 *
 * @param text - Text to draw
 *
 * @returns The total width of the text drawn.
 */
int intraFontPrint(intraFont *font, float x, float y, const char *text);

/**
 * Draw text along the baseline starting at x, y (with formatting).
 *
 * @param font - A valid ::intraFont
 *
 * @param x - X position on screen
 *
 * @param y - Y position on screen
 *
 * @param text - Text to draw
 *
 * @returns The total width of the text drawn.
 */
int intraFontPrintf(intraFont *font, float x, float y, const char *text, ...);

/**
 * Measure a length of text as if it were to be drawn
 *
 * @param font - A valid ::intraFont
 *
 * @param text - Text to measure
 *
 * @returns The total width of the text.
 */
int intraFontMeasureText(intraFont *font, const char *text);

/**
 * Measure a length of UCS-2 encoded text as if it were to be drawn
 *
 * @param font - A valid ::intraFont
 *
 * @param text - Text to measure
 *
 * @returns The total width of the text.
 */
int intraFontMeasureTextUCS2(intraFont *font, const unsigned short *text);

/** @} */

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __INTRAFONT_H__
