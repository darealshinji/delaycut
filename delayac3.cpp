/*
delaycut: cuts and corrects delay in ac3, dts and wav files
Copyright (C) 2005 by jsoto

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
// delayac3.cpp 
//
// Based on delayac3 console version 0.27
//
// 2007-09-11 E-AC3 support added by www.madshi.net
//
#include "stdafx.h"
#include "delayac3.h"
#include "sil48.h"

#include "ToolTipDialog.h"
#include "delaycut.h"
#include "delaycutDlg.h"
#include "stdio.h"
#include "string.h"
#include "io.h"
#include "sys\types.h"
#include "sys\stat.h"

#define NUMREAD 8
#define MAXBYTES_PER_FRAME 2*1280
#define MAXLOGERRORS 100
#define debug  0
#define CRC16_POLY ((1 << 0) | (1 << 2) | (1 << 15) | (1 << 16))

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern CDelaycutApp theApp;

unsigned long int p_bit;
unsigned int crc_table[256];
bool bTooManyErrors;

////////////////////////////////////////////////////////////////////////
//      ac3_crc_init
////////////////////////////////////////////////////////////////////////

// static 
void ac3_crc_init(void)
{
    unsigned int c, n, k;

    for(n=0;n<256;n++)
	{
        c = n << 8;
        for (k = 0; k < 8; k++) {
            if (c & (1 << 15)) 
                c = ((c << 1) & 0xffff) ^ (CRC16_POLY & 0xffff);
            else
                c = c << 1;
        }
        crc_table[n] = c;
    }
}

////////////////////////////////////////////////////////////////////////
//      ac3_crc: Calculates the crc (byte per byte)
////////////////////////////////////////////////////////////////////////
static unsigned int ac3_crc(unsigned char *data, int n, unsigned int crc)
{
    int i;
	// loop in bytes
    for(i=0;i<n;i++)
	{
        crc = (crc_table[(data[i] & 0xff)^ (crc >> 8)] ^ (crc << 8)) & 0xffff;
    }
    return crc;
}

////////////////////////////////////////////////////////////////////////
//      ac3_crc_bit: Calculates the crc one bit 
////////////////////////////////////////////////////////////////////////
static unsigned int ac3_crc_bit(unsigned char *data, int n, unsigned int crc)
{
    int bit;


	bit=(data[0] >> n) & 1;

	if (((crc >> 15) & 1 ) ^ bit) 
		crc = ((crc << 1) & 0xffff) ^ (CRC16_POLY & 0xffff);
	else
		crc = (crc << 1 ) & 0xffff;

	return crc;
}



////////////////////////////////////////////////////////////////////////
//      mul_poply and pow_poly: Operations with polynomials
////////////////////////////////////////////////////////////////////////
static unsigned int mul_poly(unsigned int a, unsigned int b, unsigned int poly)
{
    unsigned int c;

    c = 0;
    while (a) {
        if (a & 1)
            c ^= b;
        a = a >> 1;
        b = b << 1;
        if (b & (1 << 16))
            b ^= poly;
    }
    return c;
}

static unsigned int pow_poly(unsigned int a, unsigned int n, unsigned int poly)
{
    unsigned int r;
    r = 1;
    while (n) {
        if (n & 1)
            r = mul_poly(r, a, poly);
        a = mul_poly(a, a, poly);
        n >>= 1;
    }
    return r;
}

////////////////////////////////////////////////////////////////////////
//      writedtsframe
////////////////////////////////////////////////////////////////////////

void writedtsframe (FILE *fileout, unsigned char *p_frame)
{
	unsigned long int BytesPerFrame;

	BytesPerFrame=((p_frame[5]&0x3)*256 + p_frame[6])*16 + (p_frame[7]&0xF0)/16 +1;

	if (BytesPerFrame > MAXFRAMESIZE) BytesPerFrame = MAXFRAMESIZE;

	fwrite(p_frame,sizeof (unsigned char), BytesPerFrame, fileout);

	return;
}

////////////////////////////////////////////////////////////////////////
//      writeac3frame
////////////////////////////////////////////////////////////////////////

void writeac3frame (FILE *fileout, unsigned char *p_frame)
{
	unsigned long int frmsizecod, nubytes;

//	frmsizecod=p_frame[4] & 0x3F;
	frmsizecod=p_frame[4];
	nubytes=FrameSize_ac3[frmsizecod]*2;

	fwrite(p_frame,sizeof (unsigned char), nubytes, fileout);

	return;
}

////////////////////////////////////////////////////////////////////////
//      writeeac3frame
////////////////////////////////////////////////////////////////////////

void writeeac3frame (FILE *fileout, unsigned char *p_frame)
{
	unsigned long int nubytes;

 nubytes = (((p_frame[2] & 7) << 8) + p_frame[3] + 1) * 2;

	fwrite(p_frame,sizeof (unsigned char), nubytes, fileout);

	return;
}

////////////////////////////////////////////////////////////////////////
//      writewavsample
////////////////////////////////////////////////////////////////////////

void writewavsample (FILE *fileout, unsigned char *p_frame, int nubytes)
{
	fwrite(p_frame,sizeof (unsigned char), nubytes, fileout);

	return;
}
////////////////////////////////////////////////////////////////////////
//      writempaframe
////////////////////////////////////////////////////////////////////////

void writempaframe (FILE *fileout, unsigned char *p_frame)
{
	unsigned long int BytesPerFrame;
	unsigned int layer, bitrate, rate, fsample, padding_bit;


	layer=(p_frame[1] & 0x6)>>1;
	if (layer==3) layer=1;
	else if (layer==1) layer=3;
/*
	For Layer I the following equation is valid: 
	N = 12 * bit_rate / sampling_frequency.
	For Layers II and III the equation becomes: 
	N = 144 * bit_rate / sampling_frequency.
	 384000 / 44100
*/
	rate=p_frame[2]>>4;

	bitrate=0;
	if (layer==3)
		bitrate=layerIIIrate[rate];
	else if (layer==2)
		bitrate=layerIIrate[rate];
	else if (layer==1)
		bitrate=layerIrate[rate];
	bitrate*=1000;

	fsample=(p_frame[2]>>2) & 0x3;

	if (fsample==0)	fsample=44100;
	else if (fsample==1)	fsample=48000;
	else if (fsample==2)	fsample=32000;
	else fsample=44100;

	padding_bit=(p_frame[2]>>1) & 0x1;
	if (layer==1)
	{
		BytesPerFrame=  (12 * bitrate)/fsample;
		if ( padding_bit) BytesPerFrame++;
	}
	else
	{
		BytesPerFrame= (144*bitrate)/fsample;
		if ( padding_bit) BytesPerFrame++;
	}

	if (BytesPerFrame > MAXFRAMESIZE) BytesPerFrame = MAXFRAMESIZE;

	fwrite(p_frame,sizeof (unsigned char), BytesPerFrame, fileout);

	return;
}

////////////////////////////////////////////////////////////////////////
//      readac3frame
////////////////////////////////////////////////////////////////////////

int readac3frame (FILE *filein, unsigned char *p_frame)
{
	int i, frmsizecod,BytesPerFrame;
	int nRead,nNumRead;

	p_bit=0;
	for (i=0;!feof(filein) && i < NUMREAD ;i++)
	{
		p_frame[i]=fgetc(filein);
	}

	nNumRead=i;

	if (feof(filein)) 
		return i;

//	frmsizecod=p_frame[4] & 0x3F;
	frmsizecod=p_frame[4];
	BytesPerFrame=FrameSize_ac3[frmsizecod]*2;

	if (BytesPerFrame > MAXFRAMESIZE) BytesPerFrame = MAXFRAMESIZE;

	nRead=fread(&p_frame[nNumRead],sizeof (unsigned char),BytesPerFrame-nNumRead,filein);
	
	return  nRead+nNumRead;
}

////////////////////////////////////////////////////////////////////////
//      readeac3frame
////////////////////////////////////////////////////////////////////////

int readeac3frame (FILE *filein, unsigned char *p_frame)
{
	int i, BytesPerFrame;
	int nRead,nNumRead;

	p_bit=0;
	for (i=0;!feof(filein) && i < NUMREAD ;i++)
	{
		p_frame[i]=fgetc(filein);
	}

	nNumRead=i;

	if (feof(filein)) 
		return i;

 BytesPerFrame = (((p_frame[2] & 7) << 8) + p_frame[3] + 1) * 2;

	if (BytesPerFrame > MAXFRAMESIZE) BytesPerFrame = MAXFRAMESIZE;

	nRead=fread(&p_frame[nNumRead],sizeof (unsigned char),BytesPerFrame-nNumRead,filein);
	
	return  nRead+nNumRead;
}

////////////////////////////////////////////////////////////////////////
//      readdtsframe
////////////////////////////////////////////////////////////////////////

int readdtsframe (FILE *filein, unsigned char *p_frame)
{
	int i, BytesPerFrame;
	int nRead,nNumRead;


	p_bit=0;

	// search for 7FFE8001 */
	for (i=0;!feof(filein) && i < 4 ;i++)
	{
		p_frame[i]=fgetc(filein);
	}

	nNumRead=i;

	if (feof(filein)) 
		return i;

/*	while (p_frame[0] != 0x7F || p_frame[1] != 0xFE || 
			p_frame[2] != 0x80 || p_frame[3] != 0x01)
	{
			fseek(filein, -3L, SEEK_CUR);
				// search for 7FFE8001 
			for (i=0;!feof(filein) && i < 4 ;i++)
					p_frame[i]=fgetc(filein);
		
	}
*/
	for (i=4;!feof(filein) && i < 8 ;i++)
	{
		p_frame[i]=fgetc(filein);
	}

	nNumRead=i;

	if (feof(filein)) 
		return i;

	BytesPerFrame=((p_frame[5]&0x3)*256 + p_frame[6])*16 + (p_frame[7]&0xF0)/16 +1;

	if (BytesPerFrame > MAXFRAMESIZE) BytesPerFrame = MAXFRAMESIZE;

	nRead=fread(&p_frame[nNumRead],sizeof (unsigned char),BytesPerFrame-nNumRead,filein);
	
	return nRead+nNumRead;
}

////////////////////////////////////////////////////////////////////////
//      readmpaframe
////////////////////////////////////////////////////////////////////////

int readmpaframe (FILE *filein, unsigned char *p_frame)
{
	int i, BytesPerFrame;
	int nRead,nNumRead;
	int fsample,bitrate,rateidx,layer,padding_bit;


	p_bit=0;

	// search for FFF */
	for (i=0;!feof(filein) && i < 2 ;i++)
	{
		p_frame[i]=fgetc(filein);
	}

/*	while (p_frame[0] != 0xFF || (p_frame[1] & 0xF0 )!= 0xF0 )
	{
			fseek(filein, -1L, SEEK_CUR);
				// search for FFF 
			for (i=0;!feof(filein) && i < 2 ;i++)
					p_frame[i]=fgetc(filein);
			
	}
*/
	for (i=2;!feof(filein) && i < 4 ;i++)
	{
		p_frame[i]=fgetc(filein);
	}
	
	nNumRead=i;

	if (feof(filein)) 
		return i;

	layer=(p_frame[1] & 0x6)>>1;
	if (layer==3) layer=1;
	else if (layer==1) layer=3;
/*
	For Layer I the following equation is valid: 
	N = 12 * bit_rate / sampling_frequency.
	For Layers II and III the equation becomes: 
	N = 144 * bit_rate / sampling_frequency.
	 384000 / 44100
*/
	rateidx=p_frame[2]>>4;

	bitrate=0;
	if (layer==3)
		bitrate=layerIIIrate[rateidx];
	else if (layer==2)
		bitrate=layerIIrate[rateidx];
	else if (layer==1)
		bitrate=layerIrate[rateidx];
	bitrate*=1000;

	fsample=(p_frame[2]>>2) & 0x3;

	if (fsample==0)	fsample=44100;
	else if (fsample==1)	fsample=48000;
	else if (fsample==2)	fsample=32000;
	else fsample=44100;

	padding_bit=(p_frame[2]>>1) & 0x1;

	if (layer==1)
	{
		BytesPerFrame=  (12 * bitrate)/fsample;
		if ( padding_bit) BytesPerFrame++;
	}
	else
	{
		BytesPerFrame= (144*bitrate)/fsample;
		if ( padding_bit) BytesPerFrame++;
	}

	if (BytesPerFrame > MAXFRAMESIZE) BytesPerFrame = MAXFRAMESIZE;

	nRead=0;
	if (BytesPerFrame ==0)
		BytesPerFrame=4;
	else
		nRead=fread(&p_frame[nNumRead],sizeof (unsigned char),BytesPerFrame-nNumRead,filein);

	return nRead+nNumRead;
}
////////////////////////////////////////////////////////////////////////
//      readwavsample
////////////////////////////////////////////////////////////////////////

int readwavsample (FILE *filein, unsigned char *p_frame, int nubytes)
{
	int nRead;

	nRead=fread(p_frame,sizeof (unsigned char), nubytes ,filein);
	
	return  nRead;
}

////////////////////////////////////////////////////////////////////////
//      getbits
////////////////////////////////////////////////////////////////////////
unsigned int getbits (int number, unsigned char *p_frame)
{
	unsigned int bit_ini,byte_ini,bit_end,byte_end, output;

	byte_ini=p_bit/8;
	bit_ini=p_bit%8;

	p_bit+=number;

	byte_end=p_bit/8;
	bit_end=p_bit%8;

	if (byte_end==byte_ini)
		output= p_frame[byte_end];
	else if (byte_end==byte_ini+1)
		output= p_frame[byte_end-1]*256 + p_frame[byte_end];
	else
	    output= p_frame[byte_end-2]*256*256+p_frame[byte_end-1]*256+
		      p_frame[byte_end];
		  
	output=(output)>>(8-bit_end);
	output=output & ((1 << number)-1);
    
	return output;
}

int getwavinfo ( FILE *in, FILEINFO *fileinfo)
{
	unsigned long int BytesPerFrame,i,nextbyte;
	unsigned int iByterate, nChannels, nBits, iRate,fsample;
	unsigned char caracter[50];
	unsigned char mybuffer[20];
	struct  _stati64 statbuf;
	double  FrameDuration, FramesPerSecond;
	char     s_TimeLengthIni[120];
	__int64  nuframes, TimeLengthIni, rest,i64size;
	double dactrate;


// search for RIFF */
	for (i=0;!feof(in) && i < 12 ;i++)
		caracter[i]=fgetc(in);

	if (caracter[0] != 'R' || caracter[1] != 'I' || 
		caracter[2] != 'F' || caracter[3] != 'F' ||
		caracter[8] != 'W' || caracter[9] != 'A' || 
		caracter[10] != 'V' || caracter[11] != 'E' ) return -1;

// search for fmt //

	fseek(in, 12, SEEK_SET);
	for (i=0;!feof(in) && i < 4 ;i++)
		mybuffer[i]=fgetc(in);
	nextbyte=16;

	while ((mybuffer[0] != 'f' || mybuffer[1] != 'm' || 
			mybuffer[2] != 't' || mybuffer[3] != ' ') && !feof(in))
	{
			fseek(in, -3L, SEEK_CUR);
				// search for 7FFE8001 */
			for (i=0;!feof(in) && i < 4 ;i++)
				mybuffer[i]=fgetc(in);
			nextbyte++;
	}
	if (mybuffer[0] != 'f' || mybuffer[1] != 'm' || 
			mybuffer[2] != 't' || mybuffer[3] != ' ')  return -1;

	fseek(in, -4L, SEEK_CUR);

	for (i=0;i<24;i++)
		caracter[12+i]=fgetc(in);
	nextbyte+=20;

// search for data //

	for (i=0;!feof(in) && i < 4 ;i++)
		mybuffer[i]=fgetc(in);
	nextbyte+=4;

	while ((mybuffer[0] != 'd' || mybuffer[1] != 'a' || 
			mybuffer[2] != 't' || mybuffer[3] != 'a') && !feof(in))
	{
			fseek(in, -3L, SEEK_CUR);
				// search for data */
			for (i=0;!feof(in) && i < 4 ;i++)
				mybuffer[i]=fgetc(in);
			nextbyte++;
	}
	if (mybuffer[0] != 'd' || mybuffer[1] != 'a' || 
			mybuffer[2] != 't' || mybuffer[3] != 'a')  return -1;

	fseek(in, -4L, SEEK_CUR);

	for (i=0;i<8;i++)
		caracter[24+12+i]=fgetc(in);

	nextbyte+=4;

	_fstati64 (fileno(in), &statbuf);

	BytesPerFrame=	caracter[32]+caracter[33]*256;
	i64size=	(UINT)(caracter[40]+(caracter[41]<<8)+(caracter[42]<<16)+(caracter[43]<<24));
	if (i64size > statbuf.st_size-nextbyte) i64size= statbuf.st_size-nextbyte;
	nuframes=       i64size/BytesPerFrame;
	rest=           i64size%nuframes;
	fsample=		caracter[24]+(caracter[25]<<8)+(caracter[26]<<16)+(caracter[27]<<24);
	iByterate=		caracter[28]+(caracter[29]<<8)+(caracter[30]<<16)+(caracter[31]<<24);
	nChannels=		caracter[22]+(caracter[23]<<8);
	nBits=			caracter[34]+(caracter[35]<<8);
	iRate=			(unsigned int)(BytesPerFrame*fsample*8);
	dactrate=		(double)iByterate*8;

	FramesPerSecond=fsample;
	FrameDuration=  1000.0/FramesPerSecond; // in msecs
	TimeLengthIni=	(__int64)((double)(statbuf.st_size-nextbyte)*1000.0/(dactrate/8)); // in msecs
	sprintf (s_TimeLengthIni, "%02I64d:%02I64d:%02I64d.%03I64d",
		TimeLengthIni/(3600000), (TimeLengthIni%3600000)/60000,
		(TimeLengthIni%60000)/1000, TimeLengthIni%1000);

	fileinfo->csOriginalDuration.Format(_T("%s"),s_TimeLengthIni);
	fileinfo->fsample=(int)fsample;
	fileinfo->layer=0;
	fileinfo->csType="wav";
	fileinfo->dFrameduration=FrameDuration;
	fileinfo->i64Filesize=statbuf.st_size;
	fileinfo->iInidata=nextbyte;
	fileinfo->i64TotalFrames=nuframes;
	fileinfo->iBitrate=iRate;
	fileinfo->iBits=nBits;
	fileinfo->iByterate=iByterate;
	fileinfo->dactrate=dactrate;
	fileinfo->dBytesperframe=(double)BytesPerFrame;
	fileinfo->csMode.Format(_T("%d Channels"),nChannels);
	fileinfo->dFramesPerSecond=FramesPerSecond;
	fileinfo->i64rest=rest;
	fileinfo->fscod=0;
	fileinfo->acmod=0;
	fileinfo->cpf=0;
	fileinfo->i64frameinfo=0;


/////////////////////////////////////////////////////////////////////////////
// Fill silence frame, If bitrate is not included, use first frame
/////////////////////////////////////////////////////////////////////////////
	for (i=0; i<BytesPerFrame; i++)
		silence[i]=0;
	p_silence=silence;

	return 0;
}

int getdtsinfo ( FILE *in, FILEINFO *fileinfo)

{
	unsigned long int nubytes,BytesPerFrame,i;
	unsigned char caracter[MAXFRAMESIZE];
	unsigned long int syncword,	ftype, fshort, cpf, nblks;
	unsigned long int fsize, amode, sfreq, rate, lfeon, unused;    
	struct            _stati64 statbuf;
	double fsample, FrameDuration, FramesPerSecond;
	char     s_TimeLengthIni[120];
	__int64  nuframes, TimeLengthIni, rest;
	double dactrate;


	p_silence=dts_768k_48;

	// search for 7FFE8001 */
	for (i=0;!feof(in) && i < 4 ;i++)
		caracter[i]=fgetc(in);

	while ((caracter[0] != 0x7F || caracter[1] != 0xFE || 
			caracter[2] != 0x80 || caracter[3] != 0x01) && !feof(in))
	{
			fseek(in, -3L, SEEK_CUR);
				// search for 7FFE8001 */
			for (i=0;!feof(in) && i < 4 ;i++)
					caracter[i]=fgetc(in);

	}

	if (caracter[0] != 0x7F || caracter[1] != 0xFE || 
		caracter[2] != 0x80 || caracter[3] != 0x01) return -1;

	fseek(in, -4L, SEEK_CUR);

	nubytes=readdtsframe (in, caracter);

	syncword=   getbits (32, caracter);
	ftype=      getbits (1, caracter);
	fshort=     getbits (5, caracter);
	cpf=        getbits (1, caracter);
	nblks=      getbits (7, caracter);
	fsize=      getbits (14,caracter);
	amode=      getbits (6, caracter);
	sfreq=      getbits (4, caracter);
	rate=       getbits (5, caracter);
	unused=		getbits (10, caracter);
	lfeon=		getbits (1, caracter);


	rate=dtsbitrate[rate];
	dactrate=(double)rate;
	if (rate==768) dactrate=754.5;
	if (rate== 1536) dactrate=1509.75;
	fsample=((double)(dtsfsample[sfreq]))/1000.0;

	_fstati64 (fileno(in), &statbuf);

	BytesPerFrame=fsize+1; 
	nuframes=       statbuf.st_size/BytesPerFrame;
	rest=           statbuf.st_size%nuframes;
	FramesPerSecond=(dactrate)*1000.0/(BytesPerFrame * 8);
	FrameDuration=  1000.0/FramesPerSecond; // in msecs
	TimeLengthIni=	(__int64)((double)(statbuf.st_size)/(dactrate/8)); // in msecs
	sprintf (s_TimeLengthIni, "%02I64d:%02I64d:%02I64d.%03I64d",
		TimeLengthIni/(3600000), (TimeLengthIni%3600000)/60000,
		(TimeLengthIni%60000)/1000, TimeLengthIni%1000);


	fileinfo->csOriginalDuration.Format(_T("%s"),s_TimeLengthIni);
	fileinfo->fsample=48000;
	fileinfo->layer=0;
	fileinfo->csType="dts";
	fileinfo->dFrameduration=FrameDuration;
	fileinfo->i64Filesize=statbuf.st_size;
	fileinfo->i64TotalFrames=nuframes;
	fileinfo->iBitrate=rate;
	fileinfo->dactrate=dactrate;
	fileinfo->dBytesperframe=(double)BytesPerFrame;
	if		(amode==0)		fileinfo->csMode="Mono";
	else if (amode==1)		fileinfo->csMode="A+B (Dual Mono)";
	else if (amode==2)		fileinfo->csMode="L+R (Stereo)";
	else if (amode==3)		fileinfo->csMode="(L+R) + (L-R): (Sum + Diff)";
	else if (amode==4)		fileinfo->csMode="LT + RT ";
	else if (amode==5)		fileinfo->csMode="C+L+R";
	else if (amode==6)		fileinfo->csMode="L+R+S";
	else if (amode==7)		fileinfo->csMode="C+L+R+S";
	else if (amode==8)		fileinfo->csMode="L+R+SL+SR";
	else if (amode==9)		fileinfo->csMode="C+L+R+SL+SR";
	else if (amode==10)		fileinfo->csMode="CL+CR+L+R+SL+SR";
	else if (amode==11)		fileinfo->csMode="C+L+R+LR+RR+OV";
	else if (amode==12)		fileinfo->csMode="CF+CR+LF+RF+LR+RR";
	else if (amode==13)		fileinfo->csMode="CL+C+CR+L+R+SL+SR";
	else if (amode==14)		fileinfo->csMode="CL+CR+L+R+SL1+SL2+SR1+SR2";
	else if (amode==15)		fileinfo->csMode="CL+C+CR+L+R+SL+S+SR";
	else 					fileinfo->csMode="User defined";
	if (lfeon)				fileinfo->csLFE="LFE: Present";
	else					fileinfo->csLFE="LFE: Not present";
	fileinfo->dFramesPerSecond=FramesPerSecond;
	fileinfo->i64rest=rest;
	fileinfo->fscod=sfreq;
	fileinfo->acmod=amode;
	fileinfo->cpf=cpf;
	fileinfo->i64frameinfo=1;


/////////////////////////////////////////////////////////////////////////////
// Fill silence frame, If bitrate is not included, use first frame
/////////////////////////////////////////////////////////////////////////////
	for (i=0; i<nubytes; i++)
		silence[i]=caracter[i];
	p_silence=silence;
	if (rate == 768)
		p_silence=dts_768k_48;
	if (rate == 1536)
		p_silence=dts_1536k_48;

	return 0;
}


int getac3info ( FILE *in, FILEINFO *fileinfo)

{
// BSI variables

	unsigned long int syncword, crc1, fscod,frmsizecod;
	unsigned long int bsid, bsmod, acmod, surmixlev, cmixlev, dsurmod;
	unsigned long int lfeon,dialnorm,compre, compr;

// Other vars
	unsigned long int nubytes,rate, BytesPerFrame, i;
	char  mode[40];
	struct            _stati64 statbuf;
	double fsample, FrameDuration, FramesPerSecond;
	char     s_TimeLengthIni[120];
	__int64  nuframes, TimeLengthIni, rest;
	CString csRet,csAux;
	unsigned char caracter[MAXFRAMESIZE];

	unsigned int frame_size, frame_size_58, cal_crc2, cal_crc1, crc_inv;

// init ac3_crc
//	ac3_crc_init(); Done in Initinstance


	fseek(in, (long)0, SEEK_SET);

// Read first frame
	nubytes=readac3frame (in, caracter);
//	frmsizecod=caracter[4] & 0x3F;
	frmsizecod=caracter[4];
	BytesPerFrame=FrameSize_ac3[frmsizecod]*2;
	while (( caracter[0]!= 0x0B || caracter[1]!=0x77 || 
		    nubytes < 12 || nubytes != BytesPerFrame) && !feof(in))
	{
// Try to find the next sync Word and continue...
			// rewind nubytes (last frame)
		fseek(in, (long)(-1*nubytes), SEEK_CUR);
		caracter[0]=fgetc(in);
		caracter[1]=fgetc(in);
		while ((caracter[0]!= 0x0B || caracter[1]!=0x77) && !feof(in) )
		{
 			caracter[0]= caracter[1];
 			caracter[1]= fgetc(in);
		}
		if (caracter[0]== 0x0B && caracter[1]==0x77) 
		{
			// rewind 2 bytes
			fseek(in, -2L, SEEK_CUR);
			nubytes=readac3frame (in, caracter);
//			frmsizecod=caracter[4]& 0x3F;
			frmsizecod=caracter[4];
			BytesPerFrame=FrameSize_ac3[frmsizecod]*2;
		}
	}

	
	if (nubytes <12 || caracter[0]!= 0x0B || caracter[1]!=0x77)
		return -1;

	surmixlev=cmixlev=dsurmod=compr=0;
	syncword=   getbits (16, caracter);
	crc1=       getbits (16, caracter);
	fscod=      getbits (2,  caracter);
	frmsizecod= getbits (6,  caracter);
	bsid=       getbits (5,  caracter);
	bsmod=      getbits (3,  caracter);
	acmod=      getbits (3,  caracter);
	if ((acmod & 0x01) && (acmod != 0x01)) cmixlev=getbits(2, caracter);
	if (acmod & 0x4)	surmixlev=getbits(2, caracter);
	if (acmod == 0x2) dsurmod=getbits(2, caracter);
	lfeon=      getbits(1, caracter);
	dialnorm=   getbits(5, caracter);
	compre=     getbits(1, caracter);
	if (compre)	compr=getbits(8, caracter);

	if (fscod==0)		fsample=48.0;
	else if (fscod==1)	fsample=44.1;
	else if (fscod==2)	fsample=32.0 ;
	else fsample=0.0;

	if      (acmod==0) strcpy (mode,"1+1: A+B");
	else if (acmod==1) strcpy (mode,"1/0: C");
	else if (acmod==2) strcpy (mode,"2/0: L+R");
	else if (acmod==3) strcpy (mode,"3/0: L+C+R");
	else if (acmod==4) strcpy (mode,"2/1: L+R+S");
	else if (acmod==5) strcpy (mode,"3/1: L+C+R+S");
	else if (acmod==6) strcpy (mode,"2/2: L+R+SL+SR");
	else if (acmod==7) strcpy (mode,"3/2: L+C+R+SL+SR");

	rate=bitrate[frmsizecod];
 
	_fstati64 (fileno(in), &statbuf);

//	BytesPerFrame=  FrameSize_48[frmsizecod ]*2;
	BytesPerFrame=  FrameSize_ac3[frmsizecod + fscod*64]*2;
	nuframes=       statbuf.st_size/BytesPerFrame;
	rest=           statbuf.st_size%nuframes;
	FramesPerSecond=((double)(rate))*1000.0/(BytesPerFrame * 8);
	FrameDuration=  1000.0/FramesPerSecond; // in msecs
	TimeLengthIni=	statbuf.st_size/(rate/8); // in msecs
	sprintf (s_TimeLengthIni, "%02I64d:%02I64d:%02I64d.%03I64d",
		TimeLengthIni/(3600000), (TimeLengthIni%3600000)/60000,
		(TimeLengthIni%60000)/1000, TimeLengthIni%1000);

	fileinfo->csOriginalDuration.Format(_T("%s"),s_TimeLengthIni);
	fileinfo->fsample=(int)(fsample*1000);
	fileinfo->layer=0;
	fileinfo->csType="ac3";
	fileinfo->dFrameduration=FrameDuration;
	fileinfo->i64Filesize=statbuf.st_size;
	fileinfo->i64TotalFrames=nuframes;
	fileinfo->iBitrate=rate;
	fileinfo->dactrate=(double)rate;
	fileinfo->dBytesperframe=(double)BytesPerFrame;
	fileinfo->csMode.Format(_T("%s"), mode);
	fileinfo->dFramesPerSecond=FramesPerSecond;
	if (lfeon)	fileinfo->csLFE="LFE: Present";
	else		fileinfo->csLFE="LFE: Not present";
	fileinfo->i64rest=rest;
	fileinfo->bsmod=bsmod;
	fileinfo->acmod=acmod;
	fileinfo->frmsizecod=frmsizecod;
	fileinfo->fscod=fscod;
	fileinfo->cpf=1;
	fileinfo->i64frameinfo=1;

/////////////////////////////////////////////////////////////////////////////
// Fill silence frame, If acmod or bitrate is not included, use first frame
/////////////////////////////////////////////////////////////////////////////

	p_silence=silence;
	for (i=0; i<nubytes; i++)
		silence[i]=caracter[i];
	
	nubytes=FrameSize_ac3[frmsizecod]*2;
/////////////////////////////////////////////////////////////////////////////
// Fill silence frame in a general way.... Only two patterns (6 and 2 channels)
/////////////////////////////////////////////////////////////////////////////

	if (acmod ==7 && nubytes >=432) 
	{
		for (i=0;i<2048;i++)
			silence[i]=0;
		for (i=0;i<432;i++)
			silence[i]=ac3_6channels[i];
		silence[4]=(unsigned char)(fscod*64+frmsizecod);
	}
	else if (acmod ==2 && nubytes >=176) 
	{
		for (i=0;i<2048;i++)
			silence[i]=0;
		for (i=0;i<176;i++)
			silence[i]=ac3_2channels[i];
		silence[4]=(unsigned char)(fscod*64+frmsizecod);
	}

////////////////////////////////////////
// 	Silence frame: CRC calculation and fixing 
//
	frame_size=  FrameSize_ac3[frmsizecod+64*fscod];
	frame_size_58 = (frame_size >> 1) + (frame_size >> 3);

	cal_crc1 = ac3_crc(silence + 4, (2 * frame_size_58) - 4, 0);
  	crc_inv  = pow_poly((CRC16_POLY >> 1), (16 * frame_size_58) - 16, CRC16_POLY);
	cal_crc1 = mul_poly(crc_inv, cal_crc1, CRC16_POLY);
	cal_crc2 = ac3_crc(silence + 2 * frame_size_58, (frame_size - frame_size_58) * 2 - 2, 0);
	
	silence[2]=(cal_crc1 >> 8);
	silence[3]=cal_crc1 & 0xff;
	silence[2*frame_size - 2]=(cal_crc2 >> 8);
	silence[2*frame_size - 1]=cal_crc2 & 0xff;
		
	return 0;

}

int geteac3info ( FILE *in, FILEINFO *fileinfo)

{
// BSI variables

	unsigned long int syncword, strmtyp, substreamid, frmsize, fscod, fscod2, numblckscod, numblcks;
	unsigned long int bsid, acmod;
	unsigned long int lfeon,dialnorm,compre, compr;

// Other vars
	unsigned long int nubytes,rate, BytesPerFrame, i;
	char  mode[40];
	struct            _stati64 statbuf;
	double fsample, FrameDuration, FramesPerSecond;
	char     s_TimeLengthIni[120];
	__int64  nuframes, TimeLengthIni, rest;
	CString csRet,csAux;
	unsigned char caracter[MAXFRAMESIZE];

// init ac3_crc
//	ac3_crc_init(); Done in Initinstance

	fseek(in, (long)0, SEEK_SET);

// Read first frame
	nubytes=readeac3frame (in, caracter);
 BytesPerFrame = (((caracter[2] & 7) << 8) + caracter[3] + 1) * 2;
	while (( caracter[0]!= 0x0B || caracter[1]!=0x77 || 
		    nubytes < 12 || nubytes != BytesPerFrame) && !feof(in))
	{
// Try to find the next sync Word and continue...
			// rewind nubytes (last frame)
		fseek(in, (long)(-1*nubytes), SEEK_CUR);
		caracter[0]=fgetc(in);
		caracter[1]=fgetc(in);
		while ((caracter[0]!= 0x0B || caracter[1]!=0x77) && !feof(in) )
		{
 			caracter[0]= caracter[1];
 			caracter[1]= fgetc(in);
		}
		if (caracter[0]== 0x0B && caracter[1]==0x77) 
		{
			// rewind 2 bytes
			fseek(in, -2L, SEEK_CUR);
			nubytes=readeac3frame (in, caracter);
   BytesPerFrame = (((caracter[2] & 7) << 8) + caracter[3] + 1) * 2;
		}
	}

	
	if (nubytes <12 || caracter[0]!= 0x0B || caracter[1]!=0x77)
		return -1;

	syncword=   getbits (16, caracter);
 strmtyp=    getbits (2,  caracter);
 substreamid=getbits (3,  caracter);
 frmsize=    getbits (11, caracter);
	fscod=      getbits (2,  caracter);
 fscod2=     getbits (2,  caracter);
 numblckscod=fscod2;
	acmod=      getbits (3,  caracter);
	lfeon=      getbits (1,  caracter);
	bsid=       getbits (5,  caracter);
	dialnorm=   getbits (5,  caracter);
	compre=     getbits (1,  caracter);
	if (compre)	compr=getbits(8, caracter);

	if (fscod==0)	fsample=48.0;
	else if (fscod==1)	fsample=44.1;
	else if (fscod==2)	fsample=32.0;
	else 
 {
   numblckscod = 3;
   if (fscod2==0) fsample=24.0;
   else if (fscod==1) fsample=22.05;
   else if (fscod==2) fsample=16.0;
   else fsample=0.0;
 }

 if (numblckscod==0) numblcks=1;
 else if (numblckscod==1) numblcks=2;
 else if (numblckscod==2) numblcks=3;
 else numblcks=6;

	if      (acmod==0) strcpy (mode,"1+1: A+B");
	else if (acmod==1) strcpy (mode,"1/0: C");
	else if (acmod==2) strcpy (mode,"2/0: L+R");
	else if (acmod==3) strcpy (mode,"3/0: L+C+R");
	else if (acmod==4) strcpy (mode,"2/1: L+R+S");
	else if (acmod==5) strcpy (mode,"3/1: L+C+R+S");
	else if (acmod==6) strcpy (mode,"2/2: L+R+SL+SR");
	else if (acmod==7) strcpy (mode,"3/2: L+C+R+SL+SR");

 rate = (BytesPerFrame * 8 * (750 / numblcks)) / 4000;
 
	_fstati64 (_fileno(in), &statbuf);

	nuframes=       statbuf.st_size/BytesPerFrame;
	rest=           statbuf.st_size%nuframes;
	FramesPerSecond=((double)(rate))*1000.0/(BytesPerFrame * 8);
	FrameDuration=  1000.0/FramesPerSecond; // in msecs
	TimeLengthIni=	statbuf.st_size/(rate/8); // in msecs
	sprintf (s_TimeLengthIni, "%02I64d:%02I64d:%02I64d.%03I64d",
		TimeLengthIni/(3600000), (TimeLengthIni%3600000)/60000,
		(TimeLengthIni%60000)/1000, TimeLengthIni%1000);

	fileinfo->csOriginalDuration.Format(_T("%s"),s_TimeLengthIni);
	fileinfo->fsample=(int)(fsample*1000);
	fileinfo->layer=0;
	fileinfo->csType="eac3";
	fileinfo->dFrameduration=FrameDuration;
	fileinfo->i64Filesize=statbuf.st_size;
	fileinfo->i64TotalFrames=nuframes;
	fileinfo->iBitrate=rate;
	fileinfo->dactrate=(double)rate;
	fileinfo->dBytesperframe=(double)BytesPerFrame;
	fileinfo->csMode.Format(_T("%s"), mode);
	fileinfo->dFramesPerSecond=FramesPerSecond;
	if (lfeon)	fileinfo->csLFE="LFE: Present";
	else		fileinfo->csLFE="LFE: Not present";
	fileinfo->i64rest=rest;
//	fileinfo->bsmod=bsmod;
	fileinfo->acmod=acmod;
//	fileinfo->frmsizecod=frmsizecod;
	fileinfo->fscod=fscod;
	fileinfo->cpf=1;
	fileinfo->i64frameinfo=1;

/////////////////////////////////////////////////////////////////////////////
// Fill silence frame, If acmod or bitrate is not included, use first frame
/////////////////////////////////////////////////////////////////////////////

	p_silence=silence;
	for (i=0; i<nubytes; i++)
		silence[i]=caracter[i];
	
	return 0;

}

int getmpainfo ( FILE *in, FILEINFO *fileinfo)

{
	unsigned long int nubytes,i;
	unsigned char caracter[MAXFRAMESIZE];
	unsigned int syncword,	iID, rate, fsamp, layer, protection_bit ;
	unsigned int padding_bit,private_bit,mode,mode_extension,copyright,original,emphasis;
	unsigned int crc_check;
	struct   _stati64 statbuf;
	double fsample, FrameDuration, FramesPerSecond,BytesPerFrame;
	char     s_TimeLengthIni[120];
	__int64  nuframes, TimeLengthIni, rest;
//	double dactrate;
/*
	syncword			12	bits	bslbf
	ID					1	bit	bslbf
	layer				2	bits	bslbf
	protection_bit		1	bit	bslbf
	bitrate_index		4	bits	bslbf
	sampling_frequency	2	bits	bslbf
	padding_bit			1	bit	bslbf
	private_bit			1	bit	bslbf
	mode				2	bits	bslbf
	mode_extension		2	bits	bslbf
	copyright			1	bit	bslbf
	original/home		1	bit	bslbf
	emphasis			2	bits	bslbf
	if  (protection_bit==0)
		 crc_check 	 16	bits	rpchof
*/
	// search for FFFX */
	for (i=0;!feof(in) && i < 2 ;i++)
		caracter[i]=fgetc(in);

	while (caracter[0] != 0xFF || (caracter[1] & 0xF0 ) != 0xF0 && !feof(in))
	{
			fseek(in, -1L, SEEK_CUR);
				// search for 0xFFFX */
			for (i=0;!feof(in) && i < 2 ;i++)
					caracter[i]=fgetc(in);

	}

	if (caracter[0] != 0xFF || (caracter[1] & 0xF0 ) != 0xF0 ) return -1;

	fseek(in, -2L, SEEK_CUR);

	nubytes=readmpaframe (in, caracter);

	syncword=			getbits (12, caracter);
	iID=				getbits (1, caracter);
	layer=				getbits (2, caracter);
	protection_bit=		getbits (1, caracter);
	rate=				getbits (4, caracter);
	fsamp=				getbits (2,caracter);
	padding_bit=		getbits (1, caracter);
	private_bit=		getbits (1, caracter);
	mode=				getbits (2, caracter);
	mode_extension=		getbits (2, caracter);
	copyright=			getbits (1, caracter);
	original=			getbits (1, caracter);
	emphasis=			getbits (2, caracter);

//	MpegVers= ((syncword & 0x1)<<1) + iID;
// FFFC--> MPeg 1
// FFF0--> MPeg 2
// FFE0--> MPeg 2.5
//      Case 0
//        AddInfo("2.5" + CR$)
//      Case 1
//        AddInfo("reserved" + CR$)
//      Case 2
//        AddInfo("2" + CR$)
//      Case 3
//        AddInfo("1" + CR$)

	if (protection_bit==0)
		crc_check=		getbits (16, caracter);

	if		(layer==1) layer=3;
	else if (layer==3) layer=1;

	if (layer==3)
		rate=layerIIIrate[rate];
	else if (layer==2)
		rate=layerIIrate[rate];
	else if (layer==1)
		rate=layerIrate[rate];
	else rate=0;

	if (rate<8) rate=8;

	if (fsamp==0)	fsamp=44100;
	else if (fsamp==1)	fsamp=48000;
	else if (fsamp==2)	fsamp=32000;
	else fsamp=44100;

	fsample=(double)fsamp;

	if (layer==1)
	{
		BytesPerFrame=  (12 * rate * 1000.0)/(double)fsamp;
//		if ( padding_bit) BytesPerFrame+=1;
	}
	else
	{
		BytesPerFrame= (144 * 1000.0 * rate)/(double)fsamp;
//		if ( padding_bit) BytesPerFrame++;
	}
	if (BytesPerFrame > MAXFRAMESIZE) BytesPerFrame = MAXFRAMESIZE;

	_fstati64 (fileno(in), &statbuf);

	nuframes=    (__int64) (statbuf.st_size/BytesPerFrame +0.5);
	rest=           statbuf.st_size%nuframes;
	FramesPerSecond=((double)(rate))*1000.0/(BytesPerFrame * 8);
	FrameDuration=  1000.0/FramesPerSecond; // in msecs
	TimeLengthIni=	statbuf.st_size/(rate/8); // in msecs
	sprintf (s_TimeLengthIni, "%02I64d:%02I64d:%02I64d.%03I64d",
		TimeLengthIni/(3600000), (TimeLengthIni%3600000)/60000,
		(TimeLengthIni%60000)/1000, TimeLengthIni%1000);

	fileinfo->layer=layer;
	fileinfo->fsample=fsamp;
	fileinfo->csOriginalDuration.Format(_T("%s"),s_TimeLengthIni);
	fileinfo->csType.Format(_T("mpeg 1 layer %d"),layer);
	fileinfo->dFrameduration=FrameDuration;
	fileinfo->i64Filesize=statbuf.st_size;
	fileinfo->i64TotalFrames=nuframes;
	fileinfo->iBitrate=rate;
	fileinfo->dactrate=rate;
	fileinfo->dBytesperframe=BytesPerFrame;
	fileinfo->mode=mode;
	if		(mode==0)		fileinfo->csMode="Stereo";
	else if (mode==1)		fileinfo->csMode="Joint Stereo";
	else if (mode==2)		fileinfo->csMode="Dual Channel (Dual Mono)";
	else if (mode==3)		fileinfo->csMode="Single Channel (Mono)";
	fileinfo->csLFE="LFE: Not present";
	fileinfo->dFramesPerSecond=FramesPerSecond;
	fileinfo->i64rest=rest;
//	fileinfo->fscod=sfreq;
//	fileinfo->acmod=amode;
	if (protection_bit==0)
		fileinfo->cpf=1;
	else
		fileinfo->cpf=0;

//	fileinfo->i64frameinfo=1;


/////////////////////////////////////////////////////////////////////////////
// Fill silence frame, If bitrate is not included, use first frame
/////////////////////////////////////////////////////////////////////////////
	for (i=0; i<nubytes; i++)
		silence[i]=caracter[i];
	p_silence=silence;

	if (layer==2 && fsamp==48000)
	{
		if (mode!=3) // Stereo (any mode)
		{
			if      (rate==32 ) 	p_silence=mp2_32k_s_48;
			else if (rate==48 ) 	p_silence=mp2_48k_s_48;
			else if (rate==56 ) 	p_silence=mp2_56k_s_48;
			else if (rate==64 ) 	p_silence=mp2_64k_s_48;
			else if (rate==80 ) 	p_silence=mp2_80k_s_48;
			else if (rate==96 ) 	p_silence=mp2_96k_s_48;
			else if (rate==112) 	p_silence=mp2_112k_s_48;
			else if (rate==128) 	p_silence=mp2_128k_s_48;
			else if (rate==160) 	p_silence=mp2_160k_s_48;
			else if (rate==192) 	p_silence=mp2_192k_s_48;
			else if (rate==224) 	p_silence=mp2_224k_s_48;
			else if (rate==256) 	p_silence=mp2_256k_s_48;
			else if (rate==320) 	p_silence=mp2_320k_s_48;
			else if (rate==384) 	p_silence=mp2_384k_s_48;

		}
		if (mode==3) // Mono
		{
			if      (rate==32 ) 	p_silence=mp2_32k_m_48;
			else if (rate==48 ) 	p_silence=mp2_48k_m_48;
			else if (rate==56 ) 	p_silence=mp2_56k_m_48;
			else if (rate==64 ) 	p_silence=mp2_64k_m_48;
			else if (rate==80 ) 	p_silence=mp2_80k_m_48;
			else if (rate==96 ) 	p_silence=mp2_96k_m_48;
			else if (rate==112) 	p_silence=mp2_112k_m_48;
			else if (rate==128) 	p_silence=mp2_128k_m_48;
			else if (rate==160) 	p_silence=mp2_160k_m_48;
			else if (rate==192) 	p_silence=mp2_192k_m_48;
			else if (rate==224) 	p_silence=mp2_224k_m_48;
			else if (rate==256) 	p_silence=mp2_256k_m_48;
			else if (rate==320) 	p_silence=mp2_320k_m_48;
			else if (rate==384) 	p_silence=mp2_384k_m_48;

		}
	}

	return 0;
}


int gettargetinfo ( FILEINFO *fileinfo)

{
	double dDelay,dDelayend, FrameDuration, FramesPerSecond, NotFixedDelay;
	double dDelay2, dStartsplit,dEndsplit;
	__int64 i64StartFrame, i64EndFrame;
	__int64 i64nuframes, i64TimeLengthEst;
	CString csTimeLengthEst;

	FramesPerSecond=fileinfo->dFramesPerSecond;
	FrameDuration=fileinfo->dFrameduration;
	dDelay=theApp.m_dDelaystart;
	dDelayend=theApp.m_dDelayend;
	i64nuframes=fileinfo->i64TotalFrames;
	dStartsplit=theApp.m_dCutstart;
	dEndsplit=theApp.m_dCutend;


/////////////////////////////////////////////////////////
// Translate split and delay parameters to number of frames
/////////////////////////////////////////////////////////	
  	if (dStartsplit >0  )
		dDelay2=dDelay-dStartsplit*1000.0;
	else
		dDelay2=dDelay;

	if (dDelay2 >=0)
		i64StartFrame= (__int64)((dDelay2+(FrameDuration/2.0))/FrameDuration);
	else
		i64StartFrame= (__int64)((dDelay2-(FrameDuration/2.0))/FrameDuration);

	i64StartFrame=-i64StartFrame;

	if (dEndsplit >0  )
		dDelay2=-dDelayend+dEndsplit*1000.0;
	else // endsplit=0 , so go until the end of the file
		dDelay2=-dDelayend+i64nuframes*FrameDuration;

	if (dDelay2 >=0)
		i64EndFrame= (__int64)((dDelay2+(FrameDuration/2.0))/FrameDuration);
	else
		i64EndFrame= (__int64)((dDelay2-(FrameDuration/2.0))/FrameDuration);
	
	i64TimeLengthEst=	(__int64)((i64EndFrame-i64StartFrame)*FrameDuration);
	csTimeLengthEst.Format(_T("%02I64d:%02I64d:%02I64d.%03I64d"),
		i64TimeLengthEst/(3600000), (i64TimeLengthEst%3600000)/60000,
		(i64TimeLengthEst%60000)/1000, i64TimeLengthEst%1000);

	NotFixedDelay= dDelay + i64StartFrame*FrameDuration-dStartsplit*1000.0;

	fileinfo->csTimeLengthEst=csTimeLengthEst;
	fileinfo->i64StartFrame=i64StartFrame;
	if (i64EndFrame == i64StartFrame)
		fileinfo->i64EndFrame=i64EndFrame;
	else
		fileinfo->i64EndFrame=i64EndFrame-1;

	fileinfo->dNotFixedDelay=NotFixedDelay;

	return 0;
}

void CDelaycutApp::printlog (CString csLinea)
{
	CString csAux;

	if (bTooManyErrors) return;

	fprintf (m_Flog,csLinea+"\n");

	if (m_bCLI)
	{
		if (m_bConsole)
			fprintf (stdout,csLinea+"\n");
	}
	else
	{
		csAux.Format( _T("%s\r\n"),csLinea);
		m_csLog+=csAux;
	}
}

void CDelaycutApp::printline (CString csLinea)
{
	CString csAux;
	static CString csLineaold;

	if (csLinea== csLineaold) return;

	csLineaold=csLinea;

	if (m_bConsole)
		fprintf (stdout,csLinea+"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
}
////////////////////////////////////////////////////////////////////////
//
//      Main
//
////////////////////////////////////////////////////////////////////////
int CDelaycutApp::delayac3(CWnd* pWnd)
{
	__int64 i64;
	__int64 i64StartFrame, i64EndFrame, i64nuframes, i64frameswritten,i64TotalFrames;
	FILEINFO *fileinfo;
	__int64  i64nubytes;
	double dFrameduration;
	int iBytesPerFramen,iBytesPerFramePrev;
	UINT iFrmsizecodn;
	UINT syncwordn,	crc1n, fscodn, bsidn, bsmodn, acmodn;  
	CString csTime,csAux,csAux1;
	int f_writeframe,nuerrors;
	unsigned int frame_size, frame_size_58, cal_crc2, cal_crc1, crc_inv;
	__int64 n64skip;
	bool bEndOfFile;
	bool bCRCError;

	unsigned char caracter[MAXFRAMESIZE];


	// init ac3_crc
//	ac3_crc_init(); Done in Initinstance

	fileinfo=&theApp.m_myinfo;

	i64StartFrame=fileinfo->i64StartFrame;
	i64EndFrame=fileinfo->i64EndFrame;
	i64nuframes=i64EndFrame-i64StartFrame+1;
	dFrameduration=fileinfo->dFrameduration;
	i64TotalFrames = fileinfo->i64TotalFrames;

	i64frameswritten=0;
	nuerrors=0;



	printlog ("====== PROCESSING LOG ======================");
	if (i64StartFrame > 0 && m_bAbort==false )
	{
		printlog ("Seeking....");
//		_lseeki64 ( _fileno (m_Fin), (int)(((double)i64StartFrame)*fileinfo->dBytesperframe), SEEK_SET);
		fseek ( m_Fin, (long)(((double)i64StartFrame)*fileinfo->dBytesperframe), SEEK_SET);
		printlog ("Done.");
		printlog ("Processing....");
	}
	if (i64EndFrame == (fileinfo->i64TotalFrames-1)) i64TotalFrames=i64EndFrame=( (__int64)1<<60);

	bEndOfFile=bTooManyErrors=false;
	bCRCError=false;
	iBytesPerFramePrev=iBytesPerFramen=0;
	i64nubytes=0;
	for (i64=i64StartFrame; i64< i64EndFrame+1 && m_bAbort==false && bEndOfFile==false ;i64++)
	{ 
		if (m_bCLI == false) 
			UpdateProgress( pWnd, (int)(((i64-i64StartFrame)*100)/i64nuframes));
		else
		{
			csAux.Format(_T("Processing %02d %%"),((int)((i64*100)/i64nuframes)) );
			printline(csAux);
		}

        if (i64 < 0 || i64 >= i64TotalFrames)
			f_writeframe=WF_SILENCE;
		else 
		{
/////////////////////////////////////////////////////////
// Read & write frame by frame 
//	 If EndFrames<0 stop before the end
/////////////////////////////////////////////////////////	
			iBytesPerFramePrev=(int)i64nubytes;

			f_writeframe=WF_WRITE; //indicates write frame
			i64nubytes=readac3frame (m_Fin, caracter);
			if( i64nubytes <5)
			{
				bEndOfFile=true;
				break;
			}
//			iFrmsizecodn=caracter[4]& 0x3F;
			iFrmsizecodn=caracter[4];
			iBytesPerFramen=FrameSize_ac3[iFrmsizecodn]*2;
			csTime.Format (_T("%02d:%02d:%02d.%03d"), (int)(dFrameduration*i64/3600000),
                                               ((( (int)(dFrameduration*i64) )%3600000)/60000),
                                               ((( (int)(dFrameduration*i64) )%60000)/1000),
                                               ((  (int)(dFrameduration*i64) )%1000) );

			if ( caracter[0]!= 0x0B || caracter[1]!=0x77)
			{
				nuerrors++;
				csAux.Format (_T("Time %s; Frame#= %I64d. Unsynchronized frame..."),
			               csTime, i64+1);
// Try to find the next sync Word and continue...
			// rewind nubytes (last frame), and if previous had error, 
				if (!bCRCError)
				{
					n64skip=0;
					fseek(m_Fin, (long)(-1*i64nubytes), SEEK_CUR);
				}
				else
				{	
					n64skip= -1*iBytesPerFramePrev+2;
					fseek(m_Fin, (long)(-1*(i64nubytes+iBytesPerFramePrev-2)), SEEK_CUR);
				}

				caracter[0]=fgetc(m_Fin);
				caracter[1]=fgetc(m_Fin);
				while ((caracter[0]!= 0x0B || caracter[1]!=0x77 ) && !feof(m_Fin) )
				{
 					caracter[0]= caracter[1];
 					caracter[1]= fgetc(m_Fin);
					n64skip++;
				}
				if (caracter[0]== 0x0B && caracter[1]==0x77) 
				{
					if (n64skip>0)
						csAux1.Format(_T("SKIPPED  %I64d bytes. Found new synch word"),n64skip);
					else
						csAux1.Format(_T("REWINDED %I64d bytes. Found new synch word"),-1*n64skip);
									
					csAux+=csAux1;
					printlog(csAux);
			// rewind 2 bytes
					fseek(m_Fin, -2L, SEEK_CUR);
					f_writeframe=WF_SKIP; // do not write this frame
				}
				else // nothimg to do, reached end of file
				{
					csAux+="NOT FIXED. Reached end of file ";
					printlog(csAux);
					f_writeframe=WF_SKIP; // do not write this frame
					bEndOfFile=true;
				}
			}
			else  if (i64nubytes < 12 || i64nubytes != iBytesPerFramen)
			{
				nuerrors++;
				f_writeframe=WF_SKIP; // do not write this frame
				csAux.Format (_T("Time %s; Frame#= %I64d. Uncomplete frame...SKIPPED"),
		               csTime, i64+1);
				printlog(csAux);
			}
			else
			{
		
// Some consistence checks
				syncwordn=   getbits (16, caracter);
				crc1n=       getbits (16, caracter);
				fscodn=      getbits (2, caracter);
				iFrmsizecodn= getbits (6, caracter);
				bsidn=       getbits (5, caracter);
				bsmodn=      getbits (3, caracter);
				acmodn=      getbits (3, caracter);
				if ((iFrmsizecodn >> 1) != (fileinfo->frmsizecod >>1) ||
					fscodn != fileinfo->fscod ||
					bsmodn != fileinfo->bsmod || acmodn != fileinfo->acmod)
				{
					fileinfo->fscod=fscodn;
					fileinfo->frmsizecod= iFrmsizecodn;
					fileinfo->bsmod=bsmodn;
					fileinfo->acmod= acmodn;

		            nuerrors++;
					csAux.Format (_T("Time %s; Frame#= %I64d. Some basic parameters changed between Frame #%I64d and this frame"),
				            csTime, i64+1, fileinfo->i64frameinfo);
					printlog(csAux);
					fileinfo->i64frameinfo=i64+1;
				}
			
// CRC calculation and fixing.

//				frame_size=  FrameSize_48[iFrmsizecodn];
				frame_size=  FrameSize_ac3[iFrmsizecodn+64*fscodn];
				frame_size_58 = (frame_size >> 1) + (frame_size >> 3);

				cal_crc1 = ac3_crc(caracter + 4, (2 * frame_size_58) - 4, 0);
  	      		crc_inv  = pow_poly((CRC16_POLY >> 1), (16 * frame_size_58) - 16, CRC16_POLY);
		      	cal_crc1 = mul_poly(crc_inv, cal_crc1, CRC16_POLY);
		 		cal_crc2 = ac3_crc(caracter + 2 * frame_size_58, (frame_size - frame_size_58) * 2 - 2, 0);
	
				bCRCError=false;
				if (caracter[2]!=(cal_crc1 >> 8) ||
		    		caracter[3]!=(cal_crc1 & 0xff) )
				{
					bCRCError=true;
					nuerrors++;
					{
						csAux.Format (_T("Time %s; Frame#= %I64d.  Crc1 error %s: read = %02X%02X; calculated=%02X%02X"),
				               csTime, i64+1, m_csFixcrc,
								caracter[2], caracter[3], cal_crc1 >> 8, cal_crc1 & 0xff );
						printlog(csAux);
					}
					if (m_iCrc==CRC_FIX)
					{
						caracter[2]=(cal_crc1 >> 8);
						caracter[3]=cal_crc1 & 0xff;
					}
					else if (m_iCrc==CRC_SKIP) f_writeframe=WF_SKIP;
					else if (m_iCrc==CRC_SILENCE) f_writeframe=WF_SILENCE;
				}

				if (caracter[2*frame_size - 2]!=(cal_crc2 >> 8) ||
					caracter[2*frame_size - 1]!=(cal_crc2 & 0xff) )
				{
					bCRCError=true;
					nuerrors++;
					{
						csAux.Format (_T("Time %s; Frame#= %I64d.  Crc2 error %s: read = %02X%02X; calculated=%02X%02X"),
				            csTime, i64+1, m_csFixcrc,
							caracter[2*frame_size - 2],caracter[2*frame_size - 1],
							cal_crc2 >> 8, cal_crc2 & 0xff );
						printlog(csAux);
					}
					if (m_iCrc==CRC_FIX)
					{
						caracter[2*frame_size - 2]=(cal_crc2 >> 8);
						caracter[2*frame_size - 1]=cal_crc2 & 0xff;
					}
					else if (m_iCrc==CRC_SKIP) f_writeframe=WF_SKIP;
					else if (m_iCrc==CRC_SILENCE) f_writeframe=WF_SILENCE;
				}

			}
		}


//	Write frame
		if (f_writeframe==WF_WRITE)
		{
			i64frameswritten++;
			writeac3frame (m_Fout, caracter);
		}
		else if (f_writeframe==WF_SILENCE)
		{
			i64frameswritten++;
			writeac3frame (m_Fout, p_silence);
		}

		if (!bTooManyErrors && nuerrors > MAXLOGERRORS)
		{
			printlog("Too Many Errors. Stop Logging.");
			bTooManyErrors=true;
		}

	}
	bTooManyErrors=false;
	csAux.Format(_T("Number of written frames = %I64d"), i64frameswritten);
	printlog(csAux);
	csAux.Format(_T("Number of Errors= %d"), nuerrors);
	printlog(csAux);
	if (m_bCLI == false) UpdateProgress( pWnd, (int)(100));

	return 0;
}

int CDelaycutApp::delayeac3(CWnd* pWnd)
{
	__int64 i64;
	__int64 i64StartFrame, i64EndFrame, i64nuframes, i64frameswritten,i64TotalFrames;
	FILEINFO *fileinfo;
	__int64  i64nubytes;
	double dFrameduration;
	int iBytesPerFramen,iBytesPerFramePrev;
	UINT syncwordn,	fscodn, frmsizen, acmodn;  
	CString csTime,csAux,csAux1;
	int f_writeframe,nuerrors;
	unsigned int cal_crc1;
	__int64 n64skip;
	bool bEndOfFile;
	bool bCRCError;

	unsigned char caracter[MAXFRAMESIZE];


	// init ac3_crc
//	ac3_crc_init(); Done in Initinstance

	fileinfo=&theApp.m_myinfo;

	i64StartFrame=fileinfo->i64StartFrame;
	i64EndFrame=fileinfo->i64EndFrame;
	i64nuframes=i64EndFrame-i64StartFrame+1;
	dFrameduration=fileinfo->dFrameduration;
	i64TotalFrames = fileinfo->i64TotalFrames;

	i64frameswritten=0;
	nuerrors=0;

	printlog ("====== PROCESSING LOG ======================");
	if (i64StartFrame > 0 && m_bAbort==false )
	{
		printlog ("Seeking....");
//		_lseeki64 ( _fileno (m_Fin), (int)(((double)i64StartFrame)*fileinfo->dBytesperframe), SEEK_SET);
		fseek ( m_Fin, (long)(((double)i64StartFrame)*fileinfo->dBytesperframe), SEEK_SET);
		printlog ("Done.");
		printlog ("Processing....");
	}
	if (i64EndFrame == (fileinfo->i64TotalFrames-1)) i64TotalFrames=i64EndFrame=( (__int64)1<<60);

	bEndOfFile=bTooManyErrors=false;
	bCRCError=false;
	iBytesPerFramePrev=iBytesPerFramen=0;
	i64nubytes=0;
	for (i64=i64StartFrame; i64< i64EndFrame+1 && m_bAbort==false && bEndOfFile==false ;i64++)
	{ 
		if (m_bCLI == false) 
			UpdateProgress( pWnd, (int)(((i64-i64StartFrame)*100)/i64nuframes));
		else
		{
			csAux.Format(_T("Processing %02d %%"),((int)((i64*100)/i64nuframes)) );
			printline(csAux);
		}

        if (i64 < 0 || i64 >= i64TotalFrames)
			f_writeframe=WF_SILENCE;
		else 
		{
/////////////////////////////////////////////////////////
// Read & write frame by frame 
//	 If EndFrames<0 stop before the end
/////////////////////////////////////////////////////////	
			iBytesPerFramePrev=(int)i64nubytes;

			f_writeframe=WF_WRITE; //indicates write frame
			i64nubytes=readeac3frame (m_Fin, caracter);
			if( i64nubytes <5)
			{
				bEndOfFile=true;
				break;
			}
   iBytesPerFramen=(((caracter[2] & 7) << 8) + caracter[3] + 1) * 2;
			csTime.Format (_T("%02d:%02d:%02d.%03d"), (int)(dFrameduration*i64/3600000),
                                               ((( (int)(dFrameduration*i64) )%3600000)/60000),
                                               ((( (int)(dFrameduration*i64) )%60000)/1000),
                                               ((  (int)(dFrameduration*i64) )%1000) );

			if ( caracter[0]!= 0x0B || caracter[1]!=0x77)
			{
				nuerrors++;
				csAux.Format (_T("Time %s; Frame#= %I64d. Unsynchronized frame..."),
			               csTime, i64+1);
// Try to find the next sync Word and continue...
			// rewind nubytes (last frame), and if previous had error, 
				if (!bCRCError)
				{
					n64skip=0;
					fseek(m_Fin, (long)(-1*i64nubytes), SEEK_CUR);
				}
				else
				{	
					n64skip= -1*iBytesPerFramePrev+2;
					fseek(m_Fin, (long)(-1*(i64nubytes+iBytesPerFramePrev-2)), SEEK_CUR);
				}

				caracter[0]=fgetc(m_Fin);
				caracter[1]=fgetc(m_Fin);
				while ((caracter[0]!= 0x0B || caracter[1]!=0x77 ) && !feof(m_Fin) )
				{
 					caracter[0]= caracter[1];
 					caracter[1]= fgetc(m_Fin);
					n64skip++;
				}
				if (caracter[0]== 0x0B && caracter[1]==0x77) 
				{
					if (n64skip>0)
						csAux1.Format(_T("SKIPPED  %I64d bytes. Found new synch word"),n64skip);
					else
						csAux1.Format(_T("REWINDED %I64d bytes. Found new synch word"),-1*n64skip);
									
					csAux+=csAux1;
					printlog(csAux);
			// rewind 2 bytes
					fseek(m_Fin, -2L, SEEK_CUR);
					f_writeframe=WF_SKIP; // do not write this frame
				}
				else // nothimg to do, reached end of file
				{
					csAux+="NOT FIXED. Reached end of file ";
					printlog(csAux);
					f_writeframe=WF_SKIP; // do not write this frame
					bEndOfFile=true;
				}
			}
			else  if (i64nubytes < 12 || i64nubytes != iBytesPerFramen)
			{
				nuerrors++;
				f_writeframe=WF_SKIP; // do not write this frame
				csAux.Format (_T("Time %s; Frame#= %I64d. Uncomplete frame...SKIPPED"),
		               csTime, i64+1);
				printlog(csAux);
			}
			else
			{
		
// Some consistence checks
   	syncwordn=  getbits (16, caracter);
                getbits (2,  caracter);
                getbits (3,  caracter);
    frmsizen=   getbits (11, caracter);
 	  fscodn=     getbits (2,  caracter);
 	              getbits (2,  caracter);
 	  acmodn=     getbits (3,  caracter);

				if ((frmsizen*2+2 != fileinfo->dBytesperframe) ||
					fscodn != fileinfo->fscod ||
					acmodn != fileinfo->acmod)
				{
					fileinfo->fscod=fscodn;
					fileinfo->dBytesperframe= frmsizen*2+2;
					fileinfo->acmod= acmodn;

		            nuerrors++;
					csAux.Format (_T("Time %s; Frame#= %I64d. Some basic parameters changed between Frame #%I64d and this frame"),
				            csTime, i64+1, fileinfo->i64frameinfo);
					printlog(csAux);
					fileinfo->i64frameinfo=i64+1;
				}
			
// CRC calculation and fixing.

				cal_crc1 = ac3_crc(caracter + 2, frmsizen*2+2 - 4, 0);
	
				bCRCError=false;
				if (caracter[frmsizen*2+2-2]!=(cal_crc1 >> 8) ||
		    		caracter[frmsizen*2+2-1]!=(cal_crc1 & 0xff) )
				{
					bCRCError=true;
					nuerrors++;
					{
						csAux.Format (_T("Time %s; Frame#= %I64d.  Crc error %s: read = %02X%02X; calculated=%02X%02X"),
				               csTime, i64+1, m_csFixcrc,
								caracter[frmsizen*2+2-2], caracter[frmsizen*2+2-1], cal_crc1 >> 8, cal_crc1 & 0xff );
						printlog(csAux);
					}
					if (m_iCrc==CRC_FIX)
					{
						caracter[frmsizen*2+2-2]=(cal_crc1 >> 8);
						caracter[frmsizen*2+2-1]=cal_crc1 & 0xff;
					}
					else if (m_iCrc==CRC_SKIP) f_writeframe=WF_SKIP;
					else if (m_iCrc==CRC_SILENCE) f_writeframe=WF_SILENCE;
				}
			}
		}


//	Write frame
		if (f_writeframe==WF_WRITE)
		{
			i64frameswritten++;
			writeeac3frame (m_Fout, caracter);
		}
		else if (f_writeframe==WF_SILENCE)
		{
			i64frameswritten++;
			writeeac3frame (m_Fout, p_silence);
		}

		if (!bTooManyErrors && nuerrors > MAXLOGERRORS)
		{
			printlog("Too Many Errors. Stop Logging.");
			bTooManyErrors=true;
		}

	}
	bTooManyErrors=false;
	csAux.Format(_T("Number of written frames = %I64d"), i64frameswritten);
	printlog(csAux);
	csAux.Format(_T("Number of Errors= %d"), nuerrors);
	printlog(csAux);
	if (m_bCLI == false) UpdateProgress( pWnd, (int)(100));

	return 0;
}


int CDelaycutApp::delaydts(CWnd* pWnd)
{

	__int64 i64, i64StartFrame, i64EndFrame, i64nuframes, i64frameswritten, i64TotalFrames;
	double dFrameduration;
	FILEINFO *fileinfo;
	__int64  i64nubytes;
	int iBytesPerFramen,iBytesPerFramePrev;
	CString csTime,csAux,csAux1;
	int f_writeframe,nuerrors;
	UINT unused, amode,sfreq;
	int rate;
	__int64 n64skip;
	bool bEndOfFile;


	unsigned char caracter[MAXFRAMESIZE];

//	ac3_crc_init(); Done in Initinstance

	fileinfo=&theApp.m_myinfo;

	i64StartFrame=fileinfo->i64StartFrame;
	i64EndFrame=fileinfo->i64EndFrame;
	i64nuframes=i64EndFrame-i64StartFrame+1;
	dFrameduration=fileinfo->dFrameduration;
	i64TotalFrames = fileinfo->i64TotalFrames;

	i64frameswritten=0;
	nuerrors=0;

	printlog ("====== PROCESSING LOG ======================");

	if (i64StartFrame > 0 && m_bAbort==false )
	{
		printlog ("Seeking....");
//		_lseeki64 ( _fileno (m_Fin), (int)(((double)i64StartFrame)*fileinfo->dBytesperframe), SEEK_SET);
		fseek ( m_Fin, (long)(((double)i64StartFrame)*fileinfo->dBytesperframe), SEEK_SET);
		printlog ("Done.");
		printlog ("Processing....");
	}
	if (i64EndFrame == (fileinfo->i64TotalFrames-1)) i64TotalFrames=i64EndFrame=( (__int64)1<<60);
	bEndOfFile=bTooManyErrors=false;

	iBytesPerFramePrev=iBytesPerFramen=0;
	i64nubytes=0;

	for (i64=i64StartFrame; i64< i64EndFrame+1 && m_bAbort==false && bEndOfFile==false ;i64++)
	{ 
		if (m_bCLI == false) 
			UpdateProgress( pWnd, (int)(((i64-i64StartFrame)*100)/i64nuframes));
		else
		{
			csAux.Format(_T("Processing %02d %%"),((int)((i64*100)/i64nuframes)) );
			printline(csAux);
		}

        if (i64 < 0 || i64 >= i64TotalFrames)
				f_writeframe=WF_SILENCE;
		else 
		{
			iBytesPerFramePrev=(int)i64nubytes;
			f_writeframe=WF_WRITE; //indicates write frame
			i64nubytes=readdtsframe (m_Fin, caracter);
			if( i64nubytes <5)
			{
				bEndOfFile=true;
				break;
			}
			iBytesPerFramen=((caracter[5]&0x3)*256 + caracter[6])*16 + (caracter[7]&0xF0)/16 +1;

			csTime.Format (_T("%02d:%02d:%02d.%03d"), (int)(dFrameduration*i64/3600000),
                                               ((( (int)(dFrameduration*i64) )%3600000)/60000),
                                               ((( (int)(dFrameduration*i64) )%60000)/1000),
                                               ((  (int)(dFrameduration*i64) )%1000) );

			if (caracter[0] != 0x7F || caracter[1] != 0xFE || 
					caracter[2] != 0x80 || caracter[3] != 0x01)
			{
				nuerrors++;
				csAux.Format (_T("Time %s; Frame#= %I64d. Unsynchronized frame..."),
				               csTime, i64+1);
// Try to find the next sync Word and continue...
			// rewind nubytes (last frame) + previous frame...
				n64skip= -1*iBytesPerFramePrev+4;
				fseek(m_Fin, (long)(-1*(i64nubytes+iBytesPerFramePrev-4)), SEEK_CUR);

				caracter[0]=fgetc(m_Fin);
				caracter[1]=fgetc(m_Fin);
				caracter[2]=fgetc(m_Fin);
				caracter[3]=fgetc(m_Fin);
				while ((caracter[0] != 0x7F || caracter[1] != 0xFE || 
					caracter[2] != 0x80 || caracter[3] != 0x01)  && !feof(m_Fin) )
				{
 					caracter[0]= caracter[1];
 					caracter[1]= caracter[2];
 					caracter[2]= caracter[3];
 					caracter[3]= fgetc(m_Fin);
					n64skip++;
				}
				if (caracter[0] == 0x7F && caracter[1] == 0xFE && 
					caracter[2] == 0x80 && caracter[3] == 0x01)
				{
					if (n64skip>0)
						csAux1.Format(_T("SKIPPED  %I64d bytes. Found new synch word"),n64skip);
					else
						csAux1.Format(_T("REWINDED %I64d bytes. Found new synch word"),-1*n64skip);
					csAux+=csAux1;
					printlog(csAux);

			// rewind 4 bytes
					fseek(m_Fin, -4L, SEEK_CUR);
					f_writeframe=WF_SKIP; // do not write this frame
				}
				else // nothimg to do, reached end of file
				{
					csAux+="NOT FIXED. Reached end of file ";
					printlog(csAux);
					f_writeframe=WF_SKIP; // do not write this frame
					bEndOfFile=true;
				}
			}
			else  if (i64nubytes < 12 || i64nubytes != iBytesPerFramen)
			{
				nuerrors++;
				f_writeframe=WF_SKIP; // do not write this frame
				csAux.Format (_T("Time %s; Frame#= %I64d. Uncomplete frame...SKIPPED"),
				               csTime, i64+1);
				printlog(csAux);
			}
			else
			{
		
// Some consistence checks


/*				syncword=   getbits (32, caracter);
				ftype=      getbits (1, caracter);
				fshort=     getbits (5, caracter);
				cpf=        getbits (1, caracter);
				nblks=      getbits (7, caracter);
				fsize=      getbits (14,caracter);
*/
				unused=		getbits (32, caracter);
				unused=     getbits (1, caracter);
				unused=     getbits (5, caracter);
				unused=     getbits (1, caracter);
				unused=     getbits (7, caracter);
				unused=     getbits (14,caracter);
				amode=      getbits (6, caracter);
				sfreq=      getbits (4, caracter);
				rate=       getbits (5, caracter);
				rate= dtsbitrate[rate];

				if (sfreq != fileinfo->fscod ||	amode != fileinfo->acmod  || rate != fileinfo->iBitrate )
				{

						fileinfo->fscod=sfreq;
						fileinfo->acmod=amode;
						fileinfo->iBitrate=rate;

		              	nuerrors++;
						csAux.Format (_T("Time %s; Frame#= %I64d. Some basic parameters changed between Frame #%I64d and this frame"),
					               csTime, i64+1,fileinfo->i64frameinfo);
						printlog(csAux);

						fileinfo->i64frameinfo=i64+1;

				}
			}
		}

//	Write frame
		if (f_writeframe==WF_WRITE)
		{
			i64frameswritten++;
			writedtsframe (m_Fout, caracter);
		}
		else if (f_writeframe==WF_SILENCE)
		{
			i64frameswritten++;
			writedtsframe (m_Fout, p_silence);
		}
		if (!bTooManyErrors && nuerrors > MAXLOGERRORS)
		{
			printlog("Too Many Errors. Stop Logging.");
			bTooManyErrors=true;
		}

	}
	bTooManyErrors=false;
	csAux.Format(_T("Number of written frames = %I64d"), i64frameswritten);
	printlog(csAux);
	csAux.Format(_T("Number of Errors= %d"), nuerrors);
	printlog(csAux);
	if (m_bCLI == false) UpdateProgress( pWnd, (int)(100));

	return 0;
}


int CDelaycutApp::delaympa(CWnd* pWnd)
{

	__int64 i64, i64StartFrame, i64EndFrame, i64nuframes, i64frameswritten, i64TotalFrames;
	double dFrameduration;
	FILEINFO *fileinfo;
	__int64  i64nubytes;
	int iBytesPerFramen,iBytesPerFramePrev;
	CString csTime,csAux,csAux1;
	int f_writeframe,nuerrors;
	__int64 n64skip;
	int	syncword,iID,layer,protection_bit,rate,fsamp,unused;
	unsigned int crc_check, crc_cal1;
	bool bEndOfFile;
	unsigned char caracter[MAXFRAMESIZE];
	int j,i;
	int irate;
	int padding_bit,private_bit,mode;
	int nbits;
	int *layerIIsub;
	int ratech;

//	ac3_crc_init(); Done in Initinstance

	fileinfo=&theApp.m_myinfo;

	i64StartFrame=fileinfo->i64StartFrame;
	i64EndFrame=fileinfo->i64EndFrame;
	i64nuframes=i64EndFrame-i64StartFrame+1;
	dFrameduration=fileinfo->dFrameduration;
	i64TotalFrames = fileinfo->i64TotalFrames;

	i64frameswritten=0;
	nuerrors=0;

	printlog ("====== PROCESSING LOG ======================");

	if (i64StartFrame > 0 && m_bAbort==false )
	{
		printlog ("Seeking....");
//		_lseeki64 ( _fileno (m_Fin), (int)(((double)i64StartFrame)*fileinfo->dBytesperframe), SEEK_SET);
		fseek ( m_Fin, (long)(((double)i64StartFrame)*fileinfo->dBytesperframe), SEEK_SET);
		printlog ("Done.");
		printlog ("Processing....");
	}

	if (i64EndFrame == (fileinfo->i64TotalFrames-1)) i64TotalFrames=i64EndFrame=( (__int64)1<<60);
	bEndOfFile=bTooManyErrors=false;
	iBytesPerFramePrev=iBytesPerFramen=0;
	i64nubytes=0;

	for (i64=i64StartFrame; i64< i64EndFrame+1 && m_bAbort==false && bEndOfFile==false;i64++)
	{ 
		if (m_bCLI == false) 
			UpdateProgress( pWnd, (int)(((i64-i64StartFrame)*100)/i64nuframes));
		else
		{
			csAux.Format(_T("Processing %02d %%"),((int)((i64*100)/i64nuframes)) );
			printline(csAux);
		}

        if (i64 < 0 || i64 >= i64TotalFrames)
				f_writeframe=WF_SILENCE;
		else 
		{
/////////////////////////////////////////////////////////
// Read & write frame by frame 
//	 If EndFrames<0 stop before the end
/////////////////////////////////////////////////////////	
			f_writeframe=WF_WRITE; //indicates write frame

			iBytesPerFramePrev=(int)i64nubytes;

			i64nubytes=readmpaframe (m_Fin, caracter);
			if( i64nubytes <4)
			{
				bEndOfFile=true;
				break;
			}
			iBytesPerFramen=(int)i64nubytes;

			csTime.Format (_T("%02d:%02d:%02d.%03d"), (int)(dFrameduration*i64/3600000),
                                               ((( (int)(dFrameduration*i64) )%3600000)/60000),
                                               ((( (int)(dFrameduration*i64) )%60000)/1000),
                                               ((  (int)(dFrameduration*i64) )%1000) );

			if (caracter[0] != 0xFF || (caracter[1] & 0xF0 )!= 0xF0 )

			{
				nuerrors++;

				n64skip= -1*iBytesPerFramePrev+2;
				fseek(m_Fin, (long)(-1*(i64nubytes+iBytesPerFramePrev-2)), SEEK_CUR);

				csAux.Format (_T("Time %s; Frame#= %I64d. Unsynchronized frame..."),
				               csTime, i64+1);
// Try to find the next sync Word and continue...
			// rewind nubytes (last frame)

				fseek(m_Fin, (long)(-1*i64nubytes), SEEK_CUR);

				caracter[0]=fgetc(m_Fin);
				caracter[1]=fgetc(m_Fin);
				while ((caracter[0] != 0xFF || (caracter[1] & 0xF0 )!= 0xF0 ) && !feof(m_Fin) )
				{
 					caracter[0]= caracter[1];
 					caracter[1]= fgetc(m_Fin);
					n64skip++;
				}
				if (caracter[0] == 0xFF && (caracter[1] & 0xF0 )== 0xF0 )
				{
					if (n64skip>0)
						csAux1.Format(_T("SKIPPED  %I64d bytes. Found new synch word"),n64skip);
					else
						csAux1.Format(_T("REWINDED %I64d bytes. Found new synch word"),-1*n64skip);
					csAux+=csAux1;
					printlog(csAux);

			// rewind 4 bytes
					fseek(m_Fin, -2L, SEEK_CUR);
					f_writeframe=WF_SKIP; // do not write this frame
				}
				else // nothimg to do, reached end of file
				{
					csAux+="NOT FIXED. Reached end of file ";
					printlog(csAux);
					f_writeframe=WF_SKIP; // do not write this frame
					bEndOfFile=true;
				}
			}
			else  if (i64nubytes < 12 || i64nubytes != iBytesPerFramen)
			{
				nuerrors++;
				f_writeframe=WF_SKIP; // do not write this frame
				csAux.Format (_T("Time %s; Frame#= %I64d. Uncomplete frame...SKIPPED"),
				               csTime, i64+1);
				printlog(csAux);
			}
			else
			{
		
// Some consistence checks
//	nubytes=readmpaframe (in, caracter);

				syncword=			getbits (12, caracter);
				iID=				getbits (1, caracter);
				layer=				getbits (2, caracter);
				protection_bit=		getbits (1, caracter);
				rate=				getbits (4, caracter);
				irate=rate;
				fsamp=				getbits (2,caracter);

				padding_bit=		getbits (1, caracter);
				private_bit=		getbits (1, caracter);
				mode=				getbits (2, caracter);
/*				mode_extension=		getbits (2, caracter);
				copyright=			getbits (1, caracter);
				original=			getbits (1, caracter);
				emphasis=			getbits (2, caracter);
*/

				unused=getbits (6, caracter);

				if (protection_bit==0)
					crc_check=		getbits (16, caracter);

				if		(layer==1) layer=3;
				else if (layer==3) layer=1;

				if (layer==3)
					rate=layerIIIrate[rate];
				else if (layer==2)
					rate=layerIIrate[rate];
				else if (layer==1)
					rate=layerIrate[rate];
				else rate=0;

				if (fsamp==0)	fsamp=44100;
				else if (fsamp==1)	fsamp=48000;
				else if (fsamp==2)	fsamp=32000;
				else fsamp=44100;

				if (fsamp != fileinfo->fsample || layer != fileinfo->layer  || rate != fileinfo->iBitrate )
				{

					fileinfo->fsample=fsamp;
					fileinfo->layer=layer;
					fileinfo->iBitrate=rate;

				   	nuerrors++;
					csAux.Format (_T("Time %s; Frame#= %I64d. Some basic parameters changed between Frame #%I64d and this frame"),
						csTime, i64+1,fileinfo->i64frameinfo);
					printlog(csAux);
						fileinfo->i64frameinfo=i64+1;
				}

// CRC calculation and fixing.
				if (layer==2 && protection_bit==0)
				{
					if (mode==3) ratech=rate;
					else ratech=rate/2;

					if (fsamp==44100)
					{
						if (ratech <=48) {layerIIsub=layerIIsub2c; nbits=42;}
						else if (ratech<=80) {layerIIsub=layerIIsub2a; nbits=142;}
							else {layerIIsub=layerIIsub2b; nbits=154;}
					}
					else if (fsamp==32000)
					{
						if (ratech <=48) {layerIIsub=layerIIsub2d; nbits=62;}
						else if (ratech<=80) {layerIIsub=layerIIsub2a;nbits=142;}
						else {layerIIsub=layerIIsub2b;nbits=154;}
					}
					else
					{
						if (ratech <=48) {layerIIsub=layerIIsub2c; nbits=42;}
						else { layerIIsub=layerIIsub2a; nbits=142;}
					}
					if (mode != 3) nbits*=2;

//	nbits has now the maximum  # of bits to be protected,
//  but the actual number could be lower, different per frame
					for (i=0;i<layerIIsub[0];i++)
					{
						unused=	getbits (4, caracter);
						if (unused==0) nbits-=2;
						if (mode != 3)
						{
							unused=	getbits (4, caracter);
							if (unused==0) nbits-=2;
						}
					}
					for (i=0;i<layerIIsub[1];i++)
					{
						unused=	getbits (3, caracter);
						if (unused==0) nbits-=2;
						if (mode != 3)
						{
							unused=	getbits (3, caracter);
							if (unused==0) nbits-=2;
						}
					}
					for (i=0;i<layerIIsub[2];i++)
					{
						unused=	getbits (2, caracter);
						if (unused==0) nbits-=2;
						if (mode != 3)
						{
							unused=	getbits (2, caracter);
							if (unused==0) nbits-=2;
						}
					}

					crc_cal1 = ac3_crc(caracter + 2, 2 , 0xffff);
//					crc_cal1 = ac3_crc(caracter + 6, (int)(i64nubytes-6) , crc_cal1);
					if (6+nbits/8 <= (int)(i64nubytes))
					{
						crc_cal1 = ac3_crc(caracter + 6, nbits/8 , crc_cal1);
						for (j=0;j<(nbits%8);j++)
						{
							crc_cal1 = ac3_crc_bit( caracter+6+nbits/8, 7-j , crc_cal1);
						}
//						csAux.Format(_T("read=%04X ;cal=%04X"),crc_check,crc_cal1);
//						if (AfxMessageBox(csAux,MB_RETRYCANCEL ,0) ==IDCANCEL)
//							return 0;
					}

/*
					for (i=0; i<(int)(i64nubytes);i++)
					{
						for (j=0;j<8;j++)
						{
							crc_cal1 = ac3_crc_bit( caracter+6+i, 7-j , crc_cal1);
							if (crc_cal1==crc_check)
							{
								csAux.Format(_T("Found i=%d; j=%d; nbits=%d; nbitsc=%d"),i,j,i*8+j+1,nbits);
								if (AfxMessageBox(csAux,MB_RETRYCANCEL ,0) ==IDCANCEL)
									return 0;
								i=(int)i64nubytes;
								j=10;
							}
						}
					}
*/
					if (crc_cal1 !=crc_check)
					{
						nuerrors++;
						{
							csAux.Format (_T("Time %s; Frame#= %I64d.  Crc error %s: read = %04X; calculated=%04X"),
				               csTime, i64+1, m_csFixcrc,
								crc_check, crc_cal1 );
							printlog(csAux);
						}
						if (m_iCrc==CRC_FIX)
						{
							caracter[4]=(crc_cal1 >> 8);
							caracter[5]=crc_cal1 & 0xff;
						}
						else if (m_iCrc==CRC_SKIP) f_writeframe=WF_SKIP;
						else if (m_iCrc==CRC_SILENCE) f_writeframe=WF_SILENCE;
					}
				}
			}
		}

//	Write frame
		if (f_writeframe==WF_WRITE)
		{
			i64frameswritten++;
			writempaframe (m_Fout, caracter);
		}
		else if (f_writeframe==WF_SILENCE)
		{
			i64frameswritten++;
			writempaframe (m_Fout, p_silence);
		}
		if (!bTooManyErrors && nuerrors > MAXLOGERRORS)
		{
			printlog("Too Many Errors. Stop Logging.");
			bTooManyErrors=true;
		}

	}
	bTooManyErrors=false;
	csAux.Format(_T("Number of written frames = %I64d"), i64frameswritten);
	printlog(csAux);
	csAux.Format(_T("Number of Errors= %d"), nuerrors);
	printlog(csAux);
	if (m_bCLI == false) UpdateProgress( pWnd, (int)(100));

	return 0;
}



int CDelaycutApp::delaywav(CWnd* pWnd)
{

	__int64 i64, i64StartFrame, i64EndFrame, i64nuframes, i64frameswritten, i64TotalFrames;
	double dFrameduration;
	FILEINFO *fileinfo;
	__int64  i64nubytes;
	int iBytesPerFrame;
	int iInidata;
	CString csAux;
	int f_writeframe,nuerrors;
	__int64 i64Aux;
	bool bEndOfFile;
	unsigned char caracter[MAXFRAMESIZE];


	fileinfo=&theApp.m_myinfo;

	iInidata=fileinfo->iInidata;
	i64StartFrame=fileinfo->i64StartFrame;
	i64EndFrame=fileinfo->i64EndFrame;
	i64nuframes=i64EndFrame-i64StartFrame+1;
	dFrameduration=fileinfo->dFrameduration;
	i64TotalFrames = fileinfo->i64TotalFrames;
	iBytesPerFrame= (int)fileinfo->dBytesperframe;

	i64frameswritten=0;
	nuerrors=0;

	printlog ("====== PROCESSING LOG ======================");

// Write wav header
	readwavsample (m_Fin, caracter,iInidata);

/*	i64Aux=i64nuframes*iBytesPerFrame+iInidata-8;
	caracter[4]=(unsigned char)(i64Aux%256);
	caracter[5]=(unsigned char)((i64Aux >> 8)%256);
	caracter[6]=(unsigned char)((i64Aux >>16)%256);
	caracter[7]=(unsigned char)((i64Aux >>24)%256);

	i64Aux=i64nuframes*iBytesPerFrame;
	caracter[iInidata-4]=(unsigned char)(i64Aux%256);
	caracter[iInidata-3]=(unsigned char)((i64Aux >> 8)%256);
	caracter[iInidata-2]=(unsigned char)((i64Aux >>16)%256);
	caracter[iInidata-1]=(unsigned char)((i64Aux >>24)%256);
*/	
	writewavsample (m_Fout, caracter,iInidata);

	if (i64StartFrame > 0 && m_bAbort==false )
	{
		printlog ("Seeking....");
//		i64Aux=_lseeki64 ( _fileno (m_Fin), i64StartFrame*iBytesPerFrame + iInidata, SEEK_SET);
		fseek ( m_Fin,(long)(i64StartFrame*iBytesPerFrame + iInidata), SEEK_SET);
		printlog ("Done.");
		printlog ("Processing....");
	}
	if (i64EndFrame == (fileinfo->i64TotalFrames-1)) i64TotalFrames=i64EndFrame=( (__int64)1<<60);
	bEndOfFile=bTooManyErrors=false;
	for (i64=i64StartFrame; i64< i64EndFrame+1 && m_bAbort==false && bEndOfFile==false ;i64++)
	{ 
		if (m_bCLI == false)
		{
			if ((i64%10)==0) 
				UpdateProgress( pWnd, (int)(((i64-i64StartFrame)*100)/i64nuframes));
		}
		else
		{
			csAux.Format(_T("Processing %02d %%"),((int)((i64*100)/i64nuframes)) );
			printline(csAux);
		}

        if (i64 < 0 || i64 >= i64TotalFrames)
			f_writeframe=WF_SILENCE;
		else 
		{
			f_writeframe=WF_WRITE; //indicates write frame
			i64nubytes=readwavsample (m_Fin, caracter,iBytesPerFrame);
			if( i64nubytes <1)
			{
				bEndOfFile=true;
				break;
			}
		}
//	Write frame
		if (f_writeframe==WF_WRITE)
		{
			i64frameswritten++;
			writewavsample (m_Fout, caracter,iBytesPerFrame);
		}
		else if (f_writeframe==WF_SILENCE)
		{
			i64frameswritten++;
			writewavsample (m_Fout, p_silence,iBytesPerFrame);
		}

		if (!bTooManyErrors && nuerrors > MAXLOGERRORS)
		{
			printlog("Too Many Errors. Stop Logging.");
			bTooManyErrors=true;
		}

	}

// Rewrite in header length chunk & data subchunk

	fseek ( m_Fout,(long)4, SEEK_SET);
	i64Aux=i64frameswritten*iBytesPerFrame+iInidata-8;
	fputc((unsigned char)(i64Aux%256),m_Fout);
	fputc((unsigned char)((i64Aux >> 8)%256),m_Fout);
	fputc((unsigned char)((i64Aux >>16)%256),m_Fout);
	fputc((unsigned char)((i64Aux >>24)%256),m_Fout);

	fseek ( m_Fout,(long)(iInidata-4), SEEK_SET);
	i64Aux=i64frameswritten*iBytesPerFrame;
	fputc((unsigned char)(i64Aux%256),m_Fout);
	fputc((unsigned char)((i64Aux >> 8)%256),m_Fout);
	fputc((unsigned char)((i64Aux >>16)%256),m_Fout);
	fputc((unsigned char)((i64Aux >>24)%256),m_Fout);

	bTooManyErrors=false;
	csAux.Format(_T("Number of written samples = %I64d"), i64frameswritten);
	printlog(csAux);
	csAux.Format(_T("Number of Errors= %d"), nuerrors);
	printlog(csAux);
	if (m_bCLI == false) UpdateProgress( pWnd, (int)(100));

	return 0;
}
