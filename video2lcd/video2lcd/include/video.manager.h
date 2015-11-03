

#ifndef _VIDEO_MANGER_H
#define _VIDEO_MANGER_H

#include <config.h>
#include <pic_operation.h>

#define NB_BUFFER 4

struct VideoDevice;
struct VideoBuf;
struct VideoOpr;
typedef struct VideoDevice T_VideoDevice,*PT_VideoDevice;;
typedef struct VideoOpr T_VideoOpr,PT_VideoOpr;


typedef struct VideoDevice{
	int iFd;
	int iPixFormat;
	int iWidth;
	int iHeight;

	int iVideoBufCnt;

	unsigned char *pucVideoBuf[NB_BUFFER];
	
	/*����*/
	PT_VideoOpr ptopr;
	
}T_VideoDevice,*PT_VideoDevice;

typedef struct VideoBuf{
	T_PixelDatas tPixelDatas;
	int 		 iPixelFormat;
}T_VideoBuf,*PT_VideoBuf;

typedef struct VideoOpr{
	char *name;
	int (*InitDevice)(char *strDevName, PT_VideoDevice pt_VideoDevice);
	int (*ExitDevice)(PT_VideoDevice pt_VideoDevice);
	int (*GetFrame)(PT_VideoDevice pt_VideoDevice, PT_VideoBuf ptVideoBuf);
	int (*PutFrame)(PT_VideoDevice pt_VideoDevice, PT_VideoBuf ptVideoBuf);
	int (*StartDevice)(PT_VideoDevice pt_VideoDevice);
	int (*StopDevice)(PT_VideoDevice pt_VideoDevice);
}T_VideoOpr,PT_VideoOpr;


int V4L2Init(void);



#endif /*_VIDEO_MANGER_H*/

