#ifndef _AV_TYPE_H_
#define _AV_TYPE_H_

typedef enum 
{
	PCM,
	ADPCM
} AUDIO_CODEC;

typedef struct tagAudioChunk
{
	AUDIO_CODEC codec;
	unsigned long seq;
	unsigned long tick;
	int t;
	short sample;
	unsigned char index;
    //unsigned char res;
	unsigned long len;
	unsigned char * data;
	struct list_head list;
} AUDIO_CHUNK;

#define MJPG							0
#define MP4S							1
#define H264							2

#define SQCIF							0		//	128*96
#define QQVGA_H112						1		//	160*112
#define QQVGA							2		//	160*120
#define QCIF							3		//	176*144
#define QVGA							4		//	320*240
#define CIF								5		//	352*288
#define VGA								6		//	640*240
#define D1_W704							7		//	704*576
#define D1								8		//	720*576
#define SVGA							9		//	800*600
#define XGA								10		//	1024*768
#define H720P							11		//	1280*720

typedef struct tagVideoFrame
{
	int codec;
	int resolution;
	//unsigned short width;
	//unsigned short height;
	//unsigned short roi_width;
	//unsigned short roi_height;
	unsigned char stream;
	bool snapshot;
	bool keyframe;
	unsigned long tick;
	int t;
	unsigned long len;
	unsigned char * data;
	struct list_head list;
} VIDEO_FRAME;

#define CAP_SQCIF				(1 << SQCIF)
#define CAP_QQVGA_H112			(1 << QQVGA_H112)
#define CAP_QQVGA				(1 << QQVGA)
#define CAP_QCIF				(1 << QCIF)
#define CAP_QVGA				(1 << QVGA)
#define CAP_CIF					(1 << CIF)
#define CAP_VGA					(1 << VGA)
#define CAP_D1					(1 << D1)
#define CAP_SVGA				(1 << SVGA)
#define CAP_XGA					(1 << XGA)
#define CAP_H720P				(1 << H720P)

#endif
