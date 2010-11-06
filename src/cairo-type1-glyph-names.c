/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2006 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Kristian Høgsberg <krh@redhat.com>
 */

#include "cairoint.h"

#if CAIRO_HAS_FONT_SUBSET

#include "cairo-type1-private.h"
#include "cairo-scaled-font-subsets-private.h"

#if 0
/*
 * The two tables that follow are generated using this perl code:
 */

@encoding = (
	/*   0 */
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	/*  16 */
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	/*  32 */
	"space",	"exclam",	"quotedbl",	"numbersign",
	"dollar",	"percent",	"ampersand",	"quoteright",
	"parenleft",	"parenright",	"asterisk",	"plus",
	"comma",	"hyphen",	"period",	"slash",
	/*  48 */
	"zero",		"one",		"two",		"three",
	"four",		"five",		"six",		"seven",
	"eight",	"nine",		"colon",	"semicolon",
	"less",		"equal",	"greater",	"question",
	/*  64 */
	"at",		"A",		"B",		"C",
	"D",		"E",		"F",		"G",
	"H",		"I",		"J",		"K",
	"L",		"M",		"N",		"O",
	/*  80 */
	"P",		"Q",		"R",		"S",
	"T",		"U",		"V",		"W",
	"X",		"Y",		"Z",		"bracketleft",
	"backslash",	"bracketright",	"asciicircum",	"underscore",
	/*  96 */
	"quoteleft",	"a",		"b",		"c",
	"d",		"e",		"f",		"g",
	"h",		"i",		"j",		"k",
	"l",		"m",		"n",		"o",
	/* 112 */
	"p",		"q",		"r",		"s",
	"t",		"u",		"v",		"w",
	"x",		"y",		"z",		"braceleft",
	"bar",		"braceright",	"asciitilde",	NULL,
	/* 128 */
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	/* 144 */
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	/* 160 */
	NULL,		"exclamdown",	"cent",		"sterling",
	"fraction",	"yen",		"florin",	"section",
	"currency",	"quotesingle",	"quotedblleft",	"guillemotleft",
	"guilsinglleft","guilsinglright","fi",		"fl",
	/* 176 */
	NULL,		"endash",	"dagger",	"daggerdbl",
	"periodcentered",NULL,		"paragraph",	"bullet",
	"quotesinglbase","quotedblbase","quotedblright","guillemotright",
	"ellipsis",	"perthousand",	NULL,		"questiondown",
	/* 192 */
	NULL,		"grave",	"acute",	"circumflex",
	"tilde",	"macron",	"breve",	"dotaccent",
	"dieresis",	NULL,		"ring",		"cedilla",
	NULL,		"hungarumlaut",	"ogonek",	"caron",
	/* 208 */
	"emdash",	NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	/* 224 */
	NULL,		"AE",		NULL,		"ordfeminine",
	NULL,		NULL,		NULL,		NULL,
	"Lslash",	"Oslash",	"OE",		"ordmasculine",
	NULL,		NULL,		NULL,		NULL,
	/* 240 */
	NULL,		"ae",		NULL,		NULL,
	NULL,		"dotlessi",	NULL,		NULL,
	"lslash",	"oslash",	"oe",		"germandbls",
	NULL,		NULL,		NULL,		NULL
	);

print "static const char ps_standard_encoding_symbol[] = {\n";
$s = qq( "\\0");
for $sym (@encoding) {
    if (! ($sym eq NULL)) {
        $ss = qq( "$sym\\0");
	if (length($s) + length($ss) > 78) {
	  print qq( $s\n);
	  $s = "";
	}
	$s .= $ss;
    }
}
print qq( $s\n);
print "};\n\n";
print "static const int16_t ps_standard_encoding_offset[256] = {\n";
$offset = 1;
$s = qq();
for $sym (@encoding) {
    if (! ($sym eq NULL)) {
	$ss = qq( $offset/*$sym*/,);
	$offset += length($sym) + 1;
    } else {
	$ss = qq( 0,);
    }
    if (length($s) + length($ss) > 78) {
      print qq( $s\n);
      $s = "";
    }
    $s .= $ss;
}
print qq( $s\n);
print "};\n";
exit;
#endif

static const char ps_standard_encoding_symbol[] = {
  "\0" "space\0" "exclam\0" "quotedbl\0" "numbersign\0" "dollar\0" "percent\0"
  "ampersand\0" "quoteright\0" "parenleft\0" "parenright\0" "asterisk\0"
  "plus\0" "comma\0" "hyphen\0" "period\0" "slash\0" "zero\0" "one\0" "two\0"
  "three\0" "four\0" "five\0" "six\0" "seven\0" "eight\0" "nine\0" "colon\0"
  "semicolon\0" "less\0" "equal\0" "greater\0" "question\0" "at\0" "A\0" "B\0"
  "C\0" "D\0" "E\0" "F\0" "G\0" "H\0" "I\0" "J\0" "K\0" "L\0" "M\0" "N\0" "O\0"
  "P\0" "Q\0" "R\0" "S\0" "T\0" "U\0" "V\0" "W\0" "X\0" "Y\0" "Z\0"
  "bracketleft\0" "backslash\0" "bracketright\0" "asciicircum\0" "underscore\0"
  "quoteleft\0" "a\0" "b\0" "c\0" "d\0" "e\0" "f\0" "g\0" "h\0" "i\0" "j\0"
  "k\0" "l\0" "m\0" "n\0" "o\0" "p\0" "q\0" "r\0" "s\0" "t\0" "u\0" "v\0" "w\0"
  "x\0" "y\0" "z\0" "braceleft\0" "bar\0" "braceright\0" "asciitilde\0"
  "exclamdown\0" "cent\0" "sterling\0" "fraction\0" "yen\0" "florin\0"
  "section\0" "currency\0" "quotesingle\0" "quotedblleft\0" "guillemotleft\0"
  "guilsinglleft\0" "guilsinglright\0" "fi\0" "fl\0" "endash\0" "dagger\0"
  "daggerdbl\0" "periodcentered\0" "paragraph\0" "bullet\0" "quotesinglbase\0"
  "quotedblbase\0" "quotedblright\0" "guillemotright\0" "ellipsis\0"
  "perthousand\0" "questiondown\0" "grave\0" "acute\0" "circumflex\0" "tilde\0"
  "macron\0" "breve\0" "dotaccent\0" "dieresis\0" "ring\0" "cedilla\0"
  "hungarumlaut\0" "ogonek\0" "caron\0" "emdash\0" "AE\0" "ordfeminine\0"
  "Lslash\0" "Oslash\0" "OE\0" "ordmasculine\0" "ae\0" "dotlessi\0" "lslash\0"
  "oslash\0" "oe\0" "germandbls\0"
};

static const int16_t ps_standard_encoding_offset[256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 1/*space*/, 7/*exclam*/, 14/*quotedbl*/, 23/*numbersign*/,
  34/*dollar*/, 41/*percent*/, 49/*ampersand*/, 59/*quoteright*/,
  70/*parenleft*/, 80/*parenright*/, 91/*asterisk*/, 100/*plus*/, 105/*comma*/,
  111/*hyphen*/, 118/*period*/, 125/*slash*/, 131/*zero*/, 136/*one*/,
  140/*two*/, 144/*three*/, 150/*four*/, 155/*five*/, 160/*six*/, 164/*seven*/,
  170/*eight*/, 176/*nine*/, 181/*colon*/, 187/*semicolon*/, 197/*less*/,
  202/*equal*/, 208/*greater*/, 216/*question*/, 225/*at*/, 228/*A*/, 230/*B*/,
  232/*C*/, 234/*D*/, 236/*E*/, 238/*F*/, 240/*G*/, 242/*H*/, 244/*I*/,
  246/*J*/, 248/*K*/, 250/*L*/, 252/*M*/, 254/*N*/, 256/*O*/, 258/*P*/,
  260/*Q*/, 262/*R*/, 264/*S*/, 266/*T*/, 268/*U*/, 270/*V*/, 272/*W*/,
  274/*X*/, 276/*Y*/, 278/*Z*/, 280/*bracketleft*/, 292/*backslash*/,
  302/*bracketright*/, 315/*asciicircum*/, 327/*underscore*/, 338/*quoteleft*/,
  348/*a*/, 350/*b*/, 352/*c*/, 354/*d*/, 356/*e*/, 358/*f*/, 360/*g*/,
  362/*h*/, 364/*i*/, 366/*j*/, 368/*k*/, 370/*l*/, 372/*m*/, 374/*n*/,
  376/*o*/, 378/*p*/, 380/*q*/, 382/*r*/, 384/*s*/, 386/*t*/, 388/*u*/,
  390/*v*/, 392/*w*/, 394/*x*/, 396/*y*/, 398/*z*/, 400/*braceleft*/,
  410/*bar*/, 414/*braceright*/, 425/*asciitilde*/, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  436/*exclamdown*/, 447/*cent*/, 452/*sterling*/, 461/*fraction*/, 470/*yen*/,
  474/*florin*/, 481/*section*/, 489/*currency*/, 498/*quotesingle*/,
  510/*quotedblleft*/, 523/*guillemotleft*/, 537/*guilsinglleft*/,
  551/*guilsinglright*/, 566/*fi*/, 569/*fl*/, 0, 572/*endash*/, 579/*dagger*/,
  586/*daggerdbl*/, 596/*periodcentered*/, 0, 611/*paragraph*/, 621/*bullet*/,
  628/*quotesinglbase*/, 643/*quotedblbase*/, 656/*quotedblright*/,
  670/*guillemotright*/, 685/*ellipsis*/, 694/*perthousand*/, 0,
  706/*questiondown*/, 0, 719/*grave*/, 725/*acute*/, 731/*circumflex*/,
  742/*tilde*/, 748/*macron*/, 755/*breve*/, 761/*dotaccent*/, 771/*dieresis*/,
  0, 780/*ring*/, 785/*cedilla*/, 0, 793/*hungarumlaut*/, 806/*ogonek*/,
  813/*caron*/, 819/*emdash*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  826/*AE*/, 0, 829/*ordfeminine*/, 0, 0, 0, 0, 841/*Lslash*/, 848/*Oslash*/,
  855/*OE*/, 858/*ordmasculine*/, 0, 0, 0, 0, 0, 871/*ae*/, 0, 0, 0,
  874/*dotlessi*/, 0, 0, 883/*lslash*/, 890/*oslash*/, 897/*oe*/,
  900/*germandbls*/, 0, 0, 0, 0,
};

const char *
_cairo_ps_standard_encoding_to_glyphname (int glyph)
{
    if (ps_standard_encoding_offset[glyph])
	return ps_standard_encoding_symbol + ps_standard_encoding_offset[glyph];
    else
	return NULL;
}


/* TODO: Merge these names into ps_standard_encoding_symbol (with a
 * separate winansi_encoding_offset table) as most of the names are
 * the same. */
static const char *winansi_encoding[256] = {
	/*   0 */
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	/*  16 */
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,
	/*  32 */
	"space",	"exclam",	"quotedbl",	"numbersign",
	"dollar",	"percent",	"ampersand",	"quotesingle",
	"parenleft",	"parenright",	"asterisk",	"plus",
	"comma",	"hyphen",	"period",	"slash",
	/*  48 */
	"zero",		"one",		"two",		"three",
	"four",		"five",		"six",		"seven",
	"eight",	"nine",		"colon",	"semicolon",
	"less",		"equal",	"greater",	"question",
	/*  64 */
	"at",		"A",		"B",		"C",
	"D",		"E",		"F",		"G",
	"H",		"I",		"J",		"K",
	"L",		"M",		"N",		"O",
	/*  80 */
	"P",		"Q",		"R",		"S",
	"T",		"U",		"V",		"W",
	"X",		"Y",		"Z",		"bracketleft",
	"backslash",	"bracketright",	"asciicircum",	"underscore",
	/*  96 */
	"grave",	"a",		"b",		"c",
	"d",		"e",		"f",		"g",
	"h",		"i",		"j",		"k",
	"l",		"m",		"n",		"o",
	/* 112 */
	"p",		"q",		"r",		"s",
	"t",		"u",		"v",		"w",
	"x",		"y",		"z",		"braceleft",
	"bar",		"braceright",	"asciitilde",	NULL,
	/* 128 */
	"Euro",		NULL,		"quotesinglbase","florin",
	"quotedblbase", "ellipsis",	"dagger",	"daggerdbl",
	"circumflex",	"perthousand",	"Scaron",	"guilsinglleft",
	"OE",		NULL,		"Zcaron",	NULL,
	/* 144 */
	NULL,		"quoteleft",	"quoteright",	"quotedblleft",
	"quotedblright","bullet",	"endash",	"emdash",
	"tilde",	"trademark",	"scaron",	"guilsinglright",
	"oe",		NULL,		"zcaron",	"Ydieresis",
	/* 160 */
	NULL,		"exclamdown",	"cent",		"sterling",
	"currency",	"yen",		"brokenbar",	"section",
	"dieresis",	"copyright",	"ordfeminine",	"guillemotleft",
	/* 173 is also "hyphen" but we leave this NULL to avoid duplicate names */
	"logicalnot",	NULL,		"registered",	"macron",
	/* 176 */
	"degree",	"plusminus",	"twosuperior",	"threesuperior",
	"acute",	"mu",		"paragraph",	"periodcentered",
	"cedilla",	"onesuperior",	"ordmasculine",	"guillemotright",
	"onequarter",	"onehalf",	"threequarters","questiondown",
	/* 192 */
	"Agrave",	"Aacute",	"Acircumflex",	"Atilde",
	"Adieresis",	"Aring",	"AE",		"Ccedilla",
	"Egrave",	"Eacute",	"Ecircumflex",	"Edieresis",
	"Igrave",	"Iacute",	"Icircumflex",	"Idieresis",
	/* 208 */
	"Eth",		"Ntilde",	"Ograve",	"Oacute",
	"Ocircumflex",	"Otilde",	"Odieresis",	"multiply",
	"Oslash",	"Ugrave",	"Uacute",	"Ucircumflex",
	"Udieresis",	"Yacute",	"Thorn",	"germandbls",
	/* 224 */
	"agrave",	"aacute",	"acircumflex",	"atilde",
	"adieresis",	"aring",	"ae",		"ccedilla",
	"egrave",	"eacute",	"ecircumflex",	"edieresis",
	"igrave",	"iacute",	"icircumflex",	"idieresis",
	/* 240 */
	"eth",		"ntilde",	"ograve",	"oacute",
	"ocircumflex",	"otilde",	"odieresis",	"divide",
	"oslash",	"ugrave",	"uacute",	"ucircumflex",
	"udieresis",	"yacute",	"thorn",	"ydieresis"
};

const char *
_cairo_winansi_to_glyphname (int glyph)
{
    return winansi_encoding[glyph];
}

#endif /* CAIRO_HAS_FONT_SUBSET */
