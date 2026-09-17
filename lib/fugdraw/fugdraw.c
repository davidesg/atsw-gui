/***************************************************************************
 *   fugdraw.c -- vector graphics for FUG: EPS and PDF without external     *
 *   programs. Copyright (C) 2026 by David E. Guerrero & Arthur B. Treadway *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "fugdraw.h"
#include "fd_metrics.h"

/*****************************************************************************/
/* Content stream                                                            */
/*****************************************************************************/

static void put( FDFig *f, const char *fmt, ... )
{
   va_list ap;
   int     n;

   for ( ;; )
      {
      va_start( ap, fmt );
      n = vsnprintf( f->buf + f->len, f->cap - f->len, fmt, ap );
      va_end( ap );
      if ( n >= 0 && (size_t)n < f->cap - f->len ) break;
      f->cap = 2 * f->cap + (size_t)(n > 0 ? n : 256);
      f->buf = (char *)realloc( f->buf, f->cap );
      if ( f->buf == NULL ) { fprintf( stderr, "fugdraw: out of memory\n" ); exit( 1 ); }
      }
   f->len += (size_t)n;
}

FDFig *fd_fig_new( double w, double h )
{
   FDFig *f = (FDFig *)calloc( 1, sizeof( FDFig ) );

   if ( f == NULL ) { fprintf( stderr, "fugdraw: out of memory\n" ); exit( 1 ); }
   f->w = w;
   f->h = h;
   f->cap = 16384;
   f->buf = (char *)malloc( f->cap );
   if ( f->buf == NULL ) { fprintf( stderr, "fugdraw: out of memory\n" ); exit( 1 ); }
   f->buf[0] = '\0';
   f->lw = 1.0;                        /* PDF and PostScript defaults */
   f->dash_on = 0.0;
   f->gray = 0.0;
   put( f, "1 j\n" );                  /* round joins for the polylines */
   return( f );
}

void fd_fig_free( FDFig *f )
{
   if ( f == NULL ) return;
   free( f->buf );
   free( f );
}

void fd_linewidth( FDFig *f, double w )
{
   if ( fabs( w - f->lw ) < 1e-6 ) return;
   f->lw = w;
   put( f, "%.3f w\n", w );
}

void fd_dash( FDFig *f, double on, double off )
{
   if ( on <= 0.0 ) on = off = 0.0;
   if ( fabs( on - f->dash_on ) < 1e-6 && fabs( off - f->dash_off ) < 1e-6 ) return;
   f->dash_on = on;
   f->dash_off = off;
   if ( on <= 0.0 )
      put( f, "[] 0 d\n" );
   else
      put( f, "[%.2f %.2f] 0 d\n", on, off );
}

void fd_gray( FDFig *f, double g )
{
   if ( fabs( g - f->gray ) < 1e-6 ) return;
   f->gray = g;
   put( f, "%.3f g %.3f G\n", g, g );
}

void fd_line( FDFig *f, double x1, double y1, double x2, double y2 )
{
   put( f, "%.2f %.2f m %.2f %.2f l S\n", x1, y1, x2, y2 );
}

void fd_polyline( FDFig *f, const double *x, const double *y, int n )
{
   int i;

   if ( n < 2 ) return;
   put( f, "%.2f %.2f m\n", x[0], y[0] );
   for ( i = 1; i < n; i++ ) put( f, "%.2f %.2f l\n", x[i], y[i] );
   put( f, "S\n" );
}

void fd_disc( FDFig *f, double x, double y, double r )
{
   const double k = 0.5523 * r;       /* four Bezier arcs */

   put( f, "%.2f %.2f m ", x + r, y );
   put( f, "%.2f %.2f %.2f %.2f %.2f %.2f c ", x + r, y + k, x + k, y + r, x, y + r );
   put( f, "%.2f %.2f %.2f %.2f %.2f %.2f c ", x - k, y + r, x - r, y + k, x - r, y );
   put( f, "%.2f %.2f %.2f %.2f %.2f %.2f c ", x - r, y - k, x - k, y - r, x, y - r );
   put( f, "%.2f %.2f %.2f %.2f %.2f %.2f c h f\n", x + k, y - r, x + r, y - k, x + r, y );
}

/* The graphics state is saved with q and restored with Q: keep the cache. */
static double saved_lw, saved_on, saved_off, saved_gray;

void fd_clip_begin( FDFig *f, double x, double y, double w, double h )
{
   saved_lw = f->lw; saved_on = f->dash_on; saved_off = f->dash_off; saved_gray = f->gray;
   put( f, "q %.2f %.2f %.2f %.2f re W n\n", x, y, w, h );
}

void fd_clip_end( FDFig *f )
{
   put( f, "Q\n" );
   f->lw = saved_lw; f->dash_on = saved_on; f->dash_off = saved_off; f->gray = saved_gray;
}

/*****************************************************************************/
/* Text                                                                      */
/*****************************************************************************/

double fd_text_width( int font, double size, const char *s )
{
   double w = 0.0;

   for ( ; *s != '\0'; s++ ) w += fd_widths[font][(unsigned char)*s];
   return( w * size / 1000.0 );
}

double fd_font_xheight( int font, double size )
{
   return( fd_xheight[font] * size / 1000.0 );
}

static void put_string( FDFig *f, const char *s )
{
   put( f, "(" );
   for ( ; *s != '\0'; s++ )
       {
       unsigned char c = (unsigned char)*s;
       if ( c == '(' || c == ')' || c == '\\' ) put( f, "\\%c", c );
       else if ( c < 32 || c > 126 ) put( f, "\\%03o", c );
       else put( f, "%c", c );
       }
   put( f, ")" );
}

static void show( FDFig *f, double x, double y, int font, double size, const char *s )
{
   put( f, "BT /F%d %.2f Tf %.2f %.2f Td ", font + 1, size, x, y );
   put_string( f, s );
   put( f, " Tj ET\n" );
}

void fd_text( FDFig *f, double x, double y, int font, double size, int align, const char *s )
{
   double w = fd_text_width( font, size, s );

   if ( align == FD_CENTER ) x -= w / 2.0;
   else if ( align == FD_RIGHT ) x -= w;
   show( f, x, y, font, size, s );
}

void fd_text_up( FDFig *f, double x, double y, int font, double size, int align, const char *s )
{
   double w = fd_text_width( font, size, s ), dx = 0.0;

   if ( align == FD_CENTER ) dx = -w / 2.0;
   else if ( align == FD_RIGHT ) dx = -w;
   put( f, "q 0 1 -1 0 %.2f %.2f cm\n", x, y );
   show( f, dx, 0.0, font, size, s );
   put( f, "Q\n" );
}

double fd_runs_width( const FDRun *r, int n )
{
   double w = 0.0;
   int    i;

   for ( i = 0; i < n; i++ ) w += fd_text_width( r[i].font, r[i].size, r[i].text );
   return( w );
}

void fd_runs( FDFig *f, double x, double y, int align, const FDRun *r, int n )
{
   double w = fd_runs_width( r, n ), rw, top, lw = f->lw;
   int    i;

   if ( align == FD_CENTER ) x -= w / 2.0;
   else if ( align == FD_RIGHT ) x -= w;
   for ( i = 0; i < n; i++ )
       {
       rw = fd_text_width( r[i].font, r[i].size, r[i].text );
       show( f, x, y + r[i].rise, r[i].font, r[i].size, r[i].text );
       top = y + r[i].rise + fd_font_xheight( r[i].font, r[i].size ) + 0.12 * r[i].size;
       if ( r[i].accent == FD_ACC_BAR )
          {
          fd_linewidth( f, 0.055 * r[i].size );
          fd_line( f, x + 0.05 * rw, top, x + 0.95 * rw, top );
          }
       else if ( r[i].accent == FD_ACC_HAT )
          {
          double cx = x + rw / 2.0, hw = 0.20 * r[i].size, hh = 0.24 * r[i].size;
          fd_linewidth( f, 0.06 * r[i].size );
          put( f, "%.2f %.2f m %.2f %.2f l %.2f %.2f l S\n",
               cx - hw, top, cx, top + hh, cx + hw, top );
          }
       x += rw;
       }
   fd_linewidth( f, lw );
}

/*****************************************************************************/
/* EPS                                                                       */
/*****************************************************************************/

/* The PDF operators used in the content streams, defined in PostScript */
static const char *eps_prolog =
   "/FDdict 64 dict def FDdict begin\n"
   "/m {moveto} bind def /l {lineto} bind def /c {curveto} bind def\n"
   "/h {closepath} bind def /S {stroke} bind def /f {fill} bind def\n"
   "/n {newpath} bind def /W {clip} bind def /q {gsave} bind def /Q {grestore} bind def\n"
   "/re {4 2 roll moveto exch dup 0 rlineto exch 0 exch rlineto neg 0 rlineto closepath} bind def\n"
   "/w {setlinewidth} bind def /d {setdash} bind def /j {setlinejoin} bind def\n"
   "/g {setgray} bind def /G {setgray} bind def /cm {6 array astore concat} bind def\n"
   "/BT {} def /ET {} def /Td {moveto} bind def /Tj {show} bind def\n"
   "/Tf {exch load findfont exch scalefont setfont} bind def\n"
   "/fdenc ISOLatin1Encoding dup length array copy\n"
   "  dup 39 /quotesingle put dup 45 /hyphen put dup 96 /grave put def\n"
   "/fdre {findfont dup length dict begin\n"
   "  {1 index /FID ne {def} {pop pop} ifelse} forall\n"
   "  /Encoding fdenc def currentdict end definefont pop} bind def\n"
   "end\n";

int fd_write_eps( const FDFig *f, const char *filename )
{
   FILE       *fp = fopen( filename, "wb" );
   const char *title = filename, *c;
   int         i;

   if ( fp == NULL ) return( 1 );
   for ( c = filename; *c != '\0'; c++ )          /* title: the name without folder */
       if ( *c == '/' || *c == '\\' ) title = c + 1;
   fprintf( fp, "%%!PS-Adobe-3.0 EPSF-3.0\n" );
   fprintf( fp, "%%%%BoundingBox: 0 0 %d %d\n", (int)ceil( f->w ), (int)ceil( f->h ) );
   fprintf( fp, "%%%%HiResBoundingBox: 0 0 %.2f %.2f\n", f->w, f->h );
   fprintf( fp, "%%%%Creator: FUG (fugdraw)\n" );
   fprintf( fp, "%%%%Title: %s\n", title );
   fprintf( fp, "%%%%LanguageLevel: 2\n" );
   fprintf( fp, "%%%%DocumentNeededResources:" );
   for ( i = 0; i < FD_NFONTS; i++ ) fprintf( fp, " font %s", fd_font_names[i] );
   fprintf( fp, "\n%%%%EndComments\n%%%%BeginProlog\n%s%%%%EndProlog\n", eps_prolog );
   fprintf( fp, "%%%%BeginSetup\nFDdict begin\n" );
   for ( i = 0; i < FD_NFONTS; i++ )
       if ( i == FD_SYMBOL )
          fprintf( fp, "/F%d /Symbol def\n", i + 1 );
       else
          fprintf( fp, "/FD-%s /%s fdre /F%d /FD-%s def\n",
                   fd_font_names[i], fd_font_names[i], i + 1, fd_font_names[i] );
   fprintf( fp, "%%%%EndSetup\n" );
   fwrite( f->buf, 1, f->len, fp );
   fprintf( fp, "end\nshowpage\n%%%%EOF\n" );
   return( fclose( fp ) != 0 );
}

/*****************************************************************************/
/* Flate compression of the PDF streams (zlib format, RFC 1950/1951): LZ77   */
/* with a 32 KB window and the fixed Huffman codes. No external library.    */
/*****************************************************************************/

typedef struct {
   unsigned char *out;
   size_t         len, cap;
   unsigned long  bits;              /* pending bits, LSB first */
   int            nbits;
} BitOut;

static void bo_byte( BitOut *b, unsigned char c )
{
   if ( b->len == b->cap )
      {
      b->cap = 2 * b->cap + 1024;
      b->out = (unsigned char *)realloc( b->out, b->cap );
      if ( b->out == NULL ) { fprintf( stderr, "fugdraw: out of memory\n" ); exit( 1 ); }
      }
   b->out[b->len++] = c;
}

static void bo_bits( BitOut *b, unsigned long v, int n )     /* n <= 16 */
{
   b->bits |= v << b->nbits;
   b->nbits += n;
   while ( b->nbits >= 8 )
      {
      bo_byte( b, (unsigned char)(b->bits & 0xFF) );
      b->bits >>= 8;
      b->nbits -= 8;
      }
}

/* Huffman codes are sent most significant bit first */
static void bo_code( BitOut *b, unsigned code, int n )
{
   unsigned r = 0;
   int      i;

   for ( i = 0; i < n; i++ ) r |= ((code >> i) & 1u) << (n - 1 - i);
   bo_bits( b, r, n );
}

static void lit_code( BitOut *b, int sym )          /* fixed literal/length code */
{
   if ( sym < 144 )      bo_code( b, 0x30 + sym, 8 );
   else if ( sym < 256 ) bo_code( b, 0x190 + (sym - 144), 9 );
   else if ( sym < 280 ) bo_code( b, sym - 256, 7 );
   else                  bo_code( b, 0xC0 + (sym - 280), 8 );
}

static const short len_base[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,
                                    67,83,99,115,131,163,195,227,258 };
static const char  len_extra[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
static const int   dist_base[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,
                                     1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
static const char  dist_extra[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,
                                      12,12,13,13 };

static void put_match( BitOut *b, int len, int dist )
{
   int i = 28, j = 29;

   while ( len_base[i] > len ) i--;
   lit_code( b, 257 + i );
   if ( len_extra[i] ) bo_bits( b, (unsigned long)(len - len_base[i]), len_extra[i] );
   while ( dist_base[j] > dist ) j--;
   bo_code( b, (unsigned)j, 5 );
   if ( dist_extra[j] ) bo_bits( b, (unsigned long)(dist - dist_base[j]), dist_extra[j] );
}

#define HASH_BITS  15
#define WINDOW     32768
#define MAX_CHAIN  64

/* Compress in[0..n-1]; returns a malloc()ed buffer and its length in *outlen */
static unsigned char *deflate_zlib( const unsigned char *in, size_t n, size_t *outlen )
{
   BitOut  b = { NULL, 0, 0, 0, 0 };
   int    *head = (int *)malloc( (1 << HASH_BITS) * sizeof( int ) );
   int    *prev = (int *)malloc( WINDOW * sizeof( int ) );
   size_t  i = 0, k;
   unsigned long a1 = 1, a2 = 0;

   if ( head == NULL || prev == NULL ) { fprintf( stderr, "fugdraw: out of memory\n" ); exit( 1 ); }
   for ( k = 0; k < (1u << HASH_BITS); k++ ) head[k] = -1;

   bo_byte( &b, 0x78 );                       /* zlib header: deflate, 32K window */
   bo_byte( &b, 0x01 );
   bo_bits( &b, 1, 1 );                       /* last block */
   bo_bits( &b, 1, 2 );                       /* fixed Huffman codes */

   while ( i < n )
      {
      int best = 0, bdist = 0;
      if ( i + 2 < n )
         {
         unsigned h = ((unsigned)in[i] << 10 ^ (unsigned)in[i+1] << 5 ^ in[i+2]) & ((1u << HASH_BITS) - 1);
         int      cand = head[h], chain = MAX_CHAIN;
         while ( cand >= 0 && i - (size_t)cand <= WINDOW - 1 && chain-- > 0 )
            {
            int l = 0, maxl = (int)((n - i < 258) ? n - i : 258);
            while ( l < maxl && in[cand + l] == in[i + l] ) l++;
            if ( l > best ) { best = l; bdist = (int)(i - (size_t)cand); if ( l == maxl ) break; }
            cand = prev[cand % WINDOW];
            }
         prev[i % WINDOW] = head[h];
         head[h] = (int)i;
         }
      if ( best >= 3 )
         {
         put_match( &b, best, bdist );
         for ( k = 1; k < (size_t)best; k++ )  /* insert the skipped positions */
             if ( i + k + 2 < n )
                {
                unsigned h = ((unsigned)in[i+k] << 10 ^ (unsigned)in[i+k+1] << 5 ^ in[i+k+2]) &
                             ((1u << HASH_BITS) - 1);
                prev[(i + k) % WINDOW] = head[h];
                head[h] = (int)(i + k);
                }
         i += (size_t)best;
         }
      else
         lit_code( &b, in[i++] );
      }
   lit_code( &b, 256 );                       /* end of block */
   if ( b.nbits > 0 ) bo_bits( &b, 0, 8 - b.nbits );

   for ( k = 0; k < n; k++ )                  /* Adler-32 checksum, big endian */
       {
       a1 = (a1 + in[k]) % 65521;
       a2 = (a2 + a1) % 65521;
       }
   bo_byte( &b, (unsigned char)(a2 >> 8) );
   bo_byte( &b, (unsigned char)(a2 & 0xFF) );
   bo_byte( &b, (unsigned char)(a1 >> 8) );
   bo_byte( &b, (unsigned char)(a1 & 0xFF) );

   free( head );
   free( prev );
   *outlen = b.len;
   return( b.out );
}

/*****************************************************************************/
/* PDF                                                                       */
/*****************************************************************************/

#define PDF_FIRST_FONT 3             /* objects 1: catalog, 2: pages */

struct FDPdf {
   FILE *fp;
   long *offset;                     /* file offset of each object      */
   int   nobj, cap;
   int  *pages;                      /* object numbers of the pages     */
   int   npages, pcap;
   int   error;
};

static int pdf_begin_object( FDPdf *pdf, int num )
{
   if ( num >= pdf->cap )
      {
      pdf->cap = 2 * num + 16;
      pdf->offset = (long *)realloc( pdf->offset, pdf->cap * sizeof( long ) );
      }
   if ( num > pdf->nobj ) pdf->nobj = num;
   pdf->offset[num] = ftell( pdf->fp );
   fprintf( pdf->fp, "%d 0 obj\n", num );
   return( num );
}

FDPdf *fd_pdf_open( const char *filename )
{
   FDPdf *pdf = (FDPdf *)calloc( 1, sizeof( FDPdf ) );
   int    i;

   if ( pdf == NULL ) return( NULL );
   pdf->fp = fopen( filename, "wb" );
   if ( pdf->fp == NULL ) { free( pdf ); return( NULL ); }
   pdf->nobj = 2;                   /* catalog and pages written at the end */
   fprintf( pdf->fp, "%%PDF-1.4\n%%\342\343\317\323\n" );
   for ( i = 0; i < FD_NFONTS; i++ )
       {
       pdf_begin_object( pdf, PDF_FIRST_FONT + i );
       if ( i == FD_SYMBOL )
          fprintf( pdf->fp, "<< /Type /Font /Subtype /Type1 /BaseFont /Symbol >>\nendobj\n" );
       else
          fprintf( pdf->fp, "<< /Type /Font /Subtype /Type1 /BaseFont /%s "
                            "/Encoding /WinAnsiEncoding >>\nendobj\n", fd_font_names[i] );
       }
   return( pdf );
}

void fd_pdf_page( FDPdf *pdf, double pw, double ph, FDFig **figs,
                  const double *x, const double *y, const double *s, int n )
{
   FDFig *content = fd_fig_new( pw, ph );
   int    i, cobj, pobj;

   content->len = 0;                /* no initial state: each figure sets it */
   content->buf[0] = '\0';
   for ( i = 0; i < n; i++ )
       {
       put( content, "q %.4f 0 0 %.4f %.2f %.2f cm\n", s[i], s[i], x[i], y[i] );
       put( content, "%s", figs[i]->buf );
       put( content, "Q\n" );
       }

   {
   size_t         zlen;
   unsigned char *z = deflate_zlib( (const unsigned char *)content->buf, content->len, &zlen );

   cobj = pdf_begin_object( pdf, pdf->nobj + 1 );
   fprintf( pdf->fp, "<< /Length %lu /Filter /FlateDecode >>\nstream\n", (unsigned long)zlen );
   fwrite( z, 1, zlen, pdf->fp );
   fprintf( pdf->fp, "\nendstream\nendobj\n" );
   free( z );
   }
   fd_fig_free( content );

   pobj = pdf_begin_object( pdf, pdf->nobj + 1 );
   fprintf( pdf->fp, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.2f %.2f]\n", pw, ph );
   fprintf( pdf->fp, "   /Resources << /Font <<" );
   for ( i = 0; i < FD_NFONTS; i++ )
       fprintf( pdf->fp, " /F%d %d 0 R", i + 1, PDF_FIRST_FONT + i );
   fprintf( pdf->fp, " >> >>\n   /Contents %d 0 R >>\nendobj\n", cobj );

   if ( pdf->npages >= pdf->pcap )
      {
      pdf->pcap = 2 * pdf->pcap + 8;
      pdf->pages = (int *)realloc( pdf->pages, pdf->pcap * sizeof( int ) );
      }
   pdf->pages[pdf->npages++] = pobj;
}

int fd_pdf_close( FDPdf *pdf )
{
   long xref;
   int  i, status;

   pdf_begin_object( pdf, 2 );
   fprintf( pdf->fp, "<< /Type /Pages /Count %d /Kids [", pdf->npages );
   for ( i = 0; i < pdf->npages; i++ ) fprintf( pdf->fp, " %d 0 R", pdf->pages[i] );
   fprintf( pdf->fp, " ] >>\nendobj\n" );
   pdf_begin_object( pdf, 1 );
   fprintf( pdf->fp, "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n" );

   xref = ftell( pdf->fp );
   fprintf( pdf->fp, "xref\n0 %d\n0000000000 65535 f \n", pdf->nobj + 1 );
   for ( i = 1; i <= pdf->nobj; i++ )
       fprintf( pdf->fp, "%010ld 00000 n \n", pdf->offset[i] );
   fprintf( pdf->fp, "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n",
            pdf->nobj + 1, xref );
   status = (fclose( pdf->fp ) != 0);
   free( pdf->offset );
   free( pdf->pages );
   free( pdf );
   return( status );
}
