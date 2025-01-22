#ifndef MBFIXER_H
#define MBFIXER_H

#include <stdio.h>
#include "frame.h"

typedef struct {
  short fix;
  short bitrate;
  short vbr;
  long samplerate;
  enum channel_mode_e channel_mode;
  short protected;
  short copyright;
  short original;
  unsigned short id3v2len;
  unsigned short vbrtaglen;
  unsigned long scale;
  short haslametag;
  unsigned char lametag[32];
  unsigned short encdelay;
  unsigned long numframes;
  short hasid3v1;
} INPUTFILE;


typedef struct {
  long start;
  long stop;
} offset_t;


int ProcessFile(char * fn);
int GetInfo(FILE * fin, INPUTFILE * info);
int DecodeFile(FILE * fin, FILE * fout, INPUTFILE * info);
int EncodeFile(FILE * fin, FILE * fout, INPUTFILE * info);
int MBfix(FILE * fin1, FILE * fin2, FILE * fout, INPUTFILE * info);
int RePack(FILE * fin, FILE * fout, INPUTFILE * info);

void InitializeOffsets(offset_t * offset);
void AddNewOffset(offset_t * offset, long start, long stop);
int GetFrameData(FILE * fin, offset_t * offset, struct mp3Frame * fr, unsigned char * framedata, unsigned short * framedata_size, INPUTFILE * info);
int PutFrameData(FILE * fout, offset_t * offset, struct mp3Frame * fr, unsigned char * framedata, unsigned short * framedata_size, INPUTFILE * info);
int GetFrameNettSize(FILE * fin, struct mp3Frame * fr, INPUTFILE * info);
short GetMDB(offset_t * offset);
int WriteVBRTag(FILE * fin, INPUTFILE * info);
int InsertVBRTagSpace(FILE * file, INPUTFILE * info);

#endif
