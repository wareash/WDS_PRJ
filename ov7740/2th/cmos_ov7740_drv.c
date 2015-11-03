#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/usb.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <asm/atomic.h>
#include <asm/unaligned.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf-core.h>

#include "uvcvideo.h"

typedef struct cmos_ov7740_i2c_value{
	unsigned char regaddr;
	unsigned char value;
}ov7740_t;

ov7740_t ov7740_setting_30fps_vga_640_480[]=
{
	{0x12, 0x80},
	{0x47, 0x02},
	{0x17, 0x27},
	{0x04, 0x40},
	{0x1B, 0x81},
	{0x29, 0x17},
	{0x5F, 0x03},
	{0x3A, 0x09},
	{0x33, 0x44},
	{0x68, 0x1A},

	{0x14, 0x38},
	{0x5F, 0x04},
	{0x64, 0x00},
	{0x67, 0x90},
	{0x27, 0x80},
	{0x45, 0x41},
	{0x4B, 0x40},
	{0x36, 0x2f},
	{0x11, 0x01},
	{0x36, 0x3f},
	{0x0c, 0x12},

	{0x12, 0x00},
	{0x17, 0x25},
	{0x18, 0xa0},
	{0x1a, 0xf0},
	{0x31, 0xa0},
	{0x32, 0xf0},

	{0x85, 0x08},
	{0x86, 0x02},
	{0x87, 0x01},
	{0xd5, 0x10},
	{0x0d, 0x34},
	{0x19, 0x03},
	{0x2b, 0xf8},
	{0x2c, 0x01},

	{0x53, 0x00},
	{0x89, 0x30},
	{0x8d, 0x30},
	{0x8f, 0x85},
	{0x93, 0x30},
	{0x95, 0x85},
	{0x99, 0x30},
	{0x9b, 0x85},

	{0xac, 0x6E},
	{0xbe, 0xff},
	{0xbf, 0x00},
	{0x38, 0x14},
	{0xe9, 0x00},
	{0x3D, 0x08},
	{0x3E, 0x80},
	{0x3F, 0x40},
	{0x40, 0x7F},
	{0x41, 0x6A},
	{0x42, 0x29},
	{0x49, 0x64},
	{0x4A, 0xA1},
	{0x4E, 0x13},
	{0x4D, 0x50},
	{0x44, 0x58},
	{0x4C, 0x1A},
	{0x4E, 0x14},
	{0x38, 0x11},
	{0x84, 0x70}
};
struct cmos_ov7740_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

static struct cmos_ov7740_fmt formats[] = {
	{
		.name     = "RGB565",
		.fourcc   = V4L2_PIX_FMT_RGB565, /* gggbbbbb rrrrrggg */
		.depth    = 16,
	},
	{
		.name     = "PACKED_RGB_888",
		.fourcc   = V4L2_PIX_FMT_RGB24,
		.depth    = 24,
	}
};

struct camif_buf{
	unsigned int order;
	unsigned long virt_base;
	unsigned long phy_base;
};

struct camif_buf img_buff[] = 
{
	{
		.order = 0,
		.virt_base = (unsigned long )NULL,
		.phy_base = (unsigned long )NULL
	},
	{
		.order = 0,
		.virt_base = (unsigned long )NULL,
		.phy_base = (unsigned long )NULL
	},
	{
		.order = 0,
		.virt_base = (unsigned long )NULL,
		.phy_base = (unsigned long )NULL
	},
	{
		.order = 0,
		.virt_base = (unsigned long )NULL,
		.phy_base = (unsigned long )NULL
	}
};

#define OV7740_INIT_REGS_SIZE = (sizeof(ov7740_setting_30fps_vga_640_480) / sizeof(ov7740_setting_30fps_vga_640_480[0]))
#define CAM_SRC_HSIZE (640)
#define CAM_SRC_VSIZE (480)
#define CAM_ORDER_YCbYCr (0)
#define CAM_ORDER_YCrYCb (1)
#define CAM_ORDER_CbYCrY (2)
#define CAM_ORDER_CrYCbY (3)

#define WinHorOfst (0)
#define WinVerOfst (0)

struct cmos_ov7740_scaler{
	unsigned int PreHorRatio;
	unsigned int PreVerRatio;
	unsigned int H_Shift;
	unsigned int V_Shift;
	unsigned int PreDstWidth;	
	unsigned int PreDstHeight;
	unsigned int PreVerRatio;
	unsigned int MainHorRatio;
	unsigned int MainVerRatio;
	unsigned int SHfactor;
	unsigned int ScaleUpDown;
};
static cmos_ov7740_scaler sc;

struct i2c_client *cmos_ov7740_client;

//CAMIF GPIO
static unsigned long *GPJCON;
static unsigned long *GPJDAT;
static unsigned long *GPJUP;

//CPU CAM 接口控制寄存器
static unsigned long *CISRCFMT;
static unsigned long *CIWDOFST;
static unsigned long *CIGCTRL;
static unsigned long *CIPRCLRSA1;
static unsigned long *CIPRCLRSA2;
static unsigned long *CIPRCLRSA3;
static unsigned long *CIPRCLRSA4;
static unsigned long *CIPRTRGFMT;
static unsigned long *CIPRCTRL;
static unsigned long *CIPRSCPRERAIO;
static unsigned long *CIPRSCPREST;
static unsigned long *CIPRSCCTRL;
static unsigned long *CIPRTAREA;
static unsigned long *CIIMGCPT;

//IRQ 寄存器
static unsigned long *SRC_PND;
static unsigned long *INT_PND;
static unsigned long *SUBSRC_PND;


static unsigned int SRC_Width,SRC_Height;
static unsigned int TargetHsize_Pr;
static unsigned int TargetVsize_Pr;
unsigned long buf_size;
static unsigned int bytesperline;
//static unsigned int Main_burst;
//static unsigned int Remained_burst;


static DECLARE_WAIT_QUEUE_HEAD(cam_wait_queue);
/*中断标志*/
static volatile int ev_cam = 0;

static irqreturn_t cmos_ov7740_camif_irq_c(int irq, void *dev_id)
{
	return IRQ_HANDLED;

}
static irqreturn_t cmos_ov7740_camif_irq_p(int irq, void *dev_id)
{
	/*清中断*/
	*SRC_PND = (1<<6);
	*INT_PND = (1<<6);
	*SUBSRC_PND = (1<<12);
	
	ev_cam = 1;
	wake_up_interruptible(&cam_wait_queue);
	
	return IRQ_HANDLED;
}


/* A2 参考 uvc_v4l2_do_ioctl */
static int cmos_ov7740_vidioc_querycap(struct file *file, void  *priv,
					struct v4l2_capability *cap)
{    
    memset(cap, 0, sizeof *cap);
    strcpy(cap->driver, "cmos_ov7740");
    strcpy(cap->card, "cmos_ov7740");
    cap->version = 2;
    
    cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_READWRITE;
	return 0;
}
/* A3 列举支持哪种格式
 * 参考: uvc_fmts 数组
 */
static int cmos_ov7740_vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	struct cmos_ov7740_fmt *fmt;

	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	fmt = &formats[f->index];

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}
/* A4 返回当前所使用的格式 */
static int cmos_ov7740_vidioc_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	return (0);
}

/* A5 测试驱动程序是否支持某种格式, 强制设置该格式 
 * 参考: uvc_v4l2_try_format
 *       myvivi_vidioc_try_fmt_vid_cap
 */
static int cmos_ov7740_vidioc_try_fmt_vid_cap(struct file *file, void *priv,
			struct v4l2_format *f)
{
 
 if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
 {
	 return -EINVAL;
 }
 
 if ((f->fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565) && (f->fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24))
	 return -EINVAL;

    return 0;
}

/* A6 参考 myvivi_vidioc_s_fmt_vid_cap */
static int cmos_ov7740_vidioc_s_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{    
	
	int ret = cmos_ov7740_vidioc_try_fmt_vid_cap(file, NULL, f);
	if (ret < 0)
		return ret;
	
	/* 调整format的width, height, 
	 * 计算bytesperline, sizeimage
	 */
	
	/* 人工查看描述符, 确定支持哪几种分辨率 */
	TargetHsize_Pr = f->fmt.pix.width;
	TargetVsize_Pr = f->fmt.pix.height;

	if (f->fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565) 
	{
		*CIPRSCCTRL   &= ~(1<<30);
		
		f->fmt.pix.bytesperline = (f->fmt.pix.width * 16) >> 3;
		f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;		
		buf_size = f->fmt.pix.bytesperline * f->fmt.pix.sizeimage;
		bytesperline = f->fmt.pix.bytesperline ;
	}
	else if (f->fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24) 
	{
		
		*CIPRSCCTRL   |= (1<<30);
	
		f->fmt.pix.bytesperline = (f->fmt.pix.width * 32) >> 3;
		f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;		
		buf_size = f->fmt.pix.bytesperline * f->fmt.pix.sizeimage;
		bytesperline = f->fmt.pix.bytesperline ;
	}
	/*
	CIPRTRGFMT
	*bit[16:28]  --表示目标图片的水平像素大小 TargetHsize_Pr
	*bit[14:15]  --是否对图片进行旋转,我们不旋转
	*bit[0:12]   --表示目标图片的垂直像素大小 TargetVsize_Pr
	*/
 	*CIPRTRGFMT =  (TargetHsize_Pr <<16) | (0<<14) | (TargetVsize_Pr<<0);
    return 0;
}

/* A7 APP调用该ioctl让驱动程序分配若干个缓存, APP将从这些缓存中读到视频数据 
 * 参考: uvc_alloc_buffers
 */
static int cmos_ov7740_vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	unsigned int order;
	order = get_order(buf_size);

	img_buff[0].order = order;
	img_buff[0].virt_base = __get_free_pages(GFP_KERNEL | __GFP_DMA, img_buff[0].order);
	if(img_buff[0].virt_base  == (unsigned long)NULL)
	{
		goto error0;
	}
	img_buff[0].phy_base  = __virt_to_phys(img_buff[0].virt_base);

	
	img_buff[1].order = order;
	img_buff[1].virt_base = __get_free_pages(GFP_KERNEL | __GFP_DMA, img_buff[1].order);
	if(img_buff[1].virt_base  == (unsigned long)NULL)
	{
		goto error1;
	}
	img_buff[1].phy_base  = __virt_to_phys(img_buff[1].virt_base);

	img_buff[2].order = order;
	img_buff[2].virt_base = __get_free_pages(GFP_KERNEL | __GFP_DMA, img_buff[2].order);
	if(img_buff[2].virt_base  == (unsigned long)NULL)
	{
		goto error2;
	}
	img_buff[2].phy_base  = __virt_to_phys(img_buff[2].virt_base);

	img_buff[3].order = order;
	img_buff[3].virt_base = __get_free_pages(GFP_KERNEL | __GFP_DMA, img_buff[3].order);
	if(img_buff[3].virt_base  == (unsigned long)NULL)
	{
		goto error3;
	}
	img_buff[3].phy_base  = __virt_to_phys(img_buff[3].virt_base);

	*CIPRCLRSA1 = img_buff[0].phy_base;
	*CIPRCLRSA2 = img_buff[1].phy_base;
	*CIPRCLRSA3 = img_buff[2].phy_base;
	*CIPRCLRSA4 = img_buff[3].phy_base;

	
error3:
	free_pages(img_buff[2].virt_base, order);
	img_buff[2].phy_base = (unsigned long)NULL;	
error2:
	free_pages(img_buff[1].virt_base, order);
	img_buff[1].phy_base = (unsigned long)NULL;
error1:
	free_pages(img_buff[0].virt_base, order);
	img_buff[0].phy_base = (unsigned long)NULL;
error0:
    return -ENOMEM;
	
}


static void CalculateBurstSize(unsigned int hSize, unsigned int *mainBurstSize, unsigned int *remainedBurstSize)
{
	unsigned int tmp;

	tmp = (hSize/4)%16; //hSize/4 = 一行占多少字
	switch(tmp)
	{
		case 0:
			*mainBurstSize = 16;
			*remainedBurstSize = 16;
			break;
		case 4:
			*mainBurstSize = 16;
			*remainedBurstSize = 4;
			break;
		case 8:
			*mainBurstSize = 16;
			*remainedBurstSize = 8;
			break;
		default:
			tmp = (hSize/4)%8; //hSize/4 = 一行占多少字s
			switch(tmp)
			{
				case 0:
					*mainBurstSize = 8;
					*remainedBurstSize = 8;
					break;
				case 4:
					*mainBurstSize = 8;
					*remainedBurstSize = 4;
					break;
				default:
					*mainBurstSize = 4;
					tmp = (hSize/4)%4;
					*remainedBurstSize = (tmp)? tmp : 4;
					break
			}
			break;
	}
	
}
static void camif_get_scaler_factor(u32 src, u32 tar, u32 *ratio, u32 *shift)
{
	If ( src >= 64 * tar ) { return -EINVAL;} 
	else if (src >= 32 * tar) { *ratio = 32; *shift = 5; } 
	else if (src >= 16 * tar) { *ratio = 16; *shift = 4; } 
	else if (src >= 8  * tar) { *ratio = 8;  *shift = 3; } 
	else if (src >= 4  * tar) { *ratio = 4;  *shift = 2; } 
	else if (src >= 2  * tar) { *ratio = 2;  *shift = 1; } 
	else { *ratio = 1; *shift = 0; } 
}
static void cmos_ov7740_calculate_scaler_info(void)
{
	unsigned int sx,sy,tx,ty;

	sx = SRC_Width;
	sy = SRC_Height;
	tx = TargetHsize_Pr;
	ty = TargetVsize_Pr;

	printk("%s:SRC_in(%d,%d),Target_out(%d,%d) \n",__func__, sx, sy, tx, ty);

	camif_get_scaler_factor(sx,tx,&sc.PreHorRatio,&sc.H_Shift);
	camif_get_scaler_factor(sy,ty,&sc.PreVorRatio,&sc.V_Shift);

	sc.PreDstWidth = sx / sc.PreHorRatio; 
	sc.PreDstHeight= sy / sc.PreVerRatio;
	sc.MainHorRatio= ( sx << 8 ) / ( tx << sc.H_Shift); 
	sc.MainVerRatio= ( sy << 8 ) / ( ty << sc.V_Shift);

	sc.SHfactor = 10 - (sc.H_Shift + sc.V_Shift);

	sc.ScaleUpDown = (tx > sx)? 1:0;
	
}

/* A11 启动传输 
 * 参考: uvc_video_enable(video, 1):
 *           uvc_commit_video
 *           uvc_init_video
 */
static int cmos_ov7740_vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	unsigned int Main_burst, Remained_burst;
	/*
	 *开始摄像头采集时候，设置相应寄存器 stream on
	 *CISRCFMT bit[31] --选择传输方式是BT601 或 656
	 *bit[30] --设置偏移值(=0 YCbCr)
	 *bit[29] --必须设置为0
	 *bit[16 - 28] --设置原图片的水平像素值 = 640
	 *bit[14 - 15] --设置原图片的颜色顺序(ox0c cbYcY) =0x02
	 *bit[0  - 12] --设置原图片的垂直像素值 =480
	 */
	 *CISRCFMT |= (0<<30) | (0<<29) | (CAM_SRC_HSIZE<<16) | (CAM_ORDER_CbYCrY << 14) | (CAM_SRC_VSIZE<<0);

	/*
	 *CIWDOFST 窗口相关寄存器
	 *bit[31] --1  = 是能窗口功能
	 *bit[30 - 15:12] -清除溢出标志位
	 *bit[16-26]--水平方向上裁剪的大小
	 *bit[0 - 10] --垂直方向上裁剪的大小
	 */
     *CIWDOFST |= (1<<30) | (0xf<<12);
	 *CIWDOFST |= (1<<31) | (WinHorOfst << 16) | (WinVerOfst <<0 );
     SRC_Width = CAM_SRC_HSIZE-WinHorOfst*2;
	 SRC_Height= CAM_SRC_VSIZE-WinVerOfst*2;

	 /*
	  *CIGCTRL:
	  *bit[31] --软件复位摄像头控制器
	  *bit[30] --用于复位外部摄像头模块
	  *bit[29] --保留位，必须设置为1
	  *bit[27:28] --选择信号源 (00 - 输入源来至于摄像头模块)
	  *bit[26] --设置像素时钟的极性 -猜0
	  *bit[25] --设置帧同步信号VSYNC的极性 -0
	  *bit[24] --设置HREF的极性 -0
 	  */
	  *CIGCTRL |= (1<<29) | (0x00 << 27) | (0<<26) | (0<<25) | (0<<24);
	 
	 /*CIPRCTRL
	  *bit[19:23]  -- 主突发长度 (Main_burst)
	  *bit[14:18]  -- 剩余突发长度 (Remained_burst)
	  *bit[2]      -- 是否使能last_IRQ(不使能)
	  */
	CalculateBurstSize(bytesperline, &Main_burst, &Remained_burst);
	*CIPRCTRL = (Main_burst<<19) | (Remained_burst<<14) | (0<<2);
	
	/*CIPRSCPRERAIO
	*bit[28:31] ---表示预览缩放的变化系数 SHfactor_Pr
	*bit[16:22] ---预览缩放的水平比  PreHorRatio_Pr
	*bit[0: 6] ----预览缩放的垂直比  PreVerRatio_Pr

	*CIPRSCPREST
	*bit[16:27] --- 表示预览缩放的目标宽度  PreDstWidth_Pr
	*bit[0 :11] --- 表示预览缩放的目标高度  PreDstHeight_Pr
	
	*CIPRSCCTRL
	*bit[31]  必须设置为1 :Sample_Pr 
	*bit[30]  设置图像输出格式是RGB16或者RBG24  RGBformat_Pr :
	*bit[28:29]  告诉摄像头控制器图片是缩小还是放大 ScaleUpDown_Pr
	*bit[16:24]  预览主缩放的水平比  MainHorRatio_Pr
	*bit[15]  预览缩放开始  PrScalerStart
	*bit[0:8]  预览主缩放的垂直比  MainVerRatio_Pr
	*/

	cmos_ov7740_calculate_scaler_info();
	*CIPRSCPRERAIO = (sc.SHfactor << 28) | (sc.PreVerRatio << 16) | (sc.PreVerRatio << 0);
	*CIPRSCPREST   = (sc.PreDstWidth << 16) | (sc.PreDstHeight << 0);
	*CIPRSCCTRL   |= (1<<31) | (sc.ScaleUpDown << 28) | (sc.MainHorRatio << 16) | (sc.MainVerRatio << 0);

	/*
	 *CIPRTAREA
	 *表示预览通道的目标区域
	 */
	*CIPRTAREA = (TargetHsize_Pr << 15);

	/*
	*CIIMGCPT
	*bit[31] 使能摄像头控制器
	*bit[30] 使能编码通道
	*bit[29] 使能预览通道
	*
	*/
	*CIIMGCPT = (1 << 31) | (1 << 29);
	*CIPRSCCTRL   |= (1<<15);
	
	return 0;
}

/*
 * A14 之前已经通过mmap映射了缓存, APP可以直接读数据
 * A15 再次调用myuvc_vidioc_qbuf把缓存放入队列
 * A16 poll...
 */

/* A17 停止 
 * 参考 : uvc_video_enable(video, 0)
 */
static int cmos_ov7740_vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type t)
{    
	*CIPRSCCTRL   &= ~((1<<15));
	*CIIMGCPT &= ~((1 << 31) | (1 << 29));
	
    return 0;
}


static const struct v4l2_ioctl_ops cmos_ov7740_ioctl_ops = {
        // 表示它是一个摄像头设备
        .vidioc_querycap      = cmos_ov7740_vidioc_querycap,

        /* 用于列举、获得、测试、设置摄像头的数据的格式 */
        .vidioc_enum_fmt_vid_cap  = cmos_ov7740_vidioc_enum_fmt_vid_cap,
        .vidioc_g_fmt_vid_cap     = cmos_ov7740_vidioc_g_fmt_vid_cap,
        .vidioc_try_fmt_vid_cap   = cmos_ov7740_vidioc_try_fmt_vid_cap,
        .vidioc_s_fmt_vid_cap     = cmos_ov7740_vidioc_s_fmt_vid_cap,
        
        /* 缓冲区操作: 申请/查询/放入队列/取出队列 */
        .vidioc_reqbufs       = cmos_ov7740_vidioc_reqbufs,

		/*说明: 我们是通过读的方式获取摄像头的数据，因此查询/放入队列/取出队列这些操作函数将不需要*/
#if 0
        .vidioc_querybuf      = cmos_ov7740_vidioc_querybuf,
        .vidioc_qbuf          = cmos_ov7740_vidioc_qbuf,
        .vidioc_dqbuf         = cmos_ov7740_vidioc_dqbuf,
#endif        
        // 启动/停止
        .vidioc_streamon      = cmos_ov7740_vidioc_streamon,
        .vidioc_streamoff     = cmos_ov7740_vidioc_streamoff,   
};

/* A1 */
static int cmos_ov7740_open(struct file *file)
{
	return 0;
}
/* A18 关闭 */
static int cmos_ov7740_close(struct file *file)
{
    
	return 0;
}

/*应用程序通过读的方式读取摄像头数据*/
static ssize_t cmos_ov7740_read (struct file *file, char __user *buf, size_t count, loff_t *ops)
{
	size_t end;
	int i;

	end = min_t(size_t, buf_size, count);

	wait_event_interruptible(cam_wait_queue, ev_cam);

	for(i=0; i<4; i++)
	{
		if(copy_to_user(buf, img_buff[i].virt_base, end))
			return -EFAULT;
	}
	ev_cam = 0;

	return 0;
}

static const struct v4l2_file_operations cmos_ov7740_fops = {
	.owner				= THIS_MODULE,
    .open       		= cmos_ov7740_open,
    .release    		= cmos_ov7740_close,
    .unlocked_ioctl    	= video_ioctl2, /* V4L2 ioctl handler */
    .read				= cmos_ov7740_read,
};


static void cmos_ov7740_release(struct video_device *vdev)
{
	unsigned int order;
	order = get_order(buf_size);

	free_pages(img_buff[0].virt_base, order);
	img_buff[0].phy_base = (unsigned long)NULL;

	free_pages(img_buff[1].virt_base, order);
	img_buff[1].phy_base = (unsigned long)NULL;

	free_pages(img_buff[2].virt_base, order);
	img_buff[2].phy_base = (unsigned long)NULL; 

	free_pages(img_buff[3].virt_base, order);
	img_buff[3].phy_base = (unsigned long)NULL;

}


/* 2.1 分配设置一个video_device结构体 */
static struct video_device cmos_ov7740_vdev = {
	.name		= "cmos_ov7740",
	.release 	= cmos_ov7740_release,
	.fops 		= &cmos_ov7740_fops,
	.ioctl_ops	= &cmos_ov7740_ioctl_ops,
};
/*****************************************************************/
/*设置相关的GPIO用于CAMIF*/
static void coms_ov7740_gpio_cfg(void)
{
	*GPJCON = 0x2aaaaaa;
	*GPJDAT = 0;

	/*使能上拉电子*/
	*GPJUP = 0；
	
}

/*复位一下CAMIF控制器*/
static void cmos_ov7740_camif_reset(void)
{	
	 *CIGCTRL |= (1<<31);
	 mdelay(10);
	 *CIGCTRL &= ~(1<<31);
	 mdelay(10);

}
/*设置相关的GPIO用于CAMIF*/
static void cmos_ov7740_clk_cfg(void)
{	
	struct clk *camif_clk;
	struct clk *camif_upll_clk;
	
	/*使能CAMIF的时钟源*/
	camif_clk = clk_get(NULL, "comsif");
	if(camif_clk || IS_ERR(camif_clk))
	{
		printk(KERN_INFO"failed to get CAMIF source\n");
	}
	clk_enable(camif_clk);

	/*使能并设置CAMCLK = 24Mhz*/
	camif_upll_clk = clk_get(NULL, "camif-upll");
	clk_set_rate(camif_upll_clk, 24000000);
	mdelay(100);
}
/*
 *复位一下摄像头模块
 *1.s3c2440提供的复位时序(CAMRST)为:0->1->0 (0表示正常工作的电平)
 	但是实验证明，该复位时序与我们的 OV7740需要的复位时序(1->0->1)，不符
 *2.因此我们就应该结合OV7740的具体复位时序，来设计相应的寄存器	
 */
static void cmos_ov7740_reset(void)
{	
	/*传输方式是BT601*/
	*CISRCFMT |= (1<<31);
		
	*CIGCTRL |= (1<<30);
	mdelay(30);
	*CIGCTRL &= ~(1<<30);
	mdelay(30);
	*CIGCTRL |= (1<<30);
	mdelay(30);
}
static void cmos_ov7740_init(void)
{	
	unsigned int mid;
	int i;
	
	/*读厂家ID*/
    mid = i2c_smbus_read_byte_data(cmos_ov7740_client, 0x0A) << 8 ;  
    mid |= i2c_smbus_read_byte_data(cmos_ov7740_client, 0x0B) ;
	printk("manufacture ID 0x%4x \n",mid);
	
	/*写*/
	for(i=0; i < OV7740_INIT_REGS_SIZE; i++)
	{
    	i2c_smbus_write_byte_data(cmos_ov7740_client, ov7740_setting_30fps_vga_640_480[i].regaddr, ov7740_setting_30fps_vga_640_480[i].value);
		mdelay(2);
	}
}

/*****************************************************************************/
static int __devinit cmos_ov7740_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	/*2.3 硬件相关*/
	/*2.3.1 映射相应的寄存器*/
	GPJCON = ioremap(0x560000d0,4);
	GPJDAT = ioremap(0x560000d4,4);
	GPJUP  = ioremap(0x560000d8,4);

	CISRCFMT    = ioremap(0x4F000000,4);  /*开始摄像头采集时候，设置相应寄存器 stream on*/
	CIWDOFST 	= ioremap(0x4F000004,4); 
	CIGCTRL  	= ioremap(0x4F000008,4);
	CIPRCLRSA1 	= ioremap(0x4F00006C,4);
	CIPRCLRSA2 	= ioremap(0x4F000070,4);
	CIPRCLRSA3 	= ioremap(0x4F000074,4);
	CIPRCLRSA4 	= ioremap(0x4F000078,4);	
	CIPRTRGFMT 	= ioremap(0x4F00007C,4);
	CIPRCTRL   	= ioremap(0x4F000080,4);
	CIPRSCPRERAIO = ioremap(0x4F000084,4);
	CIPRSCPREST = ioremap(0x4F000088,4);
	CIPRSCCTRL  = ioremap(0x4F00008C,4);
	CIPRTAREA   = ioremap(0x4F000090,4);
	CIIMGCPT    = ioremap(0x4F0000A0,4);
	
	SRC_PND     = ioremap(0x4A000000,4);
	INT_PND     = ioremap(0x4A000010,4);
	SUBSRC_PND  = ioremap(0x4A000018,4);
	/*2.3.2 设置相关的GPIO用于CAMIF*/
	coms_ov7740_gpio_cfg();

	/*2.3.3 复位一下CAMIF控制器*/
	cmos_ov7740_camif_reset();

	/*2.3.4 设置/使能时钟(使能HCLK/使能并设置CAMCLK = 24MHz)*/
	cmos_ov7740_clk_cfg();

	/*2.3.5 复位一下摄像头模块 */
	cmos_ov7740_reset();

	/*2.3.6 通过IIC总线，初始化摄像头模块*/
	cmos_ov7740_client = client;
	cmos_ov7740_init();

	/*2.3.7 注册中断，每采集到一帧数据，就触发一次中断*/
	//编码通道中断
	if ( request_irq(IRQ_S3C2440_CAM_C, cmos_ov7740_camif_irq_c, IRQF_DISABLED, "CAM_C",NULL) )
		printk("%s: request irq fail !\n",__FUNCTION__);

	//预览通道中断
	if ( request_irq(IRQ_S3C2440_CAM_P, cmos_ov7740_camif_irq_p, IRQF_DISABLED, "CAM_C",NULL) )
		printk("%s: request irq fail !\n",__FUNCTION__);

	/*2.2注册*/	
	video_register_device(&cmos_ov7740_vdev, VFL_TYPE_GRABBER, -1);
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

	return 0;
}

static int __devexit cmos_ov7740_remove(struct i2c_client *client)
{
	video_unregister_device(&cmos_ov7740_vdev);
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

static const struct i2c_device_id cmos_ov7740_id_table[] = {
	{ "cmos_ov7740", 0 },
	{}
};


/* 1.1 分配/设置i2c_driver */
static struct i2c_driver cmos_ov7740_driver = {
	.driver	= {
		.name	= "cmos_ov7740",
		.owner	= THIS_MODULE,
	},
	.probe		= cmos_ov7740_probe,
	.remove		= __devexit_p(cmos_ov7740_remove),
	.id_table	= cmos_ov7740_id_table,
};

static int cmos_ov7740_drv_init(void)
{
	/* 2.2 注册i2c_driver */
	i2c_add_driver(&cmos_ov7740_driver);
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

static void cmos_ov7740_drv_exit(void)
{
	i2c_del_driver(&cmos_ov7740_driver);
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
}


module_init(cmos_ov7740_drv_init);
module_exit(cmos_ov7740_drv_exit);
MODULE_LICENSE("GPL");
