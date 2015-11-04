

#include <config.h>
#include <video_manager.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <poll.h>


static int g_aiSupported_Format[] = {V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_RGB565};

static int V4L2GetFrameForReadWrite(PT_VideoDevice pt_VideoDevice, PT_VideoBuf ptVideoBuf);
static int V4L2PutFrameForReadWrite(PT_VideoDevice pt_VideoDevice, PT_VideoBuf ptVideoBuf)
struct T_VideoOpr g_tV4l2VideoOpr;



static int isSupportThisFormat(int iPixelFormat)
{
	int i;
	for (i = 0; i < sizeof(g_aiSupported_Format)/sizeof(g_aiSupported_Format[0]); i++)
	{
		if(g_aiSupported_Format[i] == iPixelFormat)
			return 1;
	}
	return 0;
}

/* 参考 luvcview */
/*open
 *VIOIOC_QUERYCAP  确定它是否支持视频捕捉设备，支持哪种接口(streaming/readwrite)
 *VIDIOC_ENUM_FMT  查询支持哪种格式
 *VIDIOC_S_FMT     设置摄像头使用哪种格式
 *VIDIOC_REQBUFS   申请buffer
 
 对于streaming接口
 *VIDIOC_QUERYBUF  确定每一个buffer的信息
 *mmap
 *VIDIOC_QBUF	    放入队列
 *VIDOC_STREAMON  启动设备
 * poll			   等待有数据
 *VIDIOC_DQBUF	   从队列里取出
 *处理....
 *VIDIOC_QBUF	   放入队列
 *.....
 
 对于read,write接口
 	read
 	处理。。。
 	read
 	处理。。。
 	
 *VIDIOC_STREAMOFF 停止设备
 */
static int V4L2InitDevice (char *strDevName, PT_VideoDevice pt_VideoDevice)
{
	int i;
	int iFd;
	int iError;
	struct v4l2_capability tV4l2Cap;
	struct v4l2_fmtdesc tFmtDesc;
	struct v4l2_format	tV4l2Fmt;
	struct v4l2_requestbuffers tV4l2ReqBufs;
	struct v4l2_buffer tV42lBuf;
		
	int iLcdWidth,iLcdHeight,iLcdBpp;
	
	iFd = open(strDevName, O_RDWR);
	if(iFd < 0)
	{
		DBG_PRINTF ("can not open %s \n", strDevName);
		return -1;
	}
	pt_VideoDevice->iFd = iFd; 
	
    iError = ioctl(vd->fd, VIDIOC_QUERYCAP, &tV4l2Cap);
	memset(&tV4l2Cap, 0, sizeof(v4l2_capability));
    iError = ioctl(&tV4l2Cap, VIDIOC_QUERYCAP, &tV4l2Cap);
    if (iError) {
		DBG_PRINTF("Error opening device %s: unable to query device.\n", strDevName);
		goto err_exit;
    }

	if(!(tV4l2Cap->capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		DBG_PRINTF(" %s: is not a capture device.\n", strDevName);
		goto err_exit;
	}
	
    if (tV4l2Cap->capabilities  & V4L2_CAP_STREAMING)
	{
	    DBG_PRINTF("%s  support streaming i/o\n", strDevName);
    }
    if (tV4l2Cap->capabilities  & V4L2_CAP_READWRITE)
	{
	    DBG_PRINTF("%s  support read i/o\n", strDevName);
    }


	memset(&tFmtDesc, 0, sizeof(tFmtDesc));
	tFmtDesc.index = 0;
	tFmtDesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while ((iError= ioctl(iFd, VIDIOC_ENUM_FMT, &tFmtDesc)) == 0) {
		if(isSupportThisFormat(tFmtDesc.pixelformat))
		{
			pt_VideoDevice->iPixFormat = tFmtDesc.pixelformat;
			break;
		}
		tFmtDesc.index++;
	}
	if(!pt_VideoDevice->iPixFormat)
	{
		DBG_PRINTF(" can not a support PixFormat of this device.\n");
		goto err_exit;
	}
	 
    /* set format in */
	GetDispResolution(&iLcdWidth, &iLcdHeight, &iLcdBpp);
    memset(&tV4l2Fmt, 0, sizeof(struct v4l2_format));
    tV4l2Fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tV4l2Fmt.fmt.pix.pixelformat = pt_VideoDevice->iPixFormat;
    tV4l2Fmt.fmt.pix.width = iLcdWidth;
    tV4l2Fmt.fmt.pix.height = iLcdHeight;
    tV4l2Fmt.fmt.pix.field = V4L2_FIELD_ANY;
	/*
	 *如果驱动程序发现某些参数(分辨率)
	 *他会调整这些参数，并且返回给应用程序
	 */
	iError= ioctl(iFd, VIDIOC_S_FMT, &tV4l2Fmt);  
	
    if (iError) {
		DBG_PRINTF("Unable to set format \n");
		goto err_exit;
    }
	pt_VideoDevice->iWidth = tV4l2Fmt.fmt.pix.width;
	pt_VideoDevice->iHeight= tV4l2Fmt.fmt.pix.height;

    /* request buffers */
    memset(&tV4l2ReqBufs, 0, sizeof(struct v4l2_requestbuffers));
    tV4l2ReqBufs->count = NB_BUFFER;
    tV4l2ReqBufs->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tV4l2ReqBufs->memory = V4L2_MEMORY_MMAP;

    ret = ioctl(iFd, VIDIOC_REQBUFS, &tV4l2ReqBufs);
    if (iError) {
		DBG_PRINTF("Unable to allocate buffers.\n");
		goto err_exit;
    }
	pt_VideoDevice->iVideoBufCnt = tV4l2ReqBufs.count;
	//mmap
	if(tV4l2Cap.cap->capabilities & V4L2_CAP_STREAMING)
	{
		/* map the buffers */
		for (i = 0; i < pt_VideoDevice->iVideoBufCnt; i++) 
		{
			memset(&tV42lBuf, 0, sizeof(struct v4l2_buffer));
			tV42lBuf.index = i;
			tV42lBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			tV42lBuf.memory = V4L2_MEMORY_MMAP;
			iError= ioctl(iFd, VIDIOC_QUERYBUF, &tV42lBuf);
			if (iError) {
				DBG_PRINTF("Unable to query buffer .\n");
				goto err_exit;
			}
			pt_VideoDevice->iVideoBufMaxLen = tV42lBuf.length;
			pt_VideoDevice->pucVideoBuf[i] = mmap(0 /* start anywhere */ ,
					  tV42lBuf.length, PROT_READ, MAP_SHARED, iFd,
					  tV42lBuf.m.offset);
			if (pt_VideoDevice->pucVideoBuf[i] == MAP_FAILED) 
			{
				DBG_PRINTF("Unable to map buffer \n");
				goto err_exit;
			}
		}

	}
	else if (tV4l2Cap->capabilities  & V4L2_CAP_READWRITE)
	{
		g_tV4l2VideoOpr.GetFrame = V4L2GetFrameForReadWrite;
		g_tV4l2VideoOpr.PutFrame = V4L2PutFrameForReadWrite;
	
		/*read(fd  buf, size)*/
		pt_VideoDevice->iVideoBufCnt = 1;
		pt_VideoDevice->iVideoBufMaxLen = pt_VideoDevice->iWidth * pt_VideoDevice->iHeight * 4;
		/*在这个程序所能支持的格式里一个像素最多需要4个字节*/
		pt_VideoDevice->pucVideoBuf[0] = malloc(pt_VideoDevice->iVideoBufMaxLen);
	}

	 /* Queue the buffers. */
    for (i = 0; i < pt_VideoDevice->iVideoBufCnt; i++) 
	{
		memset(&tV42lBuf, 0, sizeof(struct v4l2_buffer));
		tV42lBuf.index = i;
		tV42lBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		tV42lBuf.memory = V4L2_MEMORY_MMAP;
		iError= ioctl(iFd, VIDIOC_QBUF, &tV42lBuf);
		if (iError) {
		    DBG_PRINTF("Unable to queue buffer\n");
		    goto err_exit;;
		}
    }

	return 0;
err_exit:
	close(iFd);
	return -1;
}
static int V4L2ExitDevice(PT_VideoDevice pt_VideoDevice)
{
	int i;
	close(pt_VideoDevice->iFd);

	for (i = 0; i < pt_VideoDevice->iVideoBufCnt; i++) 
	{
		if(pt_VideoDevice->pucVideoBuf[i])
		{
			munmap(pt_VideoDevice->pucVideoBuf[i], pt_VideoDevice->iVideoBufMaxLen);
			pt_VideoDevice->pucVideoBuf[i] = NULL;
		}
	}
	return 0;
}

static int V4L2GetFrameForStreaming(PT_VideoDevice pt_VideoDevice, PT_VideoBuf ptVideoBuf)
{
	struct pollfd tFds[1];
	int iRet;
	struct v4l2_buffer tV4l2Buf;

	/*poll*/
	tFds[0].fd = pt_VideoDevice.iFd;
	tFds[0].event = POLLIN;

	iRet = poll(tFds, 1, -1);
	if(iRet <=0 )
	{
		DBG_PRINTF("poll error!\n");
		return -1;
	}

	/*VIDIOC_DQBUF*/
    memset(&tV4l2Buf, 0, sizeof(struct v4l2_buffer));
    tV4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tV4l2Buf.memory = V4L2_MEMORY_MMAP;
    iRet = ioctl(pt_VideoDevice->iFd, VIDIOC_DQBUF, &tV4l2Buf);
    if (iRet < 0) 
	{
		DBG_PRINTF("Unable to dequeue buffer .\n");
		return -1;
    }
	pt_VideoDevice->iVideoBufCurIndex = tV4l2Buf.index;
	ptVideoBuf->iPixelFormat = pt_VideoDevice->iPixFormat;
	ptVideoBuf->tPixelDatas.iWidth  = pt_VideoDevice->iWidth;
	ptVideoBuf->tPixelDatas.iHeight = pt_VideoDevice->iHeight;
	ptVideoBuf->tPixelDatas.iBpp    = (pt_VideoDevice->iPixFormat==V4L2_PIX_FMT_YUYV)? 16 : \
										(pt_VideoDevice->iPixFormat==V4L2_PIX_FMT_MJPEG)?:0 \
										 (pt_VideoDevice->iPixFormat==V4L2_PIX_FMT_RGB565):16;
	ptVideoBuf->tPixelDatas.iLineBytes  = ptVideoBuf->tPixelDatas.iWidth*ptVideoBuf->tPixelDatas.iBpp/8;
	ptVideoBuf->tPixelDatas.iTotalBytes = tV4l2Buf->byteused;
	ptVideoBuf->tPixelDatas.aucPixelDatas= pt_VideoDevice->pucVideoBuf[tV4l2Buf.index];

	
	return 0;
}

static int V4L2PutFrameForStreaming(PT_VideoDevice pt_VideoDevice, PT_VideoBuf ptVideoBuf)
{
	struct v4l2_buffer tV4l2Buf;
	int iError;

	/*VIDIOC_QBUF*/
    /* Queue the buffers. */
    for (i = 0; i < NB_BUFFER; ++i) {
		memset(&tV4l2Buf, 0, sizeof(struct v4l2_buffer));
		tV4l2Buf.index  = pt_VideoDevice->iVideoBufCurIndex;
		tV4l2Buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		tV4l2Buf.memory = V4L2_MEMORY_MMAP;
		iError = ioctl(pt_VideoDevice->iFd, VIDIOC_QBUF, &tV4l2Buf);
		if (iError) 
		{
		    DBG_PRINTF("Unable to queue buffer).\n");
			return -1;
		}
    }
	return 0;
}
static int V4L2GetFrameForReadWrite(PT_VideoDevice pt_VideoDevice, PT_VideoBuf ptVideoBuf)
{
	int iRet;

	iRet = read(pt_VideoDevice->iFd, pt_VideoDevice->pucVideoBuf[0], pt_VideoDevice->iVideoBufMaxLen);
	if(iRet<=0)
	{
		DBG_PRINTF("can not read data\n");
		return -1;
	}
	
	ptVideoBuf->iPixelFormat = pt_VideoDevice->iPixFormat;
	ptVideoBuf->tPixelDatas.iWidth  = pt_VideoDevice->iWidth;
	ptVideoBuf->tPixelDatas.iHeight = pt_VideoDevice->iHeight;
	ptVideoBuf->tPixelDatas.iBpp    = (pt_VideoDevice->iPixFormat==V4L2_PIX_FMT_YUYV)? 16 : \
										(pt_VideoDevice->iPixFormat==V4L2_PIX_FMT_MJPEG)?:0 \
										 (pt_VideoDevice->iPixFormat==V4L2_PIX_FMT_RGB565):16;
	ptVideoBuf->tPixelDatas.iLineBytes  = ptVideoBuf->tPixelDatas.iWidth*ptVideoBuf->tPixelDatas.iBpp/8;
	ptVideoBuf->tPixelDatas.iTotalBytes = iRet;
	ptVideoBuf->tPixelDatas.aucPixelDatas= pt_VideoDevice->pucVideoBuf[0];

	return 0;
}

static int V4L2PutFrameForReadWrite(PT_VideoDevice pt_VideoDevice, PT_VideoBuf ptVideoBuf)
{
	return 0;
}

static int V4L2StartDevice(PT_VideoDevice pt_VideoDevice)
{
    int iType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int iError;

    iError = ioctl(pt_VideoDevice->iFd, VIDIOC_STREAMON, &type);
    if (iError )
	{
		printf("Unable to start capture .\n" );
		return -1;
    }

	return 0;
}

static int V4L2StopDevice(PT_VideoDevice pt_VideoDevice)
{
    int iType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int iError;

    iError = ioctl(pt_VideoDevice->iFd, VIDIOC_STREAMOFF, &type);
    if (iError )
	{
		printf("Unable to stop capture .\n" );
		return -1;
    }
	return 0;
}

/*构造一个VideoOps结构体*/
struct T_VideoOpr g_tV4l2VideoOpr = {
		.name 			= "V4L2",
		.InitDevice 	= V4L2InitDevice,
		.ExitDevice		= V4L2ExitDevice,
		.GetFrame		= V4L2GetFrameForStreaming,
		.PutFrame		= V4L2PutFrameForStreaming,
		.StartDevice	= V4L2StartDevice,
		.StopDevice		= V4L2StopDevice,
};


/*注册这个结构体*/
int V4l2Init(void)
{
	return RegisterVideoOpr(&g_tV4l2VideoOpr);
}


