#include <stdio.h>

#include "MBfixer.h"
#include "frame.h"


extern unsigned short BitrateTable[14];


int main(int argc, char * argv[]) {

  int i, ret;
  unsigned short count[3]={0, 0, 0};

  if (argc<2) {
    printf("File name(s) expected!\n");
    return -1;
  }

  for (i=1; i<argc; i++) {
    printf("Processing '%s'... ", argv[i]);
    fflush(stdout);
    ret=ProcessFile(argv[i]);
    switch (ret) {
      case -1:
        printf("failed.\n");
        count[0]++;
        break;
      case 0:
        printf("OK.\n");
        count[1]++;
        break;
      case 1:
        printf("BAD.\n");
        count[2]++;
    }
  }

  printf("%d failed, %d successful (%d OK, %d BAD).\n", count[0], count[1]+count[2], count[1], count[2]);

  return 0;
}



int ProcessFile(char * fn) {

  int ret = -1;

  INPUTFILE infile;

  FILE * fin=fopen(fn, "rb");
  if (fin==NULL) return -1;

  if (GetInfo(fin, &infile)) return -1;

  if (infile.fix) {
    ret=1;
  } else {
    ret=0;
  }

  fclose(fin);
  return ret;
}



int GetInfo(FILE * fin, INPUTFILE * info) {
  int i, vbrate;
  unsigned char chunk[5];
  unsigned long header, count=0, bratesum=0;
  struct mp3Frame fr;
  unsigned char si[32];

  memset(info, 0, sizeof(INPUTFILE));

  fseek(fin, 0, SEEK_SET);

  fread(chunk, 1, 3, fin);
  if (chunk[0]=='I' && chunk[1]=='D' && chunk[2]=='3') {
    fseek(fin, 2, SEEK_CUR);
    fread(chunk, 1, 5, fin);
    header=chunk[1] * 128 * 128 * 128;
    header += chunk[2] * 128 * 128;
    header += chunk[3] * 128;
    header += chunk[4];
    header += ((chunk[0] & 0x10) == 0x10) ? 20 : 10;
    info->id3v2len=header;
  }

  fseek(fin, info->id3v2len, SEEK_SET);
  header=ReadHeader(fin);
  if (!CheckFrame(header)) return -1;
  AnalyzeFrame(&fr, header);
  fseek(fin, 36, SEEK_CUR);
  fread(chunk, 1, 4, fin);
  if ((chunk[0]=='X' && chunk[1]=='i' && chunk[2]=='n' && chunk[3]=='g') || (chunk[0]=='I' && chunk[1]=='n' && chunk[2]=='f' && chunk[3]=='o')) {
    info->vbrtaglen=fr.framesize;
    fseek(fin, 112, SEEK_CUR);
    fread(&(info->scale), 4, 1, fin);
    fread(chunk, 1, 4, fin);
    if (chunk[0]=='L' && chunk[1]=='A' && chunk[2]=='M' && chunk[3]=='E') {
      info->haslametag=1;
      fread(info->lametag, 1, 32, fin);
      info->encdelay = info->lametag[17] * 16 + (info->lametag[18] >> 4);
      fseek(fin, fr.framesize - 192, SEEK_CUR);
    } else {
      fseek(fin, fr.framesize - 160, SEEK_CUR);
    }
  } else {
    fseek(fin, -40, SEEK_CUR);
  }

  while (1) {
    header=ReadHeader(fin);
    if (!CheckFrame(header)) break;
    count++;
    AnalyzeFrame(&fr, header);
    fseek(fin, 4 + 2 * fr.protected, SEEK_CUR);
    fread(si, 1, 32, fin);
    fseek(fin, - (4 + 2 * fr.protected + 32), SEEK_CUR);
    GetFrameSubInfo(&fr, si);

    if (count==1) {
      info->bitrate=fr.bitrate;
      info->samplerate=fr.samplerate;
      info->channel_mode=fr.channel_mode;
      info->protected=fr.protected;
      info->copyright=fr.copyright;
      info->original=fr.original;
    } else {
      if (info->bitrate != fr.bitrate) info->vbr=1;
    }
    bratesum += fr.bitrate;

    if ((fr.side_info.gr_ch[0][0].block_type != fr.side_info.gr_ch[0][1].block_type) ||
      (fr.side_info.gr_ch[1][0].block_type != fr.side_info.gr_ch[1][1].block_type)) {
      info->fix = 1;
    }


    fseek(fin, fr.framesize, SEEK_CUR);
  }

  if (! info->bitrate) return -1;

  info->numframes=count;

  if (info->vbr) {
    vbrate = bratesum / count;
    info->bitrate=0;
    for (i=0; i<14; i++) {
      if (abs(vbrate - BitrateTable[i]) < abs(vbrate - info->bitrate)) info->bitrate=BitrateTable[i];
    }
  }

  fseek(fin, -128, SEEK_END);
  fread(chunk, 1, 3, fin);
  if (chunk[0]=='T' && chunk[1]=='A' && chunk[2]=='G') info->hasid3v1=1;

  return 0;
}


