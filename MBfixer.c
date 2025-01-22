#include <stdio.h>
#include <math.h>

#include "MBfixer.h"
#include "frame.h"
#include <lame/lame.h>



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
        printf("fixed.\n");
        count[1]++;
        break;
      case 1:
        printf("repacked.\n");
        count[2]++;
    }
  }

  printf("%d failed, %d successful (%d fixed, %d repacked).\n", count[0], count[1]+count[2], count[1], count[2]);

  return 0;
}



int ProcessFile(char * fn) {

  int ret = -1;

  INPUTFILE infile;

  char * pcmfn = "MBfixer.pcm";
  char * mp3fn = "MBfixer.mp3";
  char * fixedfn = "MBfixed.mp3";

  FILE * fin=fopen(fn, "rb");
  if (fin==NULL) return -1;

  if (GetInfo(fin, &infile)) return -1;

  if (infile.fix) {

    FILE * fpcm = fopen(pcmfn, "wb+");
    if (fpcm==NULL) return -1;
    FILE * fmp3 = fopen(mp3fn, "wb+");
    if (fmp3==NULL) return -1;
    FILE * ffixed = fopen(fixedfn, "wb+");
    if (ffixed==NULL) return -1;

    if (!DecodeFile(fin, fpcm, &infile))
      if (!EncodeFile(fpcm, fmp3, &infile))
        if (!MBfix(fin, fmp3, ffixed, &infile)) ret = 0;

    fclose(fpcm);
    fclose(fmp3);
    fclose(ffixed);

    remove(pcmfn);
    remove(mp3fn);

  } else {

    FILE * ffixed = fopen(fixedfn, "wb+");
    if (ffixed==NULL) return -1;

    if (!RePack(fin, ffixed, &infile)) ret = 1;

    fclose(ffixed);

  }

  if ((ret==0) || (ret==1)) {
    if (remove(fn)) return -1;
    if (rename(fixedfn, fn)) return -1;
  } else {
    remove(fixedfn);
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
      if (! info->vbr) if (info->bitrate != fr.bitrate) info->vbr=1;
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



int DecodeFile(FILE * fin, FILE * fout, INPUTFILE * info) {
  short pcm[2][4608];
  unsigned char mp3buff[2000];
  int ret, i;
  unsigned long header;
  struct mp3Frame fr;
  short delay;

  fseek(fin, info->id3v2len + info->vbrtaglen, SEEK_SET);

  lame_global_flags * gfp;
  gfp = lame_init();
  lame_set_decode_only(gfp, 1);
  if (-1==lame_init_params(gfp)) return -1;

  lame_decode_init();

  //if (! info->haslametag)
    info->encdelay = lame_get_encoder_delay(gfp);
  delay = info->encdelay;
  delay += 529;

  while (1) {
    header=ReadHeader(fin);
    if (!CheckFrame(header)) break;
    AnalyzeFrame(&fr, header);
    fread(mp3buff, 1, fr.framesize, fin);
    ret=lame_decode(mp3buff, fr.framesize, pcm[0], pcm[1]);
    if (delay >= ret) {
      delay -= ret;
    } else {
      for (i=delay; i<ret; i++) {
        fwrite(pcm[0]+i, 2, 1, fout);
        fwrite(pcm[1]+i, 2, 1, fout);
      }
      delay=0;
    }
  }

  lame_decode_exit();

  return 0;

}



int EncodeFile(FILE * fin, FILE * fout, INPUTFILE * info) {

  unsigned char * mp3data = (unsigned char *) malloc(8640);
  int mp3data_size=8640;
  int i, ret;

  short pcm[2][1152];
  lame_global_flags * gfp;
  gfp = lame_init();

  lame_set_in_samplerate(gfp, info->samplerate);
  lame_set_num_channels(gfp, 2);
  lame_set_out_samplerate(gfp, info->samplerate);
  lame_set_bWriteVbrTag(gfp, 0);
  lame_set_quality(gfp, 2);
  lame_set_mode(gfp, info->channel_mode);
  if (info->vbr) {
    lame_set_VBR(gfp, vbr_abr);
    lame_set_VBR_mean_bitrate_kbps(gfp, info->bitrate);
  } else {
    lame_set_VBR(gfp, vbr_off);
    lame_set_brate(gfp, info->bitrate);
    lame_set_disable_reservoir(gfp, 1);
  }
  lame_set_copyright(gfp, info->copyright);
  lame_set_original(gfp, info->original);
  lame_set_error_protection(gfp, info->protected);
  lame_set_extension(gfp, 1);
  lame_set_allow_diff_short(gfp, 0);

  fseek(fin, 0, SEEK_SET);

  if (-1==lame_init_params(gfp)) return -1;


  while (1) {
    if (feof(fin)) break;
    for (i=0; i<1152; i++) {
      fread(pcm[0]+i, 2, 1, fin);
      fread(pcm[1]+i, 2, 1, fin);
      if (feof(fin)) break;
    }
    ret=lame_encode_buffer(gfp, pcm[0], pcm[1], i, mp3data, mp3data_size);
    if (ret>0) fwrite(mp3data, 1, ret, fout);
  }
  ret=lame_encode_flush(gfp, mp3data, mp3data_size);
  if (ret>0) fwrite(mp3data, 1, ret, fout);

  lame_close(gfp);

  free(mp3data);

  return 0;

}


int MBfix(FILE * fin1, FILE * fin2, FILE * fout, INPUTFILE * info) {

  unsigned long header, newvbrtaglen;
  struct mp3Frame fr1, fr2;
  unsigned char si[32];
  unsigned char * framedata = (unsigned char *) malloc(1);
  unsigned short framedata_size;
  offset_t offsets1[10], offsets2[10], offsets3[10];

  fseek(fin1, 0, SEEK_SET);
  fseek(fin2, 0, SEEK_SET);

  InitializeOffsets(offsets1);
  InitializeOffsets(offsets2);
  InitializeOffsets(offsets3);

  if (info->id3v2len>0) {
    framedata=(unsigned char *) realloc(framedata, info->id3v2len);
    if (framedata==NULL) return -1;
    fread(framedata, 1, info->id3v2len, fin1);
    fwrite(framedata, 1, info->id3v2len, fout);
  }

  newvbrtaglen = info->bitrate * 144000 / info->samplerate;
  if ((info->vbrtaglen > 0) || info->vbr) fseek(fout, newvbrtaglen, SEEK_CUR);

  fseek(fin1, info->id3v2len + info->vbrtaglen, SEEK_SET);

  framedata=(unsigned char *) realloc(framedata, 2000);
  if (framedata==NULL) return -1;

  while (1) {

    header=ReadHeader(fin1);
    if (!CheckFrame(header)) break;
    AnalyzeFrame(&fr1, header);
    fseek(fin1, 4+2*fr1.protected, SEEK_CUR);
    fread(si, 1, 32, fin1);
    fseek(fin1, - (4+2*fr1.protected+32), SEEK_CUR);
    GetFrameSubInfo(&fr1, si);
    AddNewOffset(offsets1, ftell(fin1) + 4 + 2 * fr1.protected + 32, ftell(fin1) + fr1.framesize-1);

    header=ReadHeader(fin2);
    if (!CheckFrame(header)) return -1;
    AnalyzeFrame(&fr2, header);
    fseek(fin2, 4+2*fr2.protected, SEEK_CUR);
    fread(si, 1, 32, fin2);
    fseek(fin2, - (4+2*fr2.protected+32), SEEK_CUR);
    GetFrameSubInfo(&fr2, si);
    AddNewOffset(offsets2, ftell(fin2) + 4 + 2 * fr2.protected + 32, ftell(fin2) + fr2.framesize-1);

    if ((fr1.side_info.gr_ch[0][0].block_type != fr1.side_info.gr_ch[0][1].block_type) ||
      (fr1.side_info.gr_ch[1][0].block_type != fr1.side_info.gr_ch[1][1].block_type)) {
      if (GetFrameData(fin2, offsets2, &fr2, framedata, &framedata_size, info)) return -1;
      if (PutFrameData(fout, offsets3, &fr2, framedata, &framedata_size, info)) return -1;
    } else {
      if (GetFrameData(fin1, offsets1, &fr1, framedata, &framedata_size, info)) return -1;
      if (PutFrameData(fout, offsets3, &fr1, framedata, &framedata_size, info)) return -1;
    }

    fseek(fin1, fr1.framesize, SEEK_CUR);
    fseek(fin2, fr2.framesize, SEEK_CUR);
  }

  if (info->hasid3v1) {
    fseek(fin1, -128, SEEK_END);
    fread(framedata, 1, 128, fin1);
    fwrite(framedata, 1, 128, fout);
  }

  if ((info->vbrtaglen > 0) || info->vbr) if (WriteVBRTag(fout, info)) return -1;

  free(framedata);

  return 0;

}

int RePack(FILE * fin, FILE * fout, INPUTFILE * info) {
  unsigned long header, newvbrtaglen;
  struct mp3Frame fr;
  unsigned char si[32];
  unsigned char * framedata = (unsigned char *) malloc(1);
  unsigned short framedata_size;
  offset_t offsets1[10], offsets2[10];

  fseek(fin, 0, SEEK_SET);

  InitializeOffsets(offsets1);
  InitializeOffsets(offsets2);

  if (info->id3v2len>0) {
    framedata=(unsigned char *) realloc(framedata, info->id3v2len);
    if (framedata==NULL) return -1;
    fread(framedata, 1, info->id3v2len, fin);
    fwrite(framedata, 1, info->id3v2len, fout);
  }

  newvbrtaglen = info->bitrate * 144000 / info->samplerate;
  if ((info->vbrtaglen > 0) || info->vbr) fseek(fout, newvbrtaglen, SEEK_CUR);

  fseek(fin, info->id3v2len + info->vbrtaglen, SEEK_SET);

  framedata=(unsigned char *) realloc(framedata, 2000);
  if (framedata==NULL) return -1;

  while (1) {

    header=ReadHeader(fin);
    if (!CheckFrame(header)) break;
    AnalyzeFrame(&fr, header);
    fseek(fin, 4+2*fr.protected, SEEK_CUR);
    fread(si, 1, 32, fin);
    fseek(fin, - (4+2*fr.protected+32), SEEK_CUR);
    GetFrameSubInfo(&fr, si);
    AddNewOffset(offsets1, ftell(fin) + 4 + 2 * fr.protected + 32, ftell(fin) + fr.framesize-1);

    if (GetFrameData(fin, offsets1, &fr, framedata, &framedata_size, info)) return -1;
    if (PutFrameData(fout, offsets2, &fr, framedata, &framedata_size, info)) return -1;

    fseek(fin, fr.framesize, SEEK_CUR);
  }

  if (info->hasid3v1) {
    fseek(fin, -128, SEEK_END);
    fread(framedata, 1, 128, fin);
    fwrite(framedata, 1, 128, fout);
  }

  if ((info->vbrtaglen > 0) || info->vbr) if (WriteVBRTag(fout, info)) return -1;

  free(framedata);

  return 0;

}


void InitializeOffsets(offset_t * offset) {
  int i;
  for (i=0; i<10; i++) {
    offset[i].start=0;
    offset[i].stop=-1;
  }
}



void AddNewOffset(offset_t * offset, long start, long stop) {
  int i;
  for (i=0; i<9; i++) {
    offset[i].start=offset[i+1].start;
    offset[i].stop=offset[i+1].stop;
  }
  offset[9].start=start;
  offset[9].stop=stop;
}



int GetFrameData(FILE * fin, offset_t * offset, struct mp3Frame * fr, unsigned char * framedata, unsigned short * framedata_size, INPUTFILE * info) {

  unsigned short datasize, off=0, mdb;
  int i;
  unsigned long stop=ftell(fin);

  if (GetFrameNettSize(fin, fr, info)) return -1;
  datasize=fr->framenettsize;
  fread(framedata+off, 1, 4+2*fr->protected+32, fin);
  off += 4+2*fr->protected+32;
  datasize -= 4+2*fr->protected+32;

  mdb=fr->side_info.mdb;

  for (i=8; i>=0; i--) {
    if (mdb>=(offset[i].stop-offset[i].start+1)) {
      mdb -= (offset[i].stop-offset[i].start+1);
    } else {
      if (mdb==0) {
        offset[i].start=0;
        offset[i].stop=-1;
      } else {
        offset[i].start += (offset[i].stop - offset[i].start + 1 - mdb);
        mdb=0;
      }
    }
  }


  for (i=0; i<10; i++) {
    if (offset[i].stop != -1) {
      if (datasize<=(offset[i].stop-offset[i].start+1)) {
        fseek(fin, offset[i].start, SEEK_SET);
        fread(framedata+off, 1, datasize, fin);
        off += datasize;
        break;
      } else {
        fseek(fin, offset[i].start, SEEK_SET);
        fread(framedata+off, 1, offset[i].stop - offset[i].start +1, fin);
        datasize -= (offset[i].stop - offset[i].start +1);
        off += (offset[i].stop - offset[i].start +1);
      }
    }
  }
  *framedata_size=off;

  fseek(fin, stop, SEEK_SET);

  framedata[2] &= 0xfe; /* reset private bit */

  return 0;
}



int PutFrameData(FILE * fout, offset_t * offset, struct mp3Frame * fr, unsigned char * framedata, unsigned short * framedata_size, INPUTFILE * info) {
  short mdb, off=0, datasize, i, diff=0;
  unsigned long stop, newvbrtaglen;
  char zero='\0';

  stop=ftell(fout) + fr->framesize;
  AddNewOffset(offset, ftell(fout) + 4 + 2*fr->protected + 32, stop-1);
  mdb=GetMDB(offset);

  newvbrtaglen = info->bitrate * 144000 / info->samplerate;

  while (mdb + fr->framesize + diff < *framedata_size) {
    if ((! info->vbr) && (info->vbrtaglen==0)) {
      info->vbr=1;
      if (InsertVBRTagSpace(fout, info)) return -1;
      stop += newvbrtaglen;
      for (i=0; i<10; i++) {
        if (offset[i].stop != -1) {
          offset[i].start += newvbrtaglen;
          offset[i].stop += newvbrtaglen;
        }
      }
    }
    if (fr->bitrate==320) return -1;
    fr->bitrate=BitrateTable[framedata[2]>>4];
    framedata[2] += 0x10;
    framedata[2] &= 0xfd; /* reset padding bit */
    diff=(fr->bitrate * 144000 / fr->samplerate) - fr->framesize;
  }

  fr->framesize += diff;
  stop += diff;
  offset[9].stop += diff;

  fseek(fout, stop-1, SEEK_SET);
  fwrite(&zero, 1, 1, fout);
  fseek(fout, - fr->framesize, SEEK_CUR);

  *(framedata + 4 + 2 * fr->protected) = mdb / 2;
  *(framedata + 4 + 2 * fr->protected + 1) &= 0x7f;
  *(framedata + 4 + 2 * fr->protected + 1) |= ((mdb % 2 == 1) ? 0x80 : 0);

  fwrite(framedata + off, 1, 4+2*fr->protected+32, fout);
  off += (4+2*fr->protected+32);

  datasize = *framedata_size - (4+2*fr->protected+32);

  for (i=0; i<10; i++) {
    if (offset[i].stop != -1) {
      if (datasize <= (offset[i].stop - offset[i].start +1)) {
        if (datasize>0) {
          fseek(fout, offset[i].start, SEEK_SET);
          fwrite(framedata + off, 1, datasize, fout);
          if (datasize==(offset[i].stop - offset[i].start +1)) {
            offset[i].start=0;
            offset[i].stop=-1;
          } else {
            offset[i].start += datasize;
          }
        }
        break;
      } else {
        fseek(fout, offset[i].start, SEEK_SET);
        fwrite(framedata + off, 1, offset[i].stop - offset[i].start + 1, fout);
        datasize -= (offset[i].stop - offset[i].start + 1);
        off += (offset[i].stop - offset[i].start + 1);
        offset[i].start=0;
        offset[i].stop=-1;
      }
    }
  }

  fseek(fout, stop, SEEK_SET);

  fr->framesize -= diff;

  return 0;
}



int GetFrameNettSize(FILE * fin, struct mp3Frame * fr, INPUTFILE * info) {
  unsigned long offset=ftell(fin), header;
  struct mp3Frame nextframe;
  unsigned char si[32];

  fseek(fin, fr->framesize, SEEK_CUR);
  header=ReadHeader(fin);
  if (CheckFrame(header) && info->vbr) {
    AnalyzeFrame(&nextframe, header);
    fseek(fin, 4+2*nextframe.protected, SEEK_CUR);
    fread(si, 1, 32, fin);
    GetFrameSubInfo(&nextframe, si);
    fr->framenettsize = fr->framesize + fr->side_info.mdb - nextframe.side_info.mdb;
  } else {
    header=fr->side_info.gr_ch[0][0].part2_3_length + fr->side_info.gr_ch[0][1].part2_3_length +
        fr->side_info.gr_ch[1][0].part2_3_length + fr->side_info.gr_ch[1][1].part2_3_length;
    if ((header % 8)==0) {
      fr->framenettsize=header / 8;
    } else {
      fr->framenettsize=header / 8 + 1;
    }
    fr->framenettsize += 4 + 2*fr->protected + 32;
  }

  fseek(fin, offset, SEEK_SET);

  return 0;
}



short GetMDB(offset_t * offset) {
  short i, size=0;
  for (i=0; i<9; i++) {
    if (offset[i].stop!=-1) {
      size += offset[i].stop - offset[i].start +1;
    }
  }

  if (size>511) {
    size -=511;
    for (i=0; i<10; i++) {
      if (offset[i].stop!=-1) {
        if ((offset[i].stop - offset[i].start + 1)>size) {
          offset[i].start += size;
          break;
        } else {
          size -= offset[i].stop - offset[i].start + 1;
          offset[i].start=0;
          offset[i].stop=-1;
          if (size==0) break;
        }
      }
    }
    return 511;
  } else {
    return size;
  }
}


int WriteVBRTag(FILE * file, INPUTFILE * info) {
  unsigned char chunk[5], toc[100];
  char tag1[] = "Xing";
  char tag2[] = "Info";
  unsigned long header, count=0, newvbrtaglen;
  long streamlen, frames[100];
  struct mp3Frame fr;
  int i;

  newvbrtaglen = info->bitrate * 144000 / info->samplerate;

  fseek(file, info->id3v2len, SEEK_SET);

  chunk[0]=0xff;
  chunk[1]=(info->protected) ? 0xfa : 0xfb;
  for (i=0; i<14; i++) {
    if (info->bitrate == BitrateTable[i]) {
      chunk[2] = (i+1)<<4;
      break;
    }
  }
  switch (info->samplerate) {
    case 48000:
      chunk[2]+=4;
      break;
    case 32000:
      chunk[2]+=8;
  }
  chunk[3]=info->channel_mode << 6;
  if (info->copyright) chunk[3] += 8;
  if (info->original) chunk[3] += 4;
  fwrite(chunk, 1, 4, file);

  fseek(file, 32, SEEK_CUR);

  if (info->vbr)
    fwrite(tag1, 1, 4, file);
  else
    fwrite(tag2, 1, 4, file);

  fseek(file, 0, SEEK_END);
  streamlen = ftell(file);
  streamlen -= info->id3v2len;
  if (info->hasid3v1) streamlen -= 128;

  for (i=0; i<100; i++) frames[i] = i * info->numframes / 100;

  i=0;
  fseek(file, info->id3v2len + newvbrtaglen, SEEK_SET);
  while (1) {
    header=ReadHeader(file);
    if (!CheckFrame(header)) break;
    if (count==frames[i]) {
      toc[i] = (ftell(file) - info->id3v2len) * 256 / streamlen;
      i++;
      if (i==100) break;
    }
    count++;
    AnalyzeFrame(&fr, header);
    fseek(file, fr.framesize, SEEK_CUR);
  }


  chunk[0]=chunk[1]=chunk[2]=0;
  chunk[3]=(info->scale > 0) ? 0x0f : 0x07;

  fseek(file, info->id3v2len + 40, SEEK_SET);
  fwrite(chunk, 1, 4, file);

  chunk[0]=info->numframes / 0x1000000;
  info->numframes %= 0x1000000;
  chunk[1]=info->numframes / 0x10000;
  info->numframes %= 0x10000;
  chunk[2]=info->numframes / 0x100;
  info->numframes %= 0x100;
  chunk[3]=info->numframes;

  fwrite(chunk, 1, 4, file);

  info->lametag[24]=chunk[0]=streamlen / 0x1000000;
  streamlen %= 0x1000000;
  info->lametag[25]=chunk[1]=streamlen / 0x10000;
  streamlen %= 0x10000;
  info->lametag[26]=chunk[2]=streamlen / 0x100;
  streamlen %= 0x100;
  info->lametag[27]=chunk[3]=streamlen;

  fwrite(chunk, 1, 4, file);

  fwrite(toc, 1, 100, file);

  if (info->scale>0) fwrite(&(info->scale), 4, 1, file);

  if (info->haslametag) {
    chunk[0]='L';
    chunk[1]='A';
    chunk[2]='M';
    chunk[3]='E';
    fwrite(chunk, 1, 4, file);
    info->lametag[28] = info->lametag[29] = info->lametag[30] = info->lametag[31] = '\0';
    fwrite(info->lametag, 1, 32, file);
  }

  return 0;
}

int InsertVBRTagSpace(FILE * file, INPUTFILE * info) {
  unsigned long stop = ftell(file), musicsize, newvbrtaglen;
  unsigned char * chunk = (unsigned char *) malloc(2000);
  if (chunk==NULL) return -1;
  musicsize = stop - info->id3v2len;
  newvbrtaglen = info->bitrate * 144000 / info->samplerate;
  while (musicsize >= 2000) {
    fseek(file, -2000, SEEK_CUR);
    fread(chunk, 1, 2000, file);
    fseek(file, - (2000-newvbrtaglen), SEEK_CUR);
    fwrite(chunk, 1, 2000, file);
    fseek(file, - (2000 + newvbrtaglen), SEEK_CUR);
    musicsize -= 2000;
  }
  if (musicsize > 0) {
    fseek(file, -musicsize, SEEK_CUR);
    fread(chunk, 1, musicsize, file);
    fseek(file, newvbrtaglen - musicsize, SEEK_CUR);
    fwrite(chunk, 1, musicsize, file);
    fseek(file, - (musicsize + newvbrtaglen), SEEK_CUR);
  }
  memset(chunk, '\0', newvbrtaglen);
  fwrite(chunk, 1, newvbrtaglen, file);

  fseek(file, stop + newvbrtaglen, SEEK_SET);
  free(chunk);
  return 0;
}


