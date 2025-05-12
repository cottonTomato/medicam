#include "camera.h"
#include "camera_comm.h"

#include <zephyr/device.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video/arducam_mega.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(main);

#define RESET_CAMERA             0XFF
#define SET_PICTURE_RESOLUTION   0X01
#define SET_VIDEO_RESOLUTION     0X02
#define SET_BRIGHTNESS           0X03
#define SET_CONTRAST             0X04
#define SET_SATURATION           0X05
#define SET_EV                   0X06
#define SET_WHITEBALANCE         0X07
#define SET_SPECIAL_EFFECTS      0X08
#define SET_FOCUS_ENABLE         0X09
#define SET_EXPOSURE_GAIN_ENABLE 0X0A
#define SET_WHITE_BALANCE_ENABLE 0X0C
#define SET_MANUAL_GAIN          0X0D
#define SET_MANUAL_EXPOSURE      0X0E
#define GET_CAMERA_INFO          0X0F
#define TAKE_PICTURE             0X10
#define SET_SHARPNESS            0X11
#define DEBUG_WRITE_REGISTER     0X12
#define STOP_STREAM              0X21
#define GET_FRM_VER_INFO         0X30
#define GET_SDK_VER_INFO         0X40
#define SET_IMAGE_QUALITY        0X50
#define SET_LOWPOWER_MODE        0X60

const struct device *video;
struct video_buffer *vbuf;

volatile uint8_t preview_on;
volatile uint8_t capture_flag;

const uint32_t pixel_format_table[] = {
	VIDEO_PIX_FMT_JPEG,
	VIDEO_PIX_FMT_RGB565,
	VIDEO_PIX_FMT_YUYV,
};

const uint16_t resolution_table[][2] = {
	{160, 120},  {320, 240},   {640, 480},   {800, 600},   {1280, 720},
	{1280, 960}, {1600, 1200}, {1920, 1080}, {2048, 1536}, {2592, 1944},
	{96, 96},    {128, 128},   {320, 320},
};

const uint8_t resolution_num = sizeof(resolution_table) / 4;

static uint8_t current_resolution;
static uint8_t take_picture_fmt = 0x1a;

static uint8_t head_and_tail[] = {0xff, 0xaa, 0x00, 0xff, 0xbb};

void camera_packet_send(uint8_t type, uint8_t *buffer, uint32_t length)
{
	head_and_tail[2] = type;
	camera_buffer_send(&head_and_tail[0], 3);
	camera_buffer_send((uint8_t *)&length, 4);
	camera_buffer_send(buffer, length);
	camera_buffer_send(&head_and_tail[3], 2);
}

int camera_set_resolution(uint8_t sfmt)
{
	uint8_t resolution = sfmt & 0x0f;
	uint8_t pixelformat = (sfmt & 0x70) >> 4;

	if (resolution > resolution_num || pixelformat > 3) {
		return -1;
	}
	struct video_format fmt = {.width = resolution_table[resolution][0],
				   .height = resolution_table[resolution][1],
				   .pixelformat = pixel_format_table[pixelformat - 1]};
	current_resolution = resolution;
	return video_set_format(video, VIDEO_EP_OUT, &fmt);
}

int camera_take_picture(void)
{
	int err;
	enum video_frame_fragmented_status f_status;

	err = video_dequeue(video, VIDEO_EP_OUT, &vbuf, K_FOREVER);
	if (err) {
		LOG_ERR("Unable to dequeue video buf");
		return -1;
	}

	f_status = vbuf->flags;

	head_and_tail[2] = 0x01;
	camera_buffer_send(&head_and_tail[0], 3);
	camera_buffer_send((uint8_t *)&vbuf->bytesframe, 4);
	camera_char_send(((current_resolution & 0x0f) << 4) | 0x01);

	camera_buffer_send(vbuf->buffer, vbuf->bytesused);

	video_enqueue(video, VIDEO_EP_OUT, vbuf);
	while (f_status == VIDEO_BUF_FRAG) {
		video_dequeue(video, VIDEO_EP_OUT, &vbuf, K_FOREVER);
		f_status = vbuf->flags;
		camera_buffer_send(vbuf->buffer, vbuf->bytesused);
		video_enqueue(video, VIDEO_EP_OUT, vbuf);
	}
	camera_buffer_send(&head_and_tail[3], 2);

	return 0;
}

void camera_video_preview(void)
{
	int err;
	enum video_frame_fragmented_status f_status;

	if (!preview_on) {
		return;
	}

	err = video_dequeue(video, VIDEO_EP_OUT, &vbuf, K_FOREVER);
	if (err) {
		LOG_ERR("Unable to dequeue video buf");
		return;
	}

	f_status = vbuf->flags;

	if (capture_flag == 1) {
		capture_flag = 0;
		head_and_tail[2] = 0x01;
		camera_buffer_send(&head_and_tail[0], 3);
		camera_buffer_send((uint8_t *)&vbuf->bytesframe, 4);
		camera_char_send(((current_resolution & 0x0f) << 4) | 0x01);
	}

	camera_buffer_send(vbuf->buffer, vbuf->bytesused);

	if (f_status == VIDEO_BUF_EOF) {
		camera_buffer_send(&head_and_tail[3], 2);
		capture_flag = 1;
	}

	err = video_enqueue(video, VIDEO_EP_OUT, vbuf);
	if (err) {
		LOG_ERR("Unable to requeue video buf");
		return;
	}
}

int camera_report_info(void)
{
	char str_buf[400];
	uint32_t str_len;
	char *mega_type;
	struct arducam_mega_info mega_info;

	video_get_ctrl(video, VIDEO_CID_ARDUCAM_INFO, &mega_info);

	switch (mega_info.camera_id) {
	case ARDUCAM_SENSOR_3MP_1:
	case ARDUCAM_SENSOR_3MP_2:
		mega_type = "3MP";
		break;
	case ARDUCAM_SENSOR_5MP_1:
		mega_type = "5MP";
		break;
	case ARDUCAM_SENSOR_5MP_2:
		mega_type = "5MP_2";
		break;
	default:
		return -ENODEV;
	}

	sprintf(str_buf,
		"ReportCameraInfo\r\nCamera Type:%s\r\n"
		"Camera Support Resolution:%d\r\nCamera Support "
		"special effects:%d\r\nCamera Support Focus:%d\r\n"
		"Camera Exposure Value Max:%ld\r\nCamera Exposure Value "
		"Min:%d\r\nCamera Gain Value Max:%d\r\nCamera Gain Value "
		"Min:%d\r\nCamera Support Sharpness:%d\r\n",
		mega_type, mega_info.support_resolution, mega_info.support_special_effects,
		mega_info.enable_focus, mega_info.exposure_value_max, mega_info.exposure_value_min,
		mega_info.gain_value_max, mega_info.gain_value_min, mega_info.enable_sharpness);
	str_len = strlen(str_buf);
	camera_packet_send(0x02, str_buf, str_len);
	return 0;
}

uint8_t camera_recv_process(uint8_t *buff)
{
	uint16_t gain_value;
	uint32_t exposure_value;

	switch (buff[0]) {
	case SET_PICTURE_RESOLUTION:
		if (camera_set_resolution(buff[1]) == 0) {
			take_picture_fmt = buff[1];
		}
		break;
	case SET_VIDEO_RESOLUTION:
		if (preview_on == 0) {
			camera_set_resolution(buff[1] | 0x10);
			video_stream_start(video);
			capture_flag = 1;
		}
		preview_on = 1;
		break;
	case SET_BRIGHTNESS:
		video_set_ctrl(video, VIDEO_CID_CAMERA_BRIGHTNESS, &buff[1]);
		break;
	case SET_CONTRAST:
		video_set_ctrl(video, VIDEO_CID_CAMERA_CONTRAST, &buff[1]);
		break;
	case SET_SATURATION:
		video_set_ctrl(video, VIDEO_CID_CAMERA_SATURATION, &buff[1]);
		break;
	case SET_EV:
		video_set_ctrl(video, VIDEO_CID_ARDUCAM_EV, &buff[1]);
		break;
	case SET_WHITEBALANCE:
		video_set_ctrl(video, VIDEO_CID_CAMERA_WHITE_BAL, &buff[1]);
		break;
	case SET_SPECIAL_EFFECTS:
		video_set_ctrl(video, VIDEO_CID_ARDUCAM_COLOR_FX, &buff[1]);
		break;
	case SET_EXPOSURE_GAIN_ENABLE:
		video_set_ctrl(video, VIDEO_CID_CAMERA_EXPOSURE_AUTO, &buff[1]);
		video_set_ctrl(video, VIDEO_CID_CAMERA_GAIN_AUTO, &buff[1]);
	case SET_WHITE_BALANCE_ENABLE:
		video_set_ctrl(video, VIDEO_CID_CAMERA_WHITE_BAL_AUTO, &buff[1]);
		break;
	case SET_SHARPNESS:
		video_set_ctrl(video, VIDEO_CID_ARDUCAM_SHARPNESS, &buff[1]);
		break;
	case SET_MANUAL_GAIN:
		gain_value = (buff[1] << 8) | buff[2];
		video_set_ctrl(video, VIDEO_CID_CAMERA_GAIN, &gain_value);
		break;
	case SET_MANUAL_EXPOSURE:
		exposure_value = (buff[1] << 16) | (buff[2] << 8) | buff[3];
		video_set_ctrl(video, VIDEO_CID_CAMERA_EXPOSURE, &exposure_value);
		break;
	case GET_CAMERA_INFO:
		camera_report_info();
		break;
	case TAKE_PICTURE:
		video_stream_start(video);
		camera_take_picture();
		video_stream_stop(video);
		break;
	case STOP_STREAM:
		if (preview_on) {
			camera_buffer_send(&head_and_tail[3], 2);
			video_stream_stop(video);
			camera_set_resolution(take_picture_fmt);
		}
		preview_on = 0;
		break;
	case RESET_CAMERA:
		video_set_ctrl(video, VIDEO_CID_ARDUCAM_RESET, NULL);
		break;
	case SET_IMAGE_QUALITY:
		video_set_ctrl(video, VIDEO_CID_JPEG_COMPRESSION_QUALITY, &buff[1]);
		break;
	case SET_LOWPOWER_MODE:
		video_set_ctrl(video, VIDEO_CID_ARDUCAM_LOWPOWER, &buff[1]);
		break;
	default:
		break;
	}

	return buff[0];
}

int camera_setup(void)
{
	struct video_buffer *buffers[3];
	int i = 0;

	video = DEVICE_DT_GET(DT_NODELABEL(arducam_mega0));

	if (!device_is_ready(video)) {
		LOG_ERR("Video device %s not ready.", video->name);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(buffers); i++) {
		buffers[i] = video_buffer_alloc(1024);
		if (buffers[i] == NULL) {
			LOG_ERR("Unable to alloc video buffer");
			return -1;
		}
		video_enqueue(video, VIDEO_EP_OUT, buffers[i]);
	}

	LOG_INF("Mega start");
	LOG_INF("- Device name: %s\n", video->name);

	return 0;
}
