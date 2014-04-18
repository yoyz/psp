/*
 * intraFont.c
 * This file is used to display the PSP's internal font (pgf firmware files)
 * Version 0.1 - Written in 2007 by BenHur
 *
 * Uses parts of pgeFont by InsertWittyName - http://insomniac.0x89.org
 *
 * This work is licensed under the Creative Commons Attribution-Share Alike 3.0 License.
 * See LICENSE for more details.
 *
 */

#include <pspkernel.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspdisplay.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <malloc.h>

#include "intraFont.h"

static const int INTRA_FONT_TEXTURE_SIZE = 512;
static unsigned int __attribute__((aligned(16))) clut[16];


unsigned long intraFontGetV(unsigned long n, unsigned char *p, unsigned long *b) {
	unsigned long i,v=0;
	for(i=0;i<n;i++) {
	    v += ( ( p[(*b)/8] >> ((*b)%8) ) & 1) << i;
		(*b)++;
	}
	return v;
}

unsigned long* intraFontGetTable(FILE *file, unsigned long n_elements, unsigned long bp_element) {
	unsigned long len_table = ((n_elements*bp_element+31)/32)*4;
	unsigned char *raw_table = (unsigned char*)malloc(len_table*sizeof(unsigned char));
	if (raw_table == NULL) return NULL;
	if (fread(raw_table, len_table*sizeof(unsigned char), 1, file) != 1) {
		free(raw_table);
		return NULL;
	}
	unsigned long *table = (unsigned long*)malloc(n_elements*sizeof(unsigned long));
	if (table == NULL) {
		free(raw_table);
		return NULL;
	}
	unsigned long i,j=0;
	for (i=0;i<n_elements;i++) {
		table[i] = intraFontGetV(bp_element,raw_table,&j);
	}
	free(raw_table);
	return table;
}

unsigned char intraFontGetBMP(intraFont *font, unsigned short id, unsigned char glyphtype) {
	Glyph *glyph;
	if (glyphtype & PGF_CHARGLYPH) {
		glyph = &(font->glyph[id]);
	} else {
		glyph = &(font->shadowGlyph[id]);
	}
	unsigned long b = glyph->ptr*8;

	if (glyph->width > 0 && glyph->height > 0) {
		if (!(glyph->flags & PGF_BMP_H_ROWS) != !(glyph->flags & PGF_BMP_V_ROWS)) { //H_ROWS xor V_ROWS
			if ((font->texX + glyph->width) > font->texWidth) {
				font->texY += font->texYSize;
				font->texX = 0;
			}
			if ((font->texY + glyph->height) > font->texHeight) {
				font->texY = 0;
				font->texX = 1;
			}
			glyph->x=font->texX;
			glyph->y=font->texY;

			/*draw bmp*/
			int i=0,j,xx,yy;
			unsigned char nibble, value = 0;
			while (i<(glyph->width*glyph->height)) {
				nibble = intraFontGetV(4,font->fontdata,&b);
				if (nibble < 8) value = intraFontGetV(4,font->fontdata,&b);
				for (j=0; (j<=((nibble<8)?(nibble):(15-nibble))) && (i<(glyph->width*glyph->height)); j++) {
					if (nibble >= 8) value = intraFontGetV(4,font->fontdata,&b);
					if (glyph->flags & PGF_BMP_H_ROWS) {
						xx = i%glyph->width;
						yy = i/glyph->width;
					} else {
						xx = i/glyph->height;
						yy = i%glyph->height;
					}
					if ((font->texX + xx) & 1) {
						font->texture[((font->texX + xx) + (font->texY + yy) * font->texWidth)>>1] &= 0x0F;
						font->texture[((font->texX + xx) + (font->texY + yy) * font->texWidth)>>1] |= (value<<4);
					} else {
						font->texture[((font->texX + xx) + (font->texY + yy) * font->texWidth)>>1] &= 0xF0;
						font->texture[((font->texX + xx) + (font->texY + yy) * font->texWidth)>>1] |= (value);
					}
					i++;
				}
			}
			font->texX += glyph->width+1; //add empty gap to prevent interpolation artifacts from showing

			//mark dirty glyphs as uncached
			for (i = 0; i < font->n_chars; i++) {
				if ( (font->glyph[i].flags & PGF_CACHED) && (font->glyph[i].y == glyph->y) ) {
					if ( (font->glyph[i].x+font->glyph[i].width) > glyph->x && font->glyph[i].x < (glyph->x+glyph->width) ) {
						font->glyph[i].flags -= PGF_CACHED;
					}
				}
			}
			for (i = 0; i < font->n_shadows; i++) {
				if ( (font->shadowGlyph[i].flags & PGF_CACHED) && (font->shadowGlyph[i].y == glyph->y) ) {
					if ( (font->shadowGlyph[i].x+font->shadowGlyph[i].width) > glyph->x && font->shadowGlyph[i].x < (glyph->x+glyph->width) ) {
						font->shadowGlyph[i].flags -= PGF_CACHED;
					}
				}
			}
		} else if ((glyph->flags & PGF_BMP_H_ROWS) && (glyph->flags & PGF_BMP_V_ROWS)) {
			/*overlay glyphs*/
			//printf("Overlay glyphs not yet implemented.\n");
		} //else printf("Unknown flags: %d\n",glyph->flags);
	} else {
		glyph->x=0;
		glyph->y=0;
		//printf("No image in this glyph.\n");
	}
	glyph->flags |= PGF_CACHED;
	return 1;
}

unsigned short intraFontGetGlyph(unsigned char *data, unsigned long *b, unsigned char glyphtype, signed long *advancemap, Glyph *glyph) {
    if (glyphtype & PGF_CHARGLYPH) {
        (*b) += 14; //skip offset pos value of shadow
    } else {
        (*b) += intraFontGetV(14,data,b)*8+14; //skip to shadow
    }
    glyph->width=intraFontGetV(7,data,b);
	glyph->height=intraFontGetV(7,data,b);
	glyph->left=intraFontGetV(7,data,b);
	if (glyph->left >= 64) glyph->left -= 128;
	glyph->top=intraFontGetV(7,data,b);
	if (glyph->top >= 64) glyph->top -= 128;
    glyph->flags = intraFontGetV(6,data,b);
    if (glyph->flags & PGF_CHARGLYPH) {
        (*b) += 7; //skip magic number
		glyph->shadowID = intraFontGetV(9,data,b);
		(*b) += 24 + ((glyph->flags & PGF_NO_EXTRA1)?0:56) + ((glyph->flags & PGF_NO_EXTRA2)?0:56) + ((glyph->flags & PGF_NO_EXTRA3)?0:56); //skip to 6bits before end of header
		glyph->advance = advancemap[intraFontGetV(8,data,b)*2]/16;
    } else {
		glyph->shadowID = 65535;
        glyph->advance = 0;
    }
	glyph->ptr = (*b)/8;
    return 1;
}

unsigned short intraFontGetID(intraFont* font, unsigned short ucs) {
	unsigned short j, id = 0;
	char found = 0;
	for (j = 0; j < font->charmap_compr_len && !found; j++) {
		if ((ucs >= font->charmap_compr[j*2]) && (ucs < (font->charmap_compr[j*2]+font->charmap_compr[j*2+1]))) {
			id += ucs - font->charmap_compr[j*2];
			found = 1;
		} else {
			id += font->charmap_compr[j*2+1];
		}
	}
	if (!found) return 65535;
	id = font->charmap[id];
	if (id >= font->n_chars) id = 65535;
	return id;
}

intraFont* intraFontLoad(const char *filename) {
    unsigned long i,j;

    //open pgf file and get file size
    FILE *file = fopen(filename, "rb"); /* read from the file in binary mode */
	if ( file == NULL ) {
		return NULL;
	}
	fseek(file, 0, SEEK_END);
    unsigned long filesize = ftell(file);
	fseek(file, 0, SEEK_SET);

	//read pgf header
	PGF_Header header;
	if (fread(&header, sizeof(PGF_Header), 1, file) != 1) {
		fclose(file);
		return NULL;
	}

	//pgf header tests: valid and known pgf file?
	if ((strncmp(header.pgf_id,"PGF0",4) != 0) || (header.version != 6) || (header.revision != 2 && header.revision != 2) ||
		(header.charmap_len > 65535) || (header.charptr_len > 65535) || (header.charmap_bpe > 32) || (header.charptr_bpe > 32) ||
		(header.charmap_min > header.charmap_max) || (header.shadowmap_len > 511) || (header.shadowmap_bpe > 16)) {
	    fclose(file);
		return NULL;
	}

	//create and initialize font structure
	intraFont* font = (intraFont*)malloc(sizeof(intraFont));
	if (font == NULL) {
		fclose(file);
		return NULL;
	}

	font->n_chars = (unsigned short)header.charptr_len;
	font->charmap_compr_len = (header.revision == 3)?7:1;
	font->texWidth = INTRA_FONT_TEXTURE_SIZE;
	font->texHeight = INTRA_FONT_TEXTURE_SIZE;
	font->texX = 1;
	font->texY = 0;
	font->texYSize = 0;
	font->advancex = header.fixedsize[0]/16;
	font->advancey = header.fixedsize[1]/16;
	font->n_shadows = (unsigned short)header.shadowmap_len;
	font->shadowscale = (unsigned char)header.shadowscale[0];
    font->options = 0;               //default options
    font->size = 1.0f;               //default size
    font->color = 0xFFFFFFFF;        //non-transparent white
    font->shadowColor = 0xFF000000;  //non-transparent black

	font->filename = (char*)malloc((strlen(filename)+1)*sizeof(char));
	font->glyph = (Glyph*)malloc(font->n_chars*sizeof(Glyph));
	font->shadowGlyph = (Glyph*)malloc(font->n_shadows*sizeof(Glyph));
	font->charmap_compr = (unsigned short*)malloc(font->charmap_compr_len*sizeof(unsigned short)*2);
	font->charmap = (unsigned short*)malloc(header.charmap_len*sizeof(unsigned short));
	font->texture = (unsigned char*)malloc(font->texWidth*font->texHeight>>1);

	if (!font->filename || !font->glyph || !font->shadowGlyph || !font->charmap_compr || !font->charmap || !font->texture) {
		fclose(file);
		intraFontUnload(font);
		return NULL;
	}
	strcpy(font->filename,filename);

	//read advance table
    fseek(file, header.header_len+(header.table1_len+header.table2_len+header.table3_len)*8, SEEK_SET);
	signed long *advancemap = (signed long*)malloc(header.advance_len*sizeof(signed long)*2);
	if (!advancemap) {
		fclose(file);
		intraFontUnload(font);
		return NULL;
	}
	if (fread(advancemap, header.advance_len*sizeof(signed long)*2, 1, file) != 1) {
		free(advancemap);
		fclose(file);
		intraFontUnload(font);
		return NULL;
	}

	//read shadowmap
	unsigned long *ucs_shadowmap = intraFontGetTable(file, header.shadowmap_len, header.shadowmap_bpe);
	if (ucs_shadowmap == NULL) {
		free(advancemap);
		fclose(file);
		intraFontUnload(font);
		return NULL;
	}

	//version 6.3 charmap compression
	if (header.revision == 3) {
		if (fread(font->charmap_compr, font->charmap_compr_len*sizeof(unsigned short)*2, 1, file) != 1) {
			free(advancemap);
		    free(ucs_shadowmap);
			fclose(file);
			intraFontUnload(font);
			return NULL;
		}
	} else {
		font->charmap_compr[0] = header.charmap_min;
		font->charmap_compr[1] = header.charmap_len;
	}

	//read charmap
	unsigned long *id_charmap = intraFontGetTable(file, header.charmap_len, header.charmap_bpe);
	if (id_charmap == NULL) {
		free(advancemap);
	    free(ucs_shadowmap);
		fclose(file);
		intraFontUnload(font);
		return NULL;
	}
	for (i=0;i<header.charmap_len;i++) {
		font->charmap[i]=(unsigned short)((id_charmap[i] < font->n_chars)?id_charmap[i]:65535);
	}
	free(id_charmap);

	//read charptr
	unsigned long *charptr = intraFontGetTable(file, header.charptr_len, header.charptr_bpe);
	if (charptr == NULL) {
		free(advancemap);
	    free(ucs_shadowmap);
		fclose(file);
		intraFontUnload(font);
		return NULL;
	}

	//read raw fontdata
	unsigned long start_fontdata = ftell(file);
	unsigned long len_fontdata = filesize-start_fontdata;
	font->fontdata = (unsigned char*)malloc(len_fontdata*sizeof(unsigned char));
	if (font->fontdata == NULL) {
		free(advancemap);
	    free(ucs_shadowmap);
		free(charptr);
		fclose(file);
		intraFontUnload(font);
		return NULL;
	}
	if (fread(font->fontdata, len_fontdata*sizeof(unsigned char), 1, file) != 1) {
		free(advancemap);
		free(ucs_shadowmap);
		free(charptr);
		fclose(file);
		intraFontUnload(font);
		return NULL;
	}

	//close file
	fclose(file);

	//populate chars
	for (i=0;i<font->n_chars;i++) {
		j = charptr[i]*4*8;
		if (!intraFontGetGlyph(font->fontdata, &j, PGF_CHARGLYPH, advancemap, &(font->glyph[i]))) {
			free(advancemap);
			free(ucs_shadowmap);
			free(charptr);
			intraFontUnload(font);
			return NULL;
		}
		if (font->glyph[i].height > font->texYSize) font->texYSize = font->glyph[i].height;
	}

	//populate shadows
	unsigned short char_id,shadow_id;
	for (i = 0; i<font->n_shadows; i++) {
		char_id = intraFontGetID(font,ucs_shadowmap[i]);
		if (char_id < font->n_chars) {
			j = charptr[char_id]*4*8;
			shadow_id = font->glyph[char_id].shadowID;
			intraFontGetGlyph(font->fontdata, &j, PGF_SHADOWGLYPH, NULL, &(font->shadowGlyph[shadow_id]));
			if (font->shadowGlyph[shadow_id].height > font->texYSize) font->texYSize = font->shadowGlyph[shadow_id].height;
		}
	}

	//free temp stuff
	free(advancemap);
	free(ucs_shadowmap);
	free(charptr);

	return font;
}

void intraFontUnload(intraFont *font) {
    if (font->filename) free(font->filename);
    if (font->fontdata) free(font->fontdata);
	if (font->texture) free(font->texture);
	if (font->charmap_compr) free(font->charmap_compr);
	if (font->charmap) free(font->charmap);
	if (font->glyph) free(font->glyph);
	if (font->shadowGlyph) free(font->shadowGlyph);
	if (font) free(font);
}

int intraFontInit(void) {
	int n;
	for(n = 0; n < 16; n++)
		clut[n] = ((n * 17) << 24) | 0xffffff;
	return 1;
}

void intraFontShutdown(void) {
	//Nothing yet
}

void intraFontActivate(intraFont *font) {
	if(!font) return;
	if(!font->texture) return;

	sceGuClutMode(GU_PSM_8888, 0, 255, 0);
	sceGuClutLoad(2, clut);

	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(GU_PSM_T4, 0, 0, 0);
	sceGuTexImage(0, font->texWidth, font->texHeight, font->texWidth, font->texture);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	sceGuTexEnvColor(0x0);
	sceGuTexOffset(0.0f, 0.0f);
	sceGuTexWrap(GU_REPEAT, GU_REPEAT);
	sceGuTexFilter(GU_LINEAR, GU_LINEAR);
}

void intraFontSetStyle(intraFont *font, float size, unsigned int color, unsigned int shadowColor, unsigned short options) {
	if (!font) return;
	font->size = size;
	font->color = color;
	font->shadowColor = shadowColor;
	font->options = options;
}

int intraFontPrintf(intraFont *font, float x, float y, const char *text, ...) {
	if(!font) return 0;

	char buffer[256];
	va_list ap;

	va_start(ap, text);
	vsnprintf(buffer, 256, text, ap);
	va_end(ap);
	buffer[255] = 0;

	return intraFontPrint(font, x, y, buffer);
}

int intraFontPrint(intraFont *font, float x, float y, const char *text) {
    if (!text || strlen(text) == 0 || !font) return 0;
    unsigned short* ucs2_text = (unsigned short*)malloc((strlen(text)+1)*sizeof(unsigned short));
    if (!ucs2_text) return 0;
    int i;
    for (i = 0; i < strlen(text); i++) ucs2_text[i] = text[i];
    ucs2_text[i] = 0;
    i = intraFontPrintUCS2(font, x, y, ucs2_text);
    free(ucs2_text);
    return i;
}

int intraFontPrintUCS2(intraFont *font, float x, float y, const unsigned short *text) {
	if(!font) return 0;

	int i, length = 0;
	typedef struct {
		float u, v;
		unsigned int c;
		float x, y, z;
	} fontVertex;
	fontVertex *v, *v0, *v1, *s0, *s1;
	float glyphscale = font->size;
	float startx = x;

	while (text[length] > 0) length++;
	if (length == 0) return 0;

	v = sceGuGetMemory((sizeof(fontVertex)<<2) * length);

	for(i = 0; i < length; i++) {
		unsigned short char_id = intraFontGetID(font,text[i]);
		if (char_id >= font->n_chars) char_id = 0;

		Glyph *glyph = &(font->glyph[char_id]);
		Glyph *shadowGlyph = &(font->shadowGlyph[font->glyph[char_id].shadowID]);

		if (!(glyph->flags & PGF_CACHED)) {
			intraFontGetBMP(font,char_id,PGF_CHARGLYPH);
		}
		if (!(shadowGlyph->flags & PGF_CACHED)) {
			intraFontGetBMP(font,glyph->shadowID,PGF_SHADOWGLYPH);
		}

		v0 = &v[((length+i)<<1) + 0];
		v1 = &v[((length+i)<<1) + 1];

		v0->u = glyph->x;
		v0->v = glyph->y;
		v0->c = font->color;
		v0->x = x + glyph->left*glyphscale;
		v0->y = y - glyph->top *glyphscale;
		v0->z = 0.0f;

		v1->u = glyph->x + glyph->width +0.5f;
		v1->v = glyph->y + glyph->height+0.5f;
		v1->c = font->color;
		v1->x = v0->x + glyph->width *glyphscale;
		v1->y = v0->y + glyph->height*glyphscale;
		v1->z = 0.0f;

		s0 = &v[(i<<1) + 0];
		s1 = &v[(i<<1) + 1];

		s0->u = shadowGlyph->x;
		s0->v = shadowGlyph->y;
		s0->c = font->shadowColor;
		s0->x = x + (shadowGlyph->left*64)/font->shadowscale*glyphscale;
		s0->y = y - (shadowGlyph->top *64)/font->shadowscale*glyphscale;
		s0->z = 0.0f;

		s1->u = shadowGlyph->x + shadowGlyph->width+0.5;
		s1->v = shadowGlyph->y + shadowGlyph->height+0.5;
		s1->c = font->shadowColor;
		s1->x = s0->x + (shadowGlyph->width *64)/font->shadowscale*glyphscale;
		s1->y = s0->y + (shadowGlyph->height*64)/font->shadowscale*glyphscale;
		s1->z = 0.0f;

		if (text[i] == '\n') {
			x = startx;
			y += font->advancey * glyphscale / 4.0;
		} else {
			x += ((font->options & INTRAFONT_FIXEDWIDTH) ? font->advancex*0.5f : glyph->advance) * glyphscale / 4.0;
		}
	}

	//finalize and activate texture
	sceKernelDcacheWritebackAll();
	intraFontActivate(font);

	sceGuDisable(GU_DEPTH_TEST);
	sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, length<<2, 0, v);
	sceGuEnable(GU_DEPTH_TEST);

	return x;
}

int intraFontMeasureText(intraFont *font, const char *text) {
    if (!text || strlen(text) == 0 || !font) return 0;
    unsigned short* ucs2_text = (unsigned short*)malloc((strlen(text)+1)*sizeof(unsigned short));
    if (!ucs2_text) return 0;
    int i;
    for (i = 0; i < strlen(text); i++) ucs2_text[i] = text[i];
    ucs2_text[i] = 0;
    i = intraFontMeasureTextUCS2(font, ucs2_text);
    free(ucs2_text);
    return i;
}

int intraFontMeasureTextUCS2(intraFont *font, const unsigned short *text) {
	if(!font) return 0;

	int i;
	int length = 0;
	int x = 0;

	float glyphscale = font->size;

	while (text[length] > 0) length++;
	if (length == 0) return 0;

	for(i = 0; i < length; i++) {
		unsigned short char_id = intraFontGetID(font,text[i]);
		if (char_id >= font->n_chars) char_id = 0;

		Glyph *glyph = &(font->glyph[char_id]);

		if (!(glyph->flags & PGF_CACHED)) {
			intraFontGetBMP(font,char_id,PGF_CHARGLYPH);
		}

		x += ((font->options & INTRAFONT_FIXEDWIDTH) ? font->advancex*0.5f : glyph->advance) * glyphscale / 4.0;
	}

	return x;
}
