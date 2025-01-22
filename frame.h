#ifndef FRAME_H
#define FRAME_H

#include <stdio.h>

enum channel_mode_e {
  ch_stereo=0,
  ch_joint,
};

struct granuleInfo {
  unsigned short part2_3_length;
  unsigned char ws_flag;
  unsigned char block_type;
  unsigned char mixed_block_flag;
};

struct mp3FrameSi {
  unsigned short mdb;
  struct granuleInfo gr_ch[2][2];
};

struct mp3Frame {
  unsigned char protected;
  unsigned short bitrate;
  unsigned long samplerate;
  unsigned char padding;
  enum channel_mode_e channel_mode;
  unsigned char copyright;
  unsigned char original;
  unsigned short framesize;
  unsigned short framenettsize;
  struct mp3FrameSi side_info;
};

unsigned long ReadHeader(FILE * fin);
int CheckFrame(unsigned long header);
void AnalyzeFrame(struct mp3Frame * fr, unsigned long header);
void GetFrameSubInfo(struct mp3Frame * fr, unsigned char * si);

#endif
