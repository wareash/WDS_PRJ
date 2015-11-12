
#include <convert_manager.h>
#include <stdlib.h>
#include "color.h"


static int Yuv2RgbisSupport (int iPixelFormatIn,int iPixelFormatOut)
{
	if(iPixelFormatIn != V4L2_PIX_FMT_YUYV)		
		return 0;
	if((iPixelFormatOut != V4L2_PIX_FMT_RGB565) && (iPixelFormatOut != V4L2_PIX_FMT_RGB565))
		return 0;
	return 1;
}


/*参考lucview*/
/* translate YUV422Packed to rgb24 */
static unsigned int
Pyuv422torgb565(unsigned char * input_ptr, unsigned char * output_ptr, unsigned int image_width, unsigned int image_height)
{
	unsigned int i, size;
	unsigned char Y, Y1, U, V;
	unsigned char *buff = input_ptr;
	unsigned char *output_pt = output_ptr;

    unsigned int r, g, b;
    unsigned int color;
    
	size = image_width * image_height /2;
	for (i = size; i > 0; i--) {
		/* bgr instead rgb ?? */
		Y = buff[0] ;
		U = buff[1] ;
		Y1 = buff[2];
		V = buff[3];
		buff += 4;
		r = R_FROMYV(Y,V);
		g = G_FROMYUV(Y,U,V); //b
		b = B_FROMYU(Y,U); //v

        /* 把r,g,b三色构造为rgb565的16位值 */
        r = r >> 3;
        g = g >> 2;
        b = b >> 3;
        color = (r << 11) | (g << 5) | b;
        *output_pt++ = color & 0xff;
        *output_pt++ = (color >> 8) & 0xff;
			
		r = R_FROMYV(Y1,V);
		g = G_FROMYUV(Y1,U,V); //b
		b = B_FROMYU(Y1,U); //v
		
        /* 把r,g,b三色构造为rgb565的16位值 */
        r = r >> 3;
        g = g >> 2;
        b = b >> 3;
        color = (r << 11) | (g << 5) | b;
        *output_pt++ = color & 0xff;
        *output_pt++ = (color >> 8) & 0xff;
	}
	
	return 0;
} 
/* translate YUV422Packed to rgb24 */

static unsigned int
Pyuv422torgb32(unsigned char * input_ptr, unsigned char * output_ptr, unsigned int image_width, unsigned int image_height)
{
	unsigned int i, size;
	unsigned char Y, Y1, U, V;
	unsigned char *buff = input_ptr;
	unsigned int *output_pt = (unsigned int *)output_ptr;

    unsigned int r, g, b;
    unsigned int color;

	size = image_width * image_height /2;
	for (i = size; i > 0; i--) {
		/* bgr instead rgb ?? */
		Y = buff[0] ;
		U = buff[1] ;
		Y1 = buff[2];
		V = buff[3];
		buff += 4;
		r = R_FROMYV(Y,V);
		g = G_FROMYUV(Y,U,V); //b
		b = B_FROMYU(Y,U); //v
		/* rgb888 */
		color = (r << 16) | (g <<8) | (b);
		*output_pt++ = color;

		r = R_FROMYV(Y1,V);
		g = G_FROMYUV(Y1,U,V); //b 
		b = B_FROMYU(Y1,U); //v
		color = (r << 16) | (g <<8) | (b);
		*output_pt++ = color;
		
	}
	
	return 0;
} 
static int Yuv2RgbCovert (PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut)
{
	PT_PixelDatas ptPixDataIn = &ptVideoBufIn->tPixelDatas;
	PT_PixelDatas ptPixDataOut = &ptVideoBufOut->tPixelDatas;

	ptPixDataOut->iWidth = ptPixDataIn->iWidth;
	ptPixDataOut->iHeight = ptPixDataIn->iHeight;

	if(ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB565)
	{
		ptPixDataOut->iBpp = 16;
		ptPixDataOut->iLineBytes = ptPixDataOut->iWidth * ptPixDataOut->iBpp /8;
		ptPixDataOut->iTotalBytes = ptPixDataOut->iLineBytes * ptPixDataOut->iHeight;

		if(!ptPixDataOut->aucPixelDatas)
		{
			ptPixDataOut->aucPixelDatas= malloc(ptPixDataOut->iTotalBytes);
		}
		Pyuv422torgb565(ptPixDataIn->aucPixelDatas, ptPixDataOut->aucPixelDatas, ptPixDataOut->iWidth, ptPixDataOut->iHeight);
	}
	else if(ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB32)
	{
		ptPixDataOut->iBpp = 32;
		ptPixDataOut->iLineBytes = ptPixDataOut->iWidth * ptPixDataOut->iBpp /8;
		ptPixDataOut->iTotalBytes = ptPixDataOut->iLineBytes * ptPixDataOut->iHeight;
		
		if(!ptPixDataOut->aucPixelDatas)
		{
			ptPixDataOut->aucPixelDatas= malloc(ptPixDataOut->iTotalBytes);
		}
		Pyuv422torgb32(ptPixDataIn->aucPixelDatas, ptPixDataOut->aucPixelDatas, ptPixDataOut->iWidth, ptPixDataOut->iHeight);
	}

	return 0;
}


static int Yuv2RgbConvertExit (PT_VideoBuf ptVideoBufOut)
{

	if(ptVideoBufOut->tPixelDatas.aucPixelDatas)
	{
		free(ptVideoBufOut->tPixelDatas.aucPixelDatas);
		ptVideoBufOut->tPixelDatas.aucPixelDatas = NULL;
	}
	return 0;
}



/*构造一个结构体*/
static T_VideoConvert  g_tYuv2RgbConvert = {
	.isSupport 	= Yuv2RgbisSupport,
	.Convert   	= Yuv2RgbCovert,
	.ConvertExit= Yuv2RgbConvertExit,
};

/*注册*/
int Yuv2RgbInit(void)
{
	initLut();
	return RegisterVideoConvert(&g_tYuv2RgbConvert);
}
