#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <config.h>
#include <disp_manager.h>
#include <video_manager.h>
#include <convert_manager.h>
#include <render.h>
#include <string.h>
#include <picfmt_manager.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>


/* video2lcd </dev/video0,1,,,>*/
int main(int argc, char **argv)
{	
	int iError;
	T_VideoDevice tVideoDevice;
	PT_VideoConvert ptVideoConvert;
	int iPixFormatOfVideo;
	int iPixFormatOfDisp;

	PT_VideoBuf ptVideoBufCur;
	T_VideoBuf tVideoBuf;
	T_VideoBuf tConvertBuf;

	T_VideoBuf tZoomBuf;
	T_VideoBuf tFrameBuf;
	
	int iLcdWidth,iLcdHeight,iLcdBpp;
	int iTopLeftX,iTopLeftY ;

	float k;

	if (argc != 2)
	{
		DBG_PRINTF("Usage:\n");
		DBG_PRINTF("%s video2lcd </dev/video0,1,,,>\n", argv[0]);
		return 0;
	}


	/*一系列的初始化*/

	/* 注册显示设备 */
	DisplayInit();
	/* 可能可支持多个显示设备: 选择和初始化指定的显示设备 */
	SelectAndInitDefaultDispDev("fb");
	GetDispResolution(&iLcdWidth, &iLcdHeight, &iLcdBpp);
	GetVideoBufForDisplay(&tFrameBuf);
	iPixFormatOfDisp = tFrameBuf.iPixelFormat;

	VideoInit();

	iError = VideoDeviceInit(argv[1], &tVideoDevice);
	if(iError)
	{
		DBG_PRINTF("VideoInit for %s error\n",argv[1]);
		return -1;
	}

	iPixFormatOfVideo = tVideoDevice.ptopr->GetFormat(&tVideoDevice);
	
	VideoConvertInit();
	ptVideoConvert = GetVideoConvertForFormat(iPixFormatOfVideo, iPixFormatOfDisp);
	if(ptVideoConvert == NULL)
	{
		DBG_PRINTF("Can not support this format convert !\n");
		return -1;
	}
	
	/*启动摄像头*/
	iError = tVideoDevice.ptopr->StartDevice(&tVideoDevice);
	if(iError)
	{
		DBG_PRINTF("Start device for %s error\n",argv[1]);
		return -1;
	}	

	memset(&tVideoBuf, 0, sizeof(tVideoBuf));
	memset(&tConvertBuf, 0, sizeof(tConvertBuf));
	tConvertBuf.tPixelDatas->iBpp = iLcdBpp;

	memset(&tZoomBuf, 0, sizeof(tZoomBuf));


	while(1)
	{
		/*读入摄像头数据*/
		iError = tVideoDevice.ptopr->GetFrame(&tVideoDevice, &tVideoBuf);
		if(iError)
		{
			DBG_PRINTF("GetFrame for %s error\n",argv[1]);
			return -1;
		}
		ptVideoBufCur = &tVideoBuf;
		
		/*转换为RGB数据*/
		if(iPixFormatOfVideo != iPixFormatOfDisp)
		{
			/*转换为RGB数据*/
			iError = ptVideoConvert->Covert(&tVideoBuf, &tConvertBuf);
			if(iError)
			{
				DBG_PRINTF("Covert for %s error\n",argv[1]);
				return -1;
			}
			ptVideoBufCur = &tConvertBuf;
		}
		
		/*如果图像分辨率，大于LCD,缩放*/
		if (ptVideoBufCur->tPixelDatas.iWidth > iLcdWidth  || ptVideoBufCur->tPixelDatas.iHeight> iLcdHeight)
		{
			/*确定缩放后的分辨率*/
			/* 把图片按比例缩放到VideoMem上, 居中显示
			 * 1. 先算出缩放后的大小
			 */
			k = (float)ptVideoBufCur->tPixelDatas.iHeight / ptVideoBufCur->tPixelDatas.iWidth;
			tZoomBuf.tPixelDatas.iWidth  = iLcdWidth;
			tZoomBuf.tPixelDatas.iHeight = iLcdWidth* k;
			if (tZoomBuf.tPixelDatas.iHeight > iLcdHeight)
			{
				tZoomBuf.tPixelDatas.iWidth  = iLcdHeight / k;
				tZoomBuf.tPixelDatas.iHeight = iLcdHeight;
			}
			tZoomBuf.tPixelDatas.iBpp 	   = iLcdBpp;
			tZoomBuf.tPixelDatas.iLineBytes  = tZoomBuf->tPixelDatas.iWidth * tZoomBuf.tPixelDatas.iBpp / 8;
			tZoomBuf.tPixelDatas.iTotalBytes = tZoomBuf->tPixelDatas.iLineBytes * tZoomBuf.tPixelDatas.iHeight;
			if(!tZoomBuf.tPixelDatas.aucPixelDatas)
			{
				tZoomBuf.tPixelDatas.aucPixelDatas = malloc(tZoomBuf.tPixelDatas.iTotalBytes);
			}
			if (tZoomBuf.tPixelDatas.aucPixelDatas == NULL)
			{
				PutVideoMem(ptVideoMem);
				return NULL;
			}
			
			PicZoom(&ptVideoBufCur.tPixelDatas, &tZoomBuf.tPixelDatas);
			ptVideoBufCur = &tZoomBuf;
		}

		/*合并进入frmaebuffer*/
		/* 接着算出居中显示时左上角坐标 */
		iTopLeftX = (iLcdWidth- ptVideoBufCur->tPixelDatas.iWidth) / 2;
		iTopLeftY = (iLcdHeight- ptVideoBufCur->tPixelDatas.iHeight) / 2;

		PicMerge(iTopLeftX,iTopLeftY,&ptVideoBufCur->tPixelDatas,&tFrameBuf.tPixelDatas);

		FlushPixelDataToDev(&tFrameBuf.tPixelDatas);
		iError = tVideoDevice.ptopr->PutFrame(&tVideoDevice, &tVideoBuf);
		if (iError)
		{
			DBG_PRINTF("PutFrame for %s error\n",argv[1]);
			return -1;
		}
		
		/*把framebuffer的数据刷到LCD上*/
		
	}
	


	/* 初始化调试模块: 可以通过"标准输出"也可以通过"网络"打印调试信息
	 * 因为下面马上就要用到DBG_PRINTF函数, 所以先初始化调试模块
	 */


	return 0;
}

