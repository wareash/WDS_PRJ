

#include <config.h>
#include <video_manager.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

static int g_aiSupported_Format[] = {V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_RGB565, V4L2_PIX_FMT_RGB24, V4L2_PIX_FMT_RGB32};


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

/* �ο� luvcview */
/*open
 *VIOIOC_QUERYCAP  ȷ�����Ƿ�֧����Ƶ��׽�豸��֧�����ֽӿ�(streaming/readwrite)
 *VIDIOC_ENUM_FMT  ��ѯ֧�����ָ�ʽ
 *VIDIOC_S_FMT     ��������ͷʹ�����ָ�ʽ
 *VIDIOC_REQBUFS   ����buffer
 
 ����streaming�ӿ�
 *VIDIOC_QUERYBUF  ȷ��ÿһ��buffer����Ϣ
 *mmap
 *VIDIOC_QBUF	    �������
 *VIDOC_STREAMON  �����豸
 * poll			   �ȴ�������
 *VIDIOC_DQBUF	   �Ӷ�����ȡ��
 *����....
 *VIDIOC_QBUF	   �������
 *.....
 
 ����read,write�ӿ�
 	read
 	��������
 	read
 	��������
 	
 *VIDIOC_STREAMOFF ֹͣ�豸
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
	 *�������������ĳЩ����(�ֱ���)
	 *���������Щ���������ҷ��ظ�Ӧ�ó���
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
		for (i = 0; i < NB_BUFFER; i++) {
			memset(&tV42lBuf, 0, sizeof(struct v4l2_buffer));
			tV42lBuf.index = i;
			tV42lBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			tV42lBuf.memory = V4L2_MEMORY_MMAP;
			iError= ioctl(iFd, VIDIOC_QUERYBUF, &tV42lBuf);
			if (iError) {
				DBG_PRINTF("Unable to query buffer .\n");
				goto err_exit;
			}

			pt_VideoDevice->pucVideoBuf[i] = mmap(0 /* start anywhere */ ,
					  tV42lBuf, PROT_READ, MAP_SHARED, iFd,
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
		/*read(fd  buf, size)*/
		pt_VideoDevice->iVideoBufCnt = 1;
		/*�������������֧�ֵĸ�ʽ��һ�����������Ҫ4���ֽ�*/
		pt_VideoDevice->pucVideoBuf[0] = malloc(pt_VideoDevice->iWidth * pt_VideoDevice->iHeight * 4);
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

	for(i = 0; i<)
	return 0;
}

static int V4L2GetFrame(PT_VideoDevice pt_VideoDevice, PT_VideoBuf ptVideoBuf)
{
	return 0;
}

static int V4L2PutFrame(PT_VideoDevice pt_VideoDevice, PT_VideoBuf ptVideoBuf)
{
	return 0;
}

static int V4L2StartDevice(PT_VideoDevice pt_VideoDevice)
{
	return 0;
}

static int V4L2StopDevice(PT_VideoDevice pt_VideoDevice)
{
	return 0;
}

/*����һ��VideoOps�ṹ��*/
struct T_VideoOpr g_tV4l2VideoOpr = {
		.name 			= "V4L2",
		.InitDevice 	= V4L2InitDevice,
		.ExitDevice		= V4L2ExitDevice,
		.GetFrame		= V4L2GetFrame,
		.PutFrame		= V4L2PutFrame,
		.StartDevice	= V4L2StartDevice,
		.StopDevice		= V4L2StopDevice,
};


/*ע������ṹ��*/
int V4l2Init(void)
{
	return RegisterVideoOpr(&g_tV4l2VideoOpr);
}


