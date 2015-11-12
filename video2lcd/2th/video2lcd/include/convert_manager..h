
#ifndef _CONVERT_MANGER_H
#define _CONVERT_MANGER_H

#include <config.h>
#include <video_manager.h>


typedef struct VideoConvert{
	int (*isSupport) (int iPixelFormatIn,int iPixelFormatOut);
	int (*Covert) (PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut);
	int (*ConvertExit)(PT_VideoBuf ptVideoBufOut);
}T_VideoConvert,*PT_VideoConvert;


PT_VideoConvert GetVideoConvertForFormat(int iPixFormatIn, int iPixFormatOut);

int VideoConvertInit(void);


#endif  /*_CONVERT_MANGER_H*/

