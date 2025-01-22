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

#endif
