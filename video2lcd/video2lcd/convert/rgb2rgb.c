
#include <convert_manager.h>


static int Rgb2RgbisSupport (int iPixelFormatIn,int iPixelFormatOut)
{
	if(iPixelFormatIn != V4L2_PIX_FMT_RGB565)		
		return 0;
	if((iPixelFormatOut != V4L2_PIX_FMT_RGB565) && (iPixelFormatOut != V4L2_PIX_FMT_RGB565))
		return 0;
	return 1;
}
static int Rgb2RgbCovert (PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut)
{
	PT_PixelDatas ptPixDataIn = &ptVideoBufIn->tPixelDatas;
	PT_PixelDatas ptPixDataOut ;

	int x,y;
	int r,g,b;
	int color;
	unsigned short *pwSrc = ptPixDataIn->aucPixelDatas;
	unsigned int   *pdwDst = ptPixDataOut->aucPixelDatas;

	if(ptVideoBufIn->iPixelFormat != V4L2_PIX_FMT_RGB565)
	{
		return -1;
	}
	
	if(ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB565)
	{
		ptPixDataOut->iWidth = ptPixDataIn->iWidth;
		ptPixDataOut->iHeight= ptPixDataIn->iHeight;
		ptPixDataOut->iBpp	 = 16;
		ptPixDataOut->iLineBytes= ptPixDataOut->iWidth*ptPixDataOut->iBpp /8;
		ptPixDataOut->iTotalBytes= ptPixDataOut->iHeight*ptPixDataOut->iLineBytes;
		if(!ptPixDataOut->aucPixelDatas)
		{
			ptPixDataOut->aucPixelDatas = malloc(ptPixDataOut->iTotalBytes);
		}
		memcpy(ptPixDataOut->aucPixelDatas, ptPixDataIn->aucPixelDatas, ptPixDataOut->iTotalBytes);
		return 0;
	}
	
	if(ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB32)
	{
		ptPixDataOut->iWidth = ptPixDataIn->iWidth;
		ptPixDataOut->iHeight= ptPixDataIn->iHeight;
		ptPixDataOut->iBpp	 = 32;
		ptPixDataOut->iLineBytes= ptPixDataOut->iWidth*ptPixDataOut->iBpp /8;
		ptPixDataOut->iTotalBytes= ptPixDataOut->iHeight*ptPixDataOut->iLineBytes;
		if(!ptPixDataOut->aucPixelDatas)
		{
			ptPixDataOut->aucPixelDatas = malloc(ptPixDataOut->iTotalBytes);
		}
		
		pdwDst = (unsigned int *) ptVideoBufOut->tPixelDatas;
		for(y=0; y < ptPixDataOut->iHeight; y++)
		{
			for(x=0; x < ptPixDataOut->iWidth; x++)
				{
					color = *pwSrc++;
					/* 从RGB565格式中提取出R,G,B */
					r = color >> 11;
					g = (color >> 5) & (0x3f);
					b = (color & 0x1f);

					/* 把rgb转为0x00RRGGBB的32位数据 */
					color = ((r << 3) << 16) | ((g << 2) << 8) | (b << 3);

					*pdwDst = color; 
					pdwDst++;
				}
		}
		return 0;
	}
	
	return -1;
}
static  int Rgb2RgbConvertExit(PT_VideoBuf ptVideoBufOut)
{
	if(ptVideoBufOut->tPixelDatas.aucPixelDatas)
	{
		free(ptVideoBufOut->tPixelDatas.aucPixelDatas);
		ptVideoBufOut->tPixelDatas.aucPixelDatas = NULL;
	}
	return 0;
}

/*构造一个结构体*/
struct T_VideoConvert  g_tRgb2RgbConvert = {
	.isSupport 	= Rgb2RgbisSupport,
	.Covert   	= Rgb2RgbCovert,
	.ConvertExit= Rgb2RgbConvertExit,
};

/*注册*/
int Rgb2RgbInit(void)
{
	return RegisterVideoConvert(g_tRgb2RgbConvert);
}

