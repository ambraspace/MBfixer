#include "frame.h"

unsigned short BitrateTable[14]={
	32,
	40,
	48,
	56,
	64,
	80,
	96,
	112,
	128,
	160,
	192,
	224,
	256,
	320
};

unsigned long SampleRateTable[3] = {
	44100,
	48000,
	32000,
};

unsigned long ReadHeader(FILE * fin) {
	unsigned long header;
	unsigned char fourbytes[4];
	fread(fourbytes, 1, 4, fin);
	if (feof(fin)) return 0;
	fseek(fin, -4, SEEK_CUR);
	header=fourbytes[0] * 0x1000000;
	header+=fourbytes[1] * 0x10000;
	header+=fourbytes[2] * 0x100;
	header+=fourbytes[3];
	return header;
}

int CheckFrame(unsigned long header) {
	if ((header & 0xfffa0000) != 0xfffa0000) return 0;
	if (((header & 0xf000) == 0) || ((header & 0xf000) == 0xf000)) return 0;
	if ((header & 0xc00) == 0xc00) return 0;
	if (((header & 0x80) == 0x80)) return 0;
	if ((header & 0x3) == 2) return 0;
	return 1;
}

void AnalyzeFrame(struct mp3Frame * fr, unsigned long header) {
	memset(fr, 0, sizeof(struct mp3Frame));
	fr->protected=1-((header >> 16) & 1);
	fr->bitrate=BitrateTable[((header >> 12) & 0xf)-1];
	fr->samplerate=SampleRateTable[(header >> 10) & 3];
	fr->padding=((header >> 9) & 1);
	fr->channel_mode=((header >> 6) & 3);
	fr->copyright=((header >> 3) & 1);
	fr->original=((header >> 2) & 1);
	fr->framesize  = fr->bitrate * 144000 / fr->samplerate + fr->padding;
}

void GetFrameSubInfo(struct mp3Frame * fr, unsigned char * si) {

	fr->side_info.mdb=si[0]*2 + (si[1] >> 7);

	fr->side_info.gr_ch[0][0].part2_3_length=(si[2] & 0xf) * 0x100 + si[3];
	fr->side_info.gr_ch[0][0].ws_flag=(si[6] >> 2) & 1;
	if (fr->side_info.gr_ch[0][0].ws_flag) {
		fr->side_info.gr_ch[0][0].block_type=(si[6] & 3);
		fr->side_info.gr_ch[0][0].block_type = (fr->side_info.gr_ch[0][0].block_type == 2);
		fr->side_info.gr_ch[0][0].mixed_block_flag=(si[7]>>7);
	}

	fr->side_info.gr_ch[0][1].part2_3_length=(si[9] & 1) * 0x800 + si[10] * 8 + (si[11]>>5);
	fr->side_info.gr_ch[0][1].ws_flag=(si[14]>>7);
	if (fr->side_info.gr_ch[0][1].ws_flag) {
		fr->side_info.gr_ch[0][1].block_type=(si[14] >> 5) & 3;
		fr->side_info.gr_ch[0][1].block_type = (fr->side_info.gr_ch[0][1].block_type == 2);
		fr->side_info.gr_ch[0][1].mixed_block_flag=(si[14]>>4)&1;
	}

	fr->side_info.gr_ch[1][0].part2_3_length=(si[17]&0x3f) * 0x40 + (si[18]>>2);
	fr->side_info.gr_ch[1][0].ws_flag=(si[21]>>4)&1;
	if (fr->side_info.gr_ch[1][0].ws_flag) {
		fr->side_info.gr_ch[1][0].block_type=(si[21]>>2)&3;
		fr->side_info.gr_ch[1][0].block_type = (fr->side_info.gr_ch[1][0].block_type == 2);
		fr->side_info.gr_ch[1][0].mixed_block_flag=(si[21]>>1)&1;
	}

	fr->side_info.gr_ch[1][1].part2_3_length=(si[24]&7)*0x200 + si[25]*2 + (si[26]>>7);
	fr->side_info.gr_ch[1][1].ws_flag=(si[28]>>1)&1;
	if (fr->side_info.gr_ch[1][1].ws_flag) {
		fr->side_info.gr_ch[1][1].block_type=(si[28]&1)*2 + (si[29]>>7);
		fr->side_info.gr_ch[1][1].block_type = (fr->side_info.gr_ch[1][1].block_type == 2);
		fr->side_info.gr_ch[1][1].mixed_block_flag=(si[29]>>6)&1;
	}

}

