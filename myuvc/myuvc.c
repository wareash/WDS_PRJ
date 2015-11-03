
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <linux/Videodev2.h>

#include <asm/atomic.h>
#include <asm/unaligned.h>

#include <media/v4l2-common.h>

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>


/*�ο� driver/media/video/uvc/* */

struct myuvc_streaming_control {
	__u16 bmHint;
	__u8  bFormatIndex;
	__u8  bFrameIndex;
	__u32 dwFrameInterval;
	__u16 wKeyFrameRate;
	__u16 wPFrameRate;
	__u16 wCompQuality;
	__u16 wCompWindowSize;
	__u16 wDelay;
	__u32 dwMaxVideoFrameSize;
	__u32 dwMaxPayloadTransferSize;
	__u32 dwClockFrequency;
	__u8  bmFramingInfo;
	__u8  bPreferedVersion;
	__u8  bMinVersion;
	__u8  bMaxVersion;
};


struct frame_desc{
	int width;
	int height;
};
/*�ο� uvc_video_queue����һЩ�ṹ�� */
struct  myuvc_buffer{
	struct v4l2_buffer buf;
	int state;
	int vma_use_count;	/* ��ʾ�Ƿ��Ѿ���mmap */
	wait_queue_head_t wait; /* APPҪ��Ī���ڴ棬��������ݣ��ڴ����� */
	struct list_head stream; /*��APP������*/
	struct list_head irq;	 /*���ײ�����������*/

};

struct myuvc_queue{
	void *mem; /*�����ڴ����ʼ��ַ*/
	int count;
	int buf_size;
	struct myuvc_buffer buffer[32];	
	struct list_head mainqueue;
	struct list_head irqqueue;
};

static struct myuvc_queue myuvc_queue;

static struct video_device *myuvc_vdev;

static struct usb_device myuvc_udev;

static struct v4l2_format *myuvc_format;

static int myuvc_streaming_intf;

static int myuvc_contorl_intf;


static struct frame_desc frames[] = {{640,480}, {353,288}, {320,240}, {176,144}, {160,120}};
static int frame_idx = 1;
static int bBitsPerPixel = 16; /*lsusb -v -d 0x046d: "bBitsPerPixel" */
static int uvc_version = 0x0100; /*lsusb -v -d 0x046d: "bcdUVC" */ 

static struct myuvc_streaming_control myuvc_params;

/*A1*/
static int myuvc_open(struct file *file)
{
	return 0;
}

/*A9 �ѻ���ӳ�䵽APP�ռ䣬�Ժ�APP�Ϳ���ֱ�Ӳ�����黺�� */
static int myuvc_mmap(struct file *file, struct vm_area_struct *vma)
{
	return 0;
}

/*A12 APP����Poll��ȷ�������Ƿ�������*/
static unsigned int myuvc_poll(struct file *file, struct poll_table_struct *wait)
{
	return 0;
}

/*A18 �ر�*/
static int myuvc_close(struct file *file)
{
	return 0;
}


/*A2 ��ѯ���� �ο� uvc_v4l2_do_ioctl */
static int myuvc_vidioc_querycap(struct file *file, void  *priv,
					struct v4l2_capability *cap)
{

	memset(cap, 0, sizeof *cap);
	strlcpy(cap->driver, "myuvc",sizeof("myuvc"));
	strlcpy(cap->card, "myuvc",sizeof("myuvc"));
	cap->version = 1;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	
	return 0;
}
/*A3 �о�֧�����ָ�ʽ */
static int myuvc_vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	/*�˹��鿴����������֪���ǵ�����ͷֻ֧��һ�ָ�ʽ*/
	if (f->index >= 1)
		return -EINVAL;

	/*֧��ʲô��ʽ��
	 *�鿴Video streaming Interface
	 *�鿴GUIDΪ:
	 */
	strcpy(f->description, "4:2:2, packed, YUYV");
	f->pixelformat = V4L2_PIX_FMT_YUYV;

	return 0;
}

/*A4 ���ص�ǰʹ�õĸ�ʽ */
static int myuvc_vidioc_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	memcpy(f, &myuvc_format, sizeof(myuvc_format));
	return (0);
}
/*A5 �������������Ƿ�֧��ĳ�ָ�ʽ �ο�uvc_v4l2_try_format
 *myvivi_vidioc_try_fmt_vid_cap
 */
static int myuvc_vidioc_try_fmt_vid_cap(struct file *file, void *priv,
			struct v4l2_format *f)
{
	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	if(f->fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
		return -EINVAL;

	/*�˹��鿴��������ȷ��֧���ļ��ֱַ���*/
	f->fmt.pix.width = frames[frame_idx].width;
	f->fmt.pix.height= frames[frame_idx].height;
	
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * bBitsPerPixel) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;
	
	return 0;
}
/*A6 ���ø��ָ�ʽ,�ο�:myvivi_vidioc_s_fmt_vid_cap*/
static int myuvc_vidioc_s_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	int ret = myuvc_vidioc_try_fmt_vid_cap(file, NULL, f);
	if (ret < 0)
		return ret;

    memcpy(&myuvc_format, f, sizeof(myuvc_format));

	return ret;
}


static int myuvc_free_buffers(void)
{
	kfree(myuvc_queue.mem);
	memset(&myuvc_queue, 0, sizeof(myuvc_queue));
	return 0;
}

/*A7 APP���ø�ioctl����������������ɸ�������,�Ժ�APP������Щ�����ж�����Ƶ����
 *�ο�: uvc_alloc_buffers
 */
static int myuvc_vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	int nbuffers = p->count;
	int bufsizie = PAGE_ALIGN(myuvc_format.fmt.pix.sizeimage);

	
	void *mem = NULL;
	int ret;


	if ((ret = myuvc_free_buffers()) < 0)
		goto done;

	/* Bail out if no buffers should be allocated. */
	if (nbuffers == 0)
		goto done;

	/* Decrement the number of buffers until allocation succeeds. */
	for (; nbuffers > 0; --nbuffers) {
		mem = vmalloc_32(nbuffers * bufsize);
		if (mem != NULL)
			break;
	}

	if (mem == NULL) {
		ret = -ENOMEM;
		goto done;
	}
	
	/*��Щ������һ������Ϊһ�������������*/
	memset (&myuvc_queue, 0, sizeof(myuvc_queue));
	
	INIT_LIST_HEAD(&myuvc_queue->mainqueue);
	INIT_LIST_HEAD(&myuvc_queue->irqqueue);
	
	for (i = 0; i < nbuffers; ++i) {
		memset(&queue->buffer[i], 0, sizeof queue->buffer[i]);
		myuvc_queue.buffer[i].buf.index = i;
		myuvc_queue.buffer[i].buf.m.offset = i * bufsize;
		myuvc_queue.buffer[i].buf.length = myuvc_format.fmt.pix.sizeimage;
		myuvc_queue.buffer[i].buf.type = queue->type;
		myuvc_queue.buffer[i].buf.sequence = 0;
		myuvc_queue.buffer[i].buf.field = V4L2_FIELD_NONE;
		myuvc_queue.buffer[i].buf.memory = V4L2_MEMORY_MMAP;
		myuvc_queue.buffer[i].buf.flags = 0;
		myuvc_queue.buffer[i].state 	= VIDEOBUF_IDLE;
		init_waitqueue_head(myuvc_queue.buffer[i].wait);
	}

	myuvc_queue.mem = mem;
	myuvc_queue.count = nbuffers;
	myuvc_queue.buf_size = bufsize;
	ret = nbuffers;
	
	done:
	return ret;

}
/*A8 ��ѯ������Ϣ�������ַ��Ϣ(APP������mmap����ӳ��)
 *�ο�: uvc_query_buffer
 */
static int myuvc_vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *v4l2_buf)
{
	int ret = 0;
	if (v4l2_buf->index >= myuvc_queue.count) {
		ret = -EINVAL;
		goto done;
	}
	
	memcpy(v4l2_buf, myuvc_queue.buffer[v4l2_buf->index].buf, sizeof *v4l2_buf);

	/*����flags*/
	if (myuvc_queue.buffer[v4l2_buf->index].vma_use_count)
		v4l2_buf->flags |= V4L2_BUF_FLAG_MAPPED;

	switch (myuvc_queue.buffer[v4l2_buf->index].state) {
	case VIDEOBUF_ERROR:
	case VIDEOBUF_DONE:
		v4l2_buf->flags |= VIDEOBUF_DONE;
		break;
	case VIDEOBUF_QUEUED:
	case VIDEOBUF_ACTIVE:
		v4l2_buf->flags |= VIDEOBUF_QUEUED;
		break;
	case VIDEOBUF_IDLE:
	default:
		break;
	}

done:	
	return ret;
}

/*A10 �ѻ�����������У��ײ��Ӳ����������������ݷ��뵽������еĻ��� 
 *�ο� uvc_queue_buffer
 */
static int myuvc_vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *v4l2_buf)
{
	struct myuvc_buffer *buf = &myuvc_queue.buffer[v4l2_buf->index];
	/*1. �޸�״̬ */
	buf->state = VIDEOBUF_QUEUED;
	v4l2_buf->bytesused = 0;
	 
	/*2. ������������*/
	/*����1 :��APPʹ��
	 *��������û������ʱ������mainqueue
	 *��������������ʱ��APP��mainqueue����ȡ��
	 */
	list_add_tail(&buf->stream, &myuvc_queue->mainqueue);
	/*����1 :���������ݵĺ���
	 *���ɼ������ݣ���irqqueue������ȡ����һ������������������
	 */
	list_add_tail(&buf->irq, &myuvc_queue->irqqueue);

	return 0;
}

/*A13 APP����poll/select ȷ�������ݺ󣬰ѻ���Ӷ�����ȡ��
 *uvc_dequeue_buffer
 */
static int myuvc_vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *v4l2_buf)
{
	/*APP�������ݾ����󣬴�mainqueue��ȡ�����buffer*/
	struct myuvc_buffer *buf = &myuvc_queue.buffer[v4l2_buf->index];
	
	list_del(&buf->stream);
	return 0;
}

/*�ο�:C  ,(ret = uvc_get_video_ctrl(video, probe, 1, GET_CUR)) < 0) */
 static int myuvc_get_streaming_params(struct myuvc_streaming_control *ctrl)
{
	__u8 *data;
	__u16 size;
	int ret;
	__u8 type = USB_TYPE_CLASS | USB_RECIP_INTERFACE;
	unsigned int pipe;
	
	size = uvc_version >= 0x0110 ? 34 : 26;
	data = kmalloc(size, GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	pipe = (GET_CUR & 0x80) ? usb_rcvctrlpipe(myuvc_udev, 0)
			      : usb_sndctrlpipe(myuvc_udev, 0);
	type |= (GET_CUR & 0x80) ? USB_DIR_IN : USB_DIR_OUT;

 	ret = usb_control_msg(myuvc_udev, pipe, GET_CUR, type, VS_PROBE_CONTROL<< 8,
			unit << 8 | myuvc_streaming_intf, data, size, timeout);

	if(ret)
		goto done;
	
	ctrl->bmHint = le16_to_cpup((__le16 *)&data[0]);
	ctrl->bFormatIndex = data[2];
	ctrl->bFrameIndex = data[3];
	ctrl->dwFrameInterval = le32_to_cpup((__le32 *)&data[4]);
	ctrl->wKeyFrameRate = le16_to_cpup((__le16 *)&data[8]);
	ctrl->wPFrameRate = le16_to_cpup((__le16 *)&data[10]);
	ctrl->wCompQuality = le16_to_cpup((__le16 *)&data[12]);
	ctrl->wCompWindowSize = le16_to_cpup((__le16 *)&data[14]);
	ctrl->wDelay = le16_to_cpup((__le16 *)&data[16]);
	ctrl->dwMaxVideoFrameSize = get_unaligned_le32(&data[18]);
	ctrl->dwMaxPayloadTransferSize = get_unaligned_le32(&data[22]);

	if (size == 34) {
		ctrl->dwClockFrequency = get_unaligned_le32(&data[26]);
		ctrl->bmFramingInfo = data[30];
		ctrl->bPreferedVersion = data[31];
		ctrl->bMinVersion = data[32];
		ctrl->bMaxVersion = data[33];
	} else {
		//ctrl->dwClockFrequency = video->dev->clock_frequency;
		ctrl->bmFramingInfo = 0;
		ctrl->bPreferedVersion = 0;
		ctrl->bMinVersion = 0;
		ctrl->bMaxVersion = 0;
	}

done:
	kfree(data); 
	
	return ret;
}


static int myuvc_print_streaming_params(struct myuvc_streaming_control *ctrl)
{
	printk("Video params: \n");
	printk("bmHint						%d \n",ctrl->bmHint);
	printk("bFormatIndex				%d \n",ctrl->bFormatIndex);
	printk("bFrameIndex					%d \n",ctrl->bFrameIndex);
	printk("dwFrameInterval				%d \n",ctrl->dwFrameInterval);
	printk("wKeyFrameRate				%d \n",ctrl->wKeyFrameRate);
	printk("wPFrameRate					%d \n",ctrl->wPFrameRate);
	printk("wCompQuality				%d \n",ctrl->wCompQuality);
	printk("wCompWindowSize				%d \n",ctrl->wCompWindowSize);
	printk("wDelay						%d \n",ctrl->wDelay);
	printk("dwMaxVideoFrameSize			%d \n",ctrl->dwMaxVideoFrameSize);
	printk("dwMaxPayloadTransferSize	%d \n",ctrl->dwMaxPayloadTransferSize);
	printk("bmFramingInfo				%d \n",ctrl->bmFramingInfo);
	printk("dwClockFrequency			%d \n",ctrl->dwFrameInterval);
	printk("bmFramingInfo				%d \n",ctrl->bmFramingInfo);
	printk("bPreferedVersion			%d \n",ctrl->bPreferedVersion);
	printk("bMinVersion					%d \n",ctrl->bMinVersion);
	printk("bMaxVersion					%d \n",ctrl->bMaxVersion);
}


 /*A11 ��������
 *�ο�: uvc_video_enable
 */
static int myuvc_vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	/*1. ������ͷ���ò���,����ʹ������format,ʹ�����format�µ�����flame(�ֱ���)
	 *�ο�:uvc_set_video_ctrl	
	 *1.1 ����һ���ṹ���������ݰ� uvc_streaming_control ���ݰ�:�����ֹ����ã�Ҳ���Զ��������޸�
	 *1.2 ����usb_control_msg�������ݰ�
	 */

	/*a. �ȶ�������*/
	myuvc_get_streaming_params(&myuvc_params);
	myuvc_print_streaming_params(&myuvc_params);
	
	/*b. �޸Ĳ���*/
	/*c. �ύ����*/
	struct uvc_streaming_control ctrl;

	/*2. ��������URB*/

	/*3. �ύURB�������� */
	
	return 0;
}

/*
*A14 ֮ǰ�Ѿ�ͨ��mmapӳ���˻��棬APP����ֱ�Ӷ�����
*A15 �ٴε���myuvc_video_dqbuf�����ݷ������
*A16 poll
*/

/*A17 ֹͣ*/
static int myuvc_vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{

	return 0;
}


static const struct v4l2_file_operations myuvc_fops = {
	.owner		= THIS_MODULE,
	.open	    = myuvc_open,
	.release    = myuvc_close,
	.mmap 		= myuvc_mmap,
	.ioctl		= video_ioctl2, /* V4L2 ioctl handler */
	.poll		= myuvc_poll,
};

static const struct v4l2_ioctl_ops myuvc_ioctl_ops = {
			// ��ʾ����һ������ͷ�豸
		.vidioc_querycap	  = myuvc_vidioc_querycap,

		/* �����о١���á����ԡ���������ͷ�����ݵĸ�ʽ */
		.vidioc_enum_fmt_vid_cap  = myuvc_vidioc_enum_fmt_vid_cap,	
		.vidioc_g_fmt_vid_cap	  = myuvc_vidioc_g_fmt_vid_cap,
		.vidioc_try_fmt_vid_cap   = myuvc_vidioc_try_fmt_vid_cap,
		.vidioc_s_fmt_vid_cap	  = myuvc_vidioc_s_fmt_vid_cap,
	
		/* ����������: ����/��ѯ/�������/ȡ������ */
		.vidioc_reqbufs 	  = myuvc_vidioc_reqbufs,
		.vidioc_querybuf	  = myuvc_vidioc_querybuf,
		.vidioc_qbuf		  = myuvc_vidioc_qbuf,
		.vidioc_dqbuf		  = myuvc_vidioc_dqbuf,

		// ����/ֹͣ
		.vidioc_streamon	  = myuvc_vidioc_streamon,
		.vidioc_streamoff	  = myuvc_vidioc_streamoff,	


};

static void myuvc_release (struct video_device *vdev)
{	
}



static int myuvc_probe(struct usb_interface *intf,
		     const struct usb_device_id *id)
{	
	static int cnt = 0;
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_device_descriptor *descriptor = &dev->descriptor;
	struct usb_host_config *hostconfig;
	struct usb_config_descriptor *config;
	struct usb_interface_assoc_descriptor *assoc_desc;
    struct usb_interface_descriptor	*interface;
    struct usb_endpoint_descriptor  *endpoint;
    int i, j, k, l, m;
	unsigned char *buffer;
	int buflen;
    int desc_len;
    int desc_cnt;

	myuvc_udev = dev;

	
	printk("myuvc_probe : cnt = %d \n", cnt++);
	if(cnt == 1)
	{
		myuvc_contorl_intf = intf->cur_altsetting->desc.bInterfaceNumber;
	}
	else if (cnt == 2)
	{
		myuvc_streaming_intf = intf->cur_altsetting->desc.bInterfaceNumber;
	}

	if(cnt == 2)
	{
		/*1. ����һ��video_device�ṹ��*/
		myuvc_vdev = video_device_alloc();
		
		/*2. ����*/
		
		/* 2.1 */
		myuvc_vdev->release = myuvc_release;
		
		/* 2.2 */
		myuvc_vdev->fops    = &myuvc_fops;
		
		/* 2.3 */
		myuvc_vdev->ioctl_ops = &myuvc_ioctl_ops;
		

		/*3. ע��*/
	    
		video_register_device(myuvc_vdev, VFL_TYPE_GRABBER, -1);
	}
    return 0;
}

static void myuvc_disconnect(struct usb_interface *intf)
{
	static int cnt = 0;
	printk("myuvc_disconnect : cnt = %d \n", cnt++);

	if(cnt == 2)
	{
		video_unregister_device(myuvc_vdev);
		video_device_release(myuvc_vdev);
	}
}

static struct usb_device_id myuvc_ids[] = {
/* Generic USB Video Class */
{ USB_INTERFACE_INFO(USB_CLASS_VIDEO, 1, 0) },	/* video_control */
{ USB_INTERFACE_INFO(USB_CLASS_VIDEO, 2, 0) },	/* video_stream */
{}
};


/*1. ����/����USB_driver */
static struct usb_driver myuvc_driver = {
		.name		= "myuvc",
		.probe		= myuvc_probe,
		.disconnect	= myuvc_disconnect,
		.id_table	= myuvc_ids,
};



static int myuvc_init(void)
{

	/*3. ע��*/

	usb_register(&myuvc_driver);
	
	return 0;
}


static void myuvc_exit(void)
{
	usb_deregister(&myuvc_driver);
}

module_init(myuvc_init);
module_exit(myuvc_exit);
MODULE_LICENSE("GPL");

	
	