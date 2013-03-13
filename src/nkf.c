/*
 *  NKF - Ruby extension for Network Kanji Filter
 *
 *  original nkf2.x is maintained at http://sourceforge.jp/projects/nkf/
 *
 *  $Id: nkf.c 36966 2012-09-14 16:07:49Z naruse $
 *
 */

#define RUBY_NKF_REVISION "$Revision: 36966 $"
#define RUBY_NKF_VERSION NKF_VERSION " (" NKF_RELEASE_DATE ")"

#include <mruby.h>
#include <mruby/string.h>

/* Encoding Constants */
#define _AUTO           0
#define _JIS            1
#define _EUC            2
#define _SJIS           3
#define _BINARY         4
#define _NOCONV         4
#define _ASCII          5
/* 0b011x is reserved for UTF-8 Family */
#define _UTF8           6
/* 0b10xx is reserved for UTF-16 Family */
#define _UTF16          8
/* 0b11xx is reserved for UTF-32 Family */
#define _UTF32          12
#define _OTHER          16
#define _UNKNOWN        _AUTO

/* Replace nkf's getchar/putchar for variable modification */
/* we never use getc, ungetc */

#undef getc
#undef ungetc
#define getc(f)         (input_ctr>=i_len?-1:input[input_ctr++])
#define ungetc(c,f)     input_ctr--

#define INCSIZE         32
#undef putchar
#undef TRUE
#undef FALSE
#define putchar(c)      mrb_nkf_putchar(c)

/* Input/Output pointers */

static unsigned char *output;
static unsigned char *input;
static int input_ctr;
static int i_len;
static int output_ctr;
static int o_len;
static int incsize;

static mrb_value result;
mrb_state *mrb_state_nkf;

static int
mrb_nkf_putchar(unsigned int c)
{
  if (output_ctr >= o_len) {
    o_len += incsize;
    mrb_str_resize(mrb_state_nkf, result, o_len);
    incsize *= 2;
    output = (unsigned char *)RSTRING_PTR(result);
  }
  output[output_ctr++] = c;

  return c;
}

/* Include kanji filter main part */
/* getchar and putchar will be replaced during inclusion */

#define PERL_XS 1
#include "nkf-utf8/config.h"
#include "nkf-utf8/utf8tbl.c"
#include "nkf-utf8/nkf.c"

int nkf_split_options(const char *arg)
{
    int count = 0;
    unsigned char option[256];
    int i = 0, j = 0;
    int is_escaped = FALSE;
    int is_single_quoted = FALSE;
    int is_double_quoted = FALSE;
    for(i = 0; arg[i]; i++){
	if(j == 255){
	    return -1;
	}else if(is_single_quoted){
	    if(arg[i] == '\''){
		is_single_quoted = FALSE;
	    }else{
		option[j++] = arg[i];
	    }
	}else if(is_escaped){
	    is_escaped = FALSE;
	    option[j++] = arg[i];
	}else if(arg[i] == '\\'){
	    is_escaped = TRUE;
	}else if(is_double_quoted){
	    if(arg[i] == '"'){
		is_double_quoted = FALSE;
	    }else{
		option[j++] = arg[i];
	    }
	}else if(arg[i] == '\''){
	    is_single_quoted = TRUE;
	}else if(arg[i] == '"'){
	    is_double_quoted = TRUE;
	}else if(arg[i] == ' '){
	    option[j] = '\0';
	    options(option);
	    j = 0;
	}else{
	    option[j++] = arg[i];
	}
    }
    if(j){
	option[j] = '\0';
	options(option);
    }
    return count;
}

/*
 *  call-seq:
 *     NKF.nkf(opt, str)   => string
 *
 *  Convert _str_ and return converted result.
 *  Conversion details are specified by _opt_ as String.
 *
 *     require 'nkf'
 *     output = NKF.nkf("-s", input)
 */

static mrb_value
mrb_nkf_convert(mrb_state *mrb, mrb_value self)
{
     mrb_value tmp;
     mrb_value opt, src;
     mrb_get_args(mrb, "SS", &opt, &src);
    reinit();

    nkf_split_options(RSTRING_PTR(opt));
    if (!output_encoding) mrb_raise(mrb, E_ARGUMENT_ERROR, "no output encoding given");

    switch (nkf_enc_to_index(output_encoding)) {
    case UTF_8_BOM:    output_encoding = nkf_enc_from_index(UTF_8); break;
    case UTF_16BE_BOM: output_encoding = nkf_enc_from_index(UTF_16BE); break;
    case UTF_16LE_BOM: output_encoding = nkf_enc_from_index(UTF_16LE); break;
    case UTF_32BE_BOM: output_encoding = nkf_enc_from_index(UTF_32BE); break;
    case UTF_32LE_BOM: output_encoding = nkf_enc_from_index(UTF_32LE); break;
    }
    output_bom_f = FALSE;

    incsize = INCSIZE;

    input_ctr = 0;

    input = (unsigned char *)RSTRING_PTR(src);
    i_len = RSTRING_LEN(src);
    tmp = mrb_str_new(mrb, 0, i_len*3 + 10);

    output_ctr = 0;
    output     = (unsigned char *)RSTRING_PTR(tmp);
    o_len      = RSTRING_LEN(tmp);
    *output    = '\0';

    /* use _result_ begin*/
    result = tmp;
    kanji_convert(NULL);
    result = mrb_nil_value();
    /* use _result_ end */

    mrb_str_resize(mrb, tmp, output_ctr);

    return tmp;
}


/*
 *  call-seq:
 *     NKF.guess(str)  => encoding
 *
 *  Returns guessed encoding of _str_ by nkf routine.
 *
 */

static mrb_value
mrb_nkf_guess(mrb_state *mrb, mrb_value self)
{
     mrb_value src;
     int code = _BINARY;
     mrb_get_args(mrb, "S", &src);

     reinit();

     input_ctr = 0;
//    StringValue(src);
     input = (unsigned char *)RSTRING_PTR(src);
     i_len = RSTRING_LEN(src);
     
     guess_f = TRUE;
     kanji_convert( NULL );
     guess_f = FALSE;

     if (strcmp(input_codename, "") == 0) {
	  code = _ASCII;
     } else if (strcmp(input_codename, "ISO-2022-JP") == 0) {
	  code = _JIS;
     } else if (strcmp(input_codename, "EUC-JP") == 0) {
	  code = _EUC;
     } else if (strcmp(input_codename, "Shift_JIS") == 0) {
	  code = _SJIS;
     } else if (strcmp(input_codename, "UTF-8") == 0) {
	  code = _UTF8;
     } else if (strcmp(input_codename, "UTF-16") == 0) {
	  code = _UTF16;
     } else if (strlen(input_codename) > 0) {
	  code = _UNKNOWN;
     }
    return mrb_fixnum_value(code);
}


/*
 *  NKF - Ruby extension for Network Kanji Filter
 *
 *  == Description
 *
 *  This is a Ruby Extension version of nkf (Network Kanji Filter).
 *  It converts the first argument and returns converted result. Conversion
 *  details are specified by flags as the first argument.
 *
 *  *Nkf* is a yet another kanji code converter among networks, hosts and terminals.
 *  It converts input kanji code to designated kanji code
 *  such as ISO-2022-JP, Shift_JIS, EUC-JP, UTF-8 or UTF-16.
 *
 *  One of the most unique faculty of *nkf* is the guess of the input kanji encodings.
 *  It currently recognizes ISO-2022-JP, Shift_JIS, EUC-JP, UTF-8 and UTF-16.
 *  So users needn't set the input kanji code explicitly.
 *
 *  By default, X0201 kana is converted into X0208 kana.
 *  For X0201 kana, SO/SI, SSO and ESC-(-I methods are supported.
 *  For automatic code detection, nkf assumes no X0201 kana in Shift_JIS.
 *  To accept X0201 in Shift_JIS, use <b>-X</b>, <b>-x</b> or <b>-S</b>.
 *
 *  == Flags
 *
 *  === -b -u
 *
 *  Output is buffered (DEFAULT), Output is unbuffered.
 *
 *  === -j -s -e -w -w16 -w32
 *
 *  Output code is ISO-2022-JP (7bit JIS), Shift_JIS, EUC-JP,
 *  UTF-8N, UTF-16BE, UTF-32BE.
 *  Without this option and compile option, ISO-2022-JP is assumed.
 *
 *  === -J -S -E -W -W16 -W32
 *
 *  Input assumption is JIS 7 bit, Shift_JIS, EUC-JP,
 *  UTF-8, UTF-16, UTF-32.
 *
 *  ==== -J
 *
 *  Assume  JIS input. It also accepts EUC-JP.
 *  This is the default. This flag does not exclude Shift_JIS.
 *
 *  ==== -S
 *
 *  Assume Shift_JIS and X0201 kana input. It also accepts JIS.
 *  EUC-JP is recognized as X0201 kana. Without <b>-x</b> flag,
 *  X0201 kana (halfwidth kana) is converted into X0208.
 *
 *  ==== -E
 *
 *  Assume EUC-JP input. It also accepts JIS.
 *  Same as -J.
 *
 *  === -t
 *
 *  No conversion.
 *
 *  === -i_
 *
 *  Output sequence to designate JIS-kanji. (DEFAULT B)
 *
 *  === -o_
 *
 *  Output sequence to designate ASCII. (DEFAULT B)
 *
 *  === -r
 *
 *  {de/en}crypt ROT13/47
 *
 *  === -h[123] --hiragana --katakana --katakana-hiragana
 *
 *  [-h1 --hiragana] Katakana to Hiragana conversion.
 *
 *  [-h2 --katakana] Hiragana to Katakana conversion.
 *
 *  [-h3 --katakana-hiragana] Katakana to Hiragana and Hiragana to Katakana conversion.
 *
 *  === -T
 *
 *  Text mode output (MS-DOS)
 *
 *  === -l
 *
 *  ISO8859-1 (Latin-1) support
 *
 *  === -f[<code>m</code> [- <code>n</code>]]
 *
 *  Folding on <code>m</code> length with <code>n</code> margin in a line.
 *  Without this option, fold length is 60 and fold margin is 10.
 *
 *  === -F
 *
 *  New line preserving line folding.
 *
 *  === -Z[0-3]
 *
 *  Convert X0208 alphabet (Fullwidth Alphabets) to ASCII.
 *
 *  [-Z -Z0] Convert X0208 alphabet to ASCII.
 *
 *  [-Z1] Converts X0208 kankaku to single ASCII space.
 *
 *  [-Z2] Converts X0208 kankaku to double ASCII spaces.
 *
 *  [-Z3] Replacing Fullwidth >, <, ", & into '&gt;', '&lt;', '&quot;', '&amp;' as in HTML.
 *
 *  === -X -x
 *
 *  Assume X0201 kana in MS-Kanji.
 *  With <b>-X</b> or without this option, X0201 is converted into X0208 Kana.
 *  With <b>-x</b>, try to preserve X0208 kana and do not convert X0201 kana to X0208.
 *  In JIS output, ESC-(-I is used. In EUC output, SSO is used.
 *
 *  === -B[0-2]
 *
 *  Assume broken JIS-Kanji input, which lost ESC.
 *  Useful when your site is using old B-News Nihongo patch.
 *
 *  [-B1] allows any char after ESC-( or ESC-$.
 *
 *  [-B2] forces ASCII after NL.
 *
 *  === -I
 *
 *  Replacing non iso-2022-jp char into a geta character
 *  (substitute character in Japanese).
 *
 *  === -d -c
 *
 *  Delete \r in line feed, Add \r in line feed.
 *
 *  === -m[BQN0]
 *
 *  MIME ISO-2022-JP/ISO8859-1 decode. (DEFAULT)
 *  To see ISO8859-1 (Latin-1) -l is necessary.
 *
 *  [-mB] Decode MIME base64 encoded stream. Remove header or other part before
 *  conversion.
 *
 *  [-mQ] Decode MIME quoted stream. '_' in quoted stream is converted to space.
 *
 *  [-mN] Non-strict decoding.
 *  It allows line break in the middle of the base64 encoding.
 *
 *  [-m0] No MIME decode.
 *
 *  === -M
 *
 *  MIME encode. Header style. All ASCII code and control characters are intact.
 *  Kanji conversion is performed before encoding, so this cannot be used as a picture encoder.
 *
 *  [-MB] MIME encode Base64 stream.
 *
 *  [-MQ] Perfome quoted encoding.
 *
 *  === -l
 *
 *  Input and output code is ISO8859-1 (Latin-1) and ISO-2022-JP.
 *  <b>-s</b>, <b>-e</b> and <b>-x</b> are not compatible with this option.
 *
 *  === -L[uwm]
 *
 *  new line mode
 *  Without this option, nkf doesn't convert line breaks.
 *
 *  [-Lu] unix (LF)
 *
 *  [-Lw] windows (CRLF)
 *
 *  [-Lm] mac (CR)
 *
 *  === --fj --unix --mac --msdos --windows
 *
 *  convert for these system
 *
 *  === --jis --euc --sjis --mime --base64
 *
 *  convert for named code
 *
 *  === --jis-input --euc-input --sjis-input --mime-input --base64-input
 *
 *  assume input system
 *
 *  === --ic=<code>input codeset</code> --oc=<code>output codeset</code>
 *
 *  Set the input or output codeset.
 *  NKF supports following codesets and those codeset name are case insensitive.
 *
 *  [ISO-2022-JP] a.k.a. RFC1468, 7bit JIS, JUNET
 *
 *  [EUC-JP (eucJP-nkf)] a.k.a. AT&T JIS, Japanese EUC, UJIS
 *
 *  [eucJP-ascii] a.k.a. x-eucjp-open-19970715-ascii
 *
 *  [eucJP-ms] a.k.a. x-eucjp-open-19970715-ms
 *
 *  [CP51932] Microsoft Version of EUC-JP.
 *
 *  [Shift_JIS] SJIS, MS-Kanji
 *
 *  [Windows-31J] a.k.a. CP932
 *
 *  [UTF-8] same as UTF-8N
 *
 *  [UTF-8N] UTF-8 without BOM
 *
 *  [UTF-8-BOM] UTF-8 with BOM
 *
 *  [UTF-16] same as UTF-16BE
 *
 *  [UTF-16BE] UTF-16 Big Endian without BOM
 *
 *  [UTF-16BE-BOM] UTF-16 Big Endian with BOM
 *
 *  [UTF-16LE] UTF-16 Little Endian without BOM
 *
 *  [UTF-16LE-BOM] UTF-16 Little Endian with BOM
 *
 *  [UTF-32] same as UTF-32BE
 *
 *  [UTF-32BE] UTF-32 Big Endian without BOM
 *
 *  [UTF-32BE-BOM] UTF-32 Big Endian with BOM
 *
 *  [UTF-32LE] UTF-32 Little Endian without BOM
 *
 *  [UTF-32LE-BOM] UTF-32 Little Endian with BOM
 *
 *  [UTF8-MAC] NKDed UTF-8, a.k.a. UTF8-NFD (input only)
 *
 *  === --fb-{skip, html, xml, perl, java, subchar}
 *
 *  Specify the way that nkf handles unassigned characters.
 *  Without this option, --fb-skip is assumed.
 *
 *  === --prefix= <code>escape character</code> <code>target character</code> ..
 *
 *  When nkf converts to Shift_JIS,
 *  nkf adds a specified escape character to specified 2nd byte of Shift_JIS characters.
 *  1st byte of argument is the escape character and following bytes are target characters.
 *
 *  === --no-cp932ext
 *
 *  Handle the characters extended in CP932 as unassigned characters.
 *
 *  == --no-best-fit-chars
 *
 *  When Unicode to Encoded byte conversion,
 *  don't convert characters which is not round trip safe.
 *  When Unicode to Unicode conversion,
 *  with this and -x option, nkf can be used as UTF converter.
 *  (In other words, without this and -x option, nkf doesn't save some characters)
 *
 *  When nkf convert string which related to path, you should use this opion.
 *
 *  === --cap-input
 *
 *  Decode hex encoded characters.
 *
 *  === --url-input
 *
 *  Unescape percent escaped characters.
 *
 *  === --
 *
 *  Ignore rest of -option.
 */

void
mrb_mruby_nkf_gem_init(mrb_state *mrb)
{
     struct RClass *mrb_nkf;
     mrb_state_nkf = mrb;

     mrb_nkf = mrb_define_module(mrb, "NKF");

     mrb_define_module_function(mrb, mrb_nkf, "nkf", mrb_nkf_convert, 
				ARGS_REQ(2));
     mrb_define_module_function(mrb, mrb_nkf, "guess", mrb_nkf_guess,
				ARGS_REQ(1));

     mrb_define_const(mrb, mrb_nkf, "AUTO",       mrb_fixnum_value(_AUTO));
     mrb_define_const(mrb, mrb_nkf, "NOCONV",     mrb_fixnum_value(_NOCONV));

     mrb_define_const(mrb, mrb_nkf, "UNKNOWN",    mrb_fixnum_value(_UNKNOWN));
     mrb_define_const(mrb, mrb_nkf, "BINARY",     mrb_fixnum_value(_BINARY));
     mrb_define_const(mrb, mrb_nkf, "ASCII",      mrb_fixnum_value(_ASCII));
     mrb_define_const(mrb, mrb_nkf, "JIS",        mrb_fixnum_value(_JIS));
     mrb_define_const(mrb, mrb_nkf, "EUC",        mrb_fixnum_value(_EUC));
     mrb_define_const(mrb, mrb_nkf, "SJIS",       mrb_fixnum_value(_SJIS));
     mrb_define_const(mrb, mrb_nkf, "UTF8",       mrb_fixnum_value(_UTF8));
     mrb_define_const(mrb, mrb_nkf, "UTF16",      mrb_fixnum_value(_UTF16));
     mrb_define_const(mrb, mrb_nkf, "UTF32",      mrb_fixnum_value(_UTF32));

    /* Full version string of nkf */
     mrb_define_const(mrb, mrb_nkf, "VERSION", mrb_str_new_cstr(mrb, RUBY_NKF_VERSION));
    /* Version of nkf */
     mrb_define_const(mrb, mrb_nkf, "NKF_VERSION", mrb_str_new_cstr(mrb, NKF_VERSION));
    /* Release date of nkf */
     mrb_define_const(mrb, mrb_nkf, "NKF_RELEASE_DATE", mrb_str_new_cstr(mrb, NKF_RELEASE_DATE));

}

void
mrb_mruby_nkf_gem_final(mrb_state *mrb)
{
}
