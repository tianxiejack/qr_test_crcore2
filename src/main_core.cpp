/*
 * main_core.cpp
 *
 *  Created on: Sep 27, 2018
 *      Author: wzk
 */

#include <opencv2/opencv.hpp>
#include <glew.h>
#include <glut.h>
#include <freeglut_ext.h>
#include <opencv/cv.hpp>
#include <linux/videodev2.h>
#include <unistd.h>
#include <osa_sem.h>
#include "main.h"
#include "crCore.hpp"
#include "crosd.hpp"
#include "secondScreen.hpp"
#include "targetRender.hpp"
#include "thread.h"
#include "MultiChVideo.hpp"

#include "menu.hpp"
#include "PortFactory.hpp"
#include "Capture.hpp"

using namespace cv;

#define WHITECOLOR 		0x008080FF
#define YELLOWCOLOR 	0x009110D2
#define CRAYCOLOR		0x0010A6AA
#define GREENCOLOR		0x00223691
#define MAGENTACOLOR	0x00DECA6A
#define REDCOLOR		0x00F05A51
#define BLUECOLOR		0x006EF029
#define BLACKCOLOR		0x00808010
#define BLANKCOLOR		0x00000000


static ICore_1001 *core = NULL;
static CORE1001_STATS stats;

static CMenu* gMenu = NULL;
static CPortInterface* pCom = NULL;
static int fduart;

static void processFrame_core(int cap_chid,unsigned char *src, struct v4l2_buffer capInfo, int format)
{
	if(capInfo.flags & V4L2_BUF_FLAG_ERROR){
		//OSA_printf("ch%d V4L2_BUF_FLAG_ERROR", cap_chid);
		return;
	}

	if(core != NULL){
		if(cap_chid == stats.mainChId){
			cv::Mat frame;
			if(format == V4L2_PIX_FMT_GREY){
				frame = cv::Mat(SYS_CHN_HEIGHT(cap_chid), SYS_CHN_WIDTH(cap_chid), CV_8UC1, src);
			}else if(format == V4L2_PIX_FMT_YUYV || format == V4L2_PIX_FMT_UYVY){
				frame = cv::Mat(SYS_CHN_HEIGHT(cap_chid), SYS_CHN_WIDTH(cap_chid)*2, CV_8UC1, src);
			}
			#if 0
			//Uint32 curTm = (OSA_getCurTimeInMsec())>>3;
			static float curTm = 0.0;
			int offsetY = (frame.rows-20)*fabs(sin(curTm*CV_PI/180.0));
			cv::rectangle(frame, cv::Rect(frame.cols-20, offsetY, 20, 20), cv::Scalar(255), -1);
			int offsetX = (frame.cols-20)*fabs(sin(curTm*CV_PI/180.0));
			cv::rectangle(frame, cv::Rect(offsetX, frame.rows-20, 20, 20), cv::Scalar(255), -1);
			curTm += 0.5;
			#endif
		}
		core->processFrame(cap_chid, src, capInfo, format);
	}
}

enum {
	KEYDIR_UP = 0, KEYDIR_DOWN, KEYDIR_LEFT, KEYDIR_RIGHT
};
static void Track_armRefine(int dir, int step = 1)
{
	switch(dir)
	{
	case KEYDIR_UP:
		{
			cv::Point raf(0, -1*step);
			core->setTrackPosRef(raf);
		}
		break;
	case KEYDIR_DOWN:
		{
			cv::Point raf(0, step);
			core->setTrackPosRef(raf);
		}
		break;
	case KEYDIR_LEFT:
		{
			cv::Point raf(-1*step, 0);
			core->setTrackPosRef(raf);
		}
		break;
	case KEYDIR_RIGHT:
		{
			cv::Point raf(step, 0);
			core->setTrackPosRef(raf);
		}
		break;
	default:
		break;
	}
}

static void Axis_move(int dir, int step = 1)
{
	cv::Point2f curPos = core->m_stats.chn[core->m_stats.mainChId].axis;
	cv::Point pos;
	switch(dir)
	{
	case KEYDIR_UP:
		{
			pos = cv::Point(curPos.x+0.5, curPos.y+0.5-1*step);
			core->setAxisPos(pos);
		}
		break;
	case KEYDIR_DOWN:
		{
			pos = cv::Point(curPos.x+0.5, curPos.y+0.5+1*step);
			core->setAxisPos(pos);
		}
		break;
	case KEYDIR_LEFT:
		{
			pos = cv::Point(curPos.x+0.5-1*step, curPos.y+0.5);
			core->setAxisPos(pos);
		}
		break;
	case KEYDIR_RIGHT:
		{
			pos = cv::Point(curPos.x+0.5+1*step, curPos.y+0.5);
			core->setAxisPos(pos);
		}
		break;
	default:
		break;
	}
	OSA_printf("%s %d: cur(%f,%f) to(%d %d)", __func__, __LINE__,curPos.x, curPos.y, pos.x, pos.y);
}

static int iMenu = 0;
static int chrChId = 0;
static void keyboard_event(unsigned char key, int x, int y)
{
	#if 1
	cv::Size winSize(80, 60);
	static int fovId[SYS_CHN_CNT] = {0,0};
	static bool mmtdEnable = false;
	static bool trkEnable = false;
	static bool motionDetect = false;
	static bool blobEnable = false;

	char strMenus[2][2048] ={
			"\n"
			"----------------------------------------\n"
			"|---Main Menu -------------------------|\n"
			"----------------------------------------\n"
			" [t] Main Channel Choice chId++         \n"
			" [f] Main Channel Choice Fov++          \n"
			" [a] Enable/Disable Track               \n"
			" [b] Enable/Disable MMTD                \n"
			" [B] Enable/Disable INTELL              \n"
			" [c] Enable/Disable Enh                 \n"
			" [d] Change EZoomx (X1/X2/X4)           \n"
			" [e] Enable/Disable stab                \n"
			" [E] Enable/Disable OSD                 \n"
			" [g] Change OSD Color                   \n"
			" [h] Force Track To Coast               \n"
			" [i][k][j][l] Refine Track Pos          \n"
			" [m] Setup Axis                         \n"
			" [n] Start/Pause Encoder transfer       \n"
			" [u] Change EncTrans level (0/1/2)      \n"
			" [o] Enable/Disable motion detect       \n"
			" [p] Change Pinp channel ID (0/1/null)  \n"
			" [r] Bind/Unbind blend TV BY Flr        \n"
			" [s] Enable/Disable Blob detect         \n"
			" [v] Move sub window position           \n"
			" [w] EZoomx sub window video            \n"
			" [x] Force Track to keep locked         \n"
			" [1].[5] Enable Track By MMTD           \n"
			" [esc][q]Quit                           \n"
			"--> ",

			"\n"
			"----------------------------------------\n"
			"|---Axis Menu -------------------------|\n"
			"----------------------------------------\n"
			" [t] Main Channel Choice chId++         \n"
			" [f] Main Channel Choice Fov++          \n"
			" [i] Move Up                            \n"
			" [k] Move Down                          \n"
			" [j] Move Left                          \n"
			" [l] Move Right                         \n"
			" [s] Save to file                       \n"
			" [esc][q]Back                           \n"
			"--> ",
	};

	switch(key)
	{
	case 't':
		chrChId ++;
		if(chrChId == SYS_CHN_CNT)
			chrChId = 0;
		winSize.width *= ((float)SYS_CHN_WIDTH(chrChId))/1920.0f;
		winSize.height *= ((float)SYS_CHN_HEIGHT(chrChId))/1080.0f;
		core->setMainChId(chrChId, fovId[chrChId], 0, winSize);
		break;
	case 'f':
		fovId[chrChId] = (fovId[chrChId]<4-1) ? (fovId[chrChId]+1) : 0;
		winSize.width *= ((float)SYS_CHN_WIDTH(chrChId))/1920.0f;
		winSize.height *= ((float)SYS_CHN_HEIGHT(chrChId))/1080.0f;
		core->setMainChId(chrChId, fovId[chrChId], 0, winSize);
		break;
	case 'z':
		OSA_printf("%s %d: ...", __func__, __LINE__);
		if(chrChId == 2)
			fovId[chrChId] = (fovId[chrChId]<4-1) ? (fovId[chrChId]+1) : 0;
		chrChId = 2;
		winSize.width *= ((float)SYS_CHN_WIDTH(chrChId))/1920.0f;
		winSize.height *= ((float)SYS_CHN_HEIGHT(chrChId))/1080.0f;
		core->setMainChId(chrChId, fovId[chrChId], 0, winSize);
		break;
	case 'u':
		static int speedLevel = 0;
		speedLevel++;
		if(speedLevel>2)
			speedLevel=0;
		core->setEncTransLevel(speedLevel);
		break;
	case 'a':
		trkEnable ^= 1;
		//winSize.width = 40; winSize.height = 30;
		//winSize.width *= ((float)SYS_CHN_WIDTH(chrChId))/1920.0f;
		//winSize.height *= ((float)SYS_CHN_HEIGHT(chrChId))/1080.0f;
		core->enableTrack(trkEnable, winSize, true);
		//Track_armRefine(KEYDIR_UP, 1);
		break;
	//case 'w':
	//	winSize.width = 80; winSize.height = 60;
	//	core->enableTrack(trkEnable, winSize, true);
	//	break;
	case 'b':
	{
		mmtdEnable ^=1;
		//core->enableMMTD(mmtdEnable, 8, 8, cv::Rect(SYS_CHN_WIDTH(chrChId)/4, SYS_CHN_HEIGHT(chrChId)/4, SYS_CHN_WIDTH(chrChId)/2, SYS_CHN_HEIGHT(chrChId)/2));
		//core->enableMMTD(mmtdEnable, 8, 3);//, cv::Rect(SYS_CHN_WIDTH(chrChId)/2-300, SYS_CHN_HEIGHT(chrChId)/2-200, 600, 400));
		core->enableMMTD(mmtdEnable, 5, 5);//, cv::Rect(SYS_CHN_WIDTH(chrChId)/2-300, SYS_CHN_HEIGHT(chrChId)/2-200, 600, 400));
	}
		break;
	case 'B':
		mmtdEnable ^=1;
		core->enableIntellDetect(mmtdEnable, 11, 11);
		break;
	case 'o':
		motionDetect ^=1;
		core->enableMotionDetect(motionDetect);
		break;
	case 'c':
	{
		static bool enhEnable[SYS_CHN_CNT] = {false, };
		enhEnable[chrChId] ^= 1;
		core->enableEnh(enhEnable[chrChId]);
	}
		break;
	case 'd':
	{
		static int ezoomx[4] = {1,1,1,1};
		ezoomx[chrChId] = (ezoomx[chrChId] == 4 || ezoomx[chrChId] < 1) ? 1 : ezoomx[chrChId]<<1;
		core->setEZoomx(ezoomx[chrChId]);
	}
		break;
	case 'E':
	{
		static bool osdEnable = true;
		osdEnable ^= 1;
		core->enableOSD(osdEnable);
	}
		break;
	case 'e':
	{
		static bool stabEnable[SYS_CHN_CNT] = {false, };
		stabEnable[chrChId] ^= 1;
		CORE_STAB_PARAM params;
		params.mm = CORE_STAB_PARAM::MM_HOMOGRAPHY;
		params.bBorderTransparent = false;
		params.cropMargin = -1.0f;
		params.bCropMarginScale = false;
		params.noise_cov = 1e-6;
		params.bFixedPos = false;
		core->enableStab(stabEnable[chrChId], params);
	}
		break;
	case 'g':
	{
		static int colorTab[] = {WHITECOLOR,YELLOWCOLOR,CRAYCOLOR,GREENCOLOR,MAGENTACOLOR,REDCOLOR,BLUECOLOR,BLACKCOLOR};
		static int icolor = 0;
		icolor = (icolor<sizeof(colorTab)/sizeof(int)-1) ? (icolor+1) : 0;
		core->setOSDColor(colorTab[icolor], 2);
		cr_osd::set(colorTab[icolor]);
	}
		break;
	case 'h':
	{
		static bool coastEnable = false;
		coastEnable ^= 1;
		core->setTrackCoast(((coastEnable) ? -1: 0));
	}
		break;
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	{
		//OSA_printf("enableTrackByMMTD winSize(%dx%d) chrChId=%d", winSize.width, winSize.height,chrChId);
		//winSize.width *= ((float)SYS_CHN_WIDTH(chrChId))/1920.0f;
		//winSize.height *= ((float)SYS_CHN_HEIGHT(chrChId))/1080.0f;
		//OSA_printf("enableTrackByMMTD winSize(%dx%d)", winSize.width, winSize.height);
		if(core->enableTrackByMMTD(key-'1', &winSize, true)==OSA_SOK){
			trkEnable = true;
			mmtdEnable = false;
			core->enableMMTD(mmtdEnable, 0);
		}
	}
		break;
	case 'i'://move up
		if(iMenu == 0 && trkEnable)
			Track_armRefine(KEYDIR_UP);
		if(iMenu == 1)
			Axis_move(KEYDIR_UP);
		break;
	case 'k'://move down
		if(iMenu == 0 && trkEnable)
			Track_armRefine(KEYDIR_DOWN);
		if(iMenu == 1)
			Axis_move(KEYDIR_DOWN);
		break;
	case 'j'://move left
		if(iMenu == 0 && trkEnable)
			Track_armRefine(KEYDIR_LEFT);
		if(iMenu == 1)
			Axis_move(KEYDIR_LEFT);
		break;
	case 'l'://move right
		if(iMenu == 0 && trkEnable)
			Track_armRefine(KEYDIR_RIGHT);
		if(iMenu == 1)
			Axis_move(KEYDIR_RIGHT);
		break;
	case 'm':
		printf("%s",strMenus[iMenu]);
		iMenu = 1;
		break;
	case 'n':
		static bool encEnable[2] = {true, true};
		encEnable[chrChId] ^=1;
		core->enableEncoder(chrChId, encEnable[chrChId]);
		break;
	case 's':
		if(iMenu == 1)
			core->saveAxisPos();
		else{
			blobEnable ^= 1;
			core->enableBlob(blobEnable);
		}
		break;
	case 'p':
		static int subChId = -1;
		subChId++;
		if(subChId==SYS_CHN_CNT)
			subChId = -1;
		core->setSubChId(subChId);
		break;
	case 'r':
	{
		static int blendchId = -1;
		blendchId = (blendchId == -1) ? 0 : -1;
		cv::Matx44f matricScale;
		cv::Matx44f matricRotate;
		cv::Matx44f matricTranslate;
		matricScale = cv::Matx44f::eye();
		matricRotate = cv::Matx44f::eye();
		matricTranslate = cv::Matx44f::eye();
		//matricScale.val[0] = 0.5f;
		//matricScale.val[5] = 0.5f;
		float rads = float(0) * 0.0174532925f;
		matricRotate.val[0] = cos(rads);
		matricRotate.val[1] = -sin(rads);
		matricRotate.val[4] = sin(rads);
		matricRotate.val[5] = cos(rads);
		//matricTranslate.val[3] = 0.5f;
		//matricTranslate.val[7] = -0.5f;
		core->bindBlend(1, blendchId, (matricTranslate*matricRotate*matricScale).t());
	}
		break;

	case 'v':
	{
		static int posflag = -1;
		posflag++;
		posflag = (posflag >= 4) ? 0 : posflag;
		cv::Rect rc;
		int width = SYS_DIS0_WIDTH, height = SYS_DIS0_HEIGHT;
		switch(posflag)
		{
		case 1:
			rc = cv::Rect(width*2/3, 0, width/3, height/3);
			break;
		case 2:
			rc = cv::Rect(0, 0, width/3, height/3);
			break;
		case 3:
			rc = cv::Rect(0, height*2/3, width/3, height/3);
			break;
		case 0:
		default:
			rc = cv::Rect(width*2/3, height*2/3, width/3, height/3);
			break;
		}
		core->setWinPos(1, rc);
	}
		break;
	case 'w':
	{
		static int ezoomxSub = 1;
		ezoomxSub = (ezoomxSub == 8) ? 1 : ezoomxSub<<1;
		cv::Matx44f matricScale;
		matricScale = cv::Matx44f::eye();
		matricScale.val[0] = ezoomxSub;
		matricScale.val[5] = ezoomxSub;
		core->setWinMatric(1, matricScale.t());
	}
		break;
	case 'x':
	{
		static bool keeplocked = false;
		keeplocked ^= 1;
		core->setTrackForce(keeplocked);
	}
		break;
	case 'q':
	case 27:
		if(iMenu == 0)
			glutLeaveMainLoop();
		else
			iMenu = 0;
		break;
	default:
		//printf("%s",strMenus[iMenu]);
		break;
	}
	#endif
}


/*********************************************************
 *
 * test main
 */
static void *thrdhndl_glutloop(void *context)
{
	for(;;){
		int key = getc(stdin);
		if(key == 10)
			continue;
		if(key == 'q' || key == 'Q' || key == 27){
			if(iMenu == 0)
				break;
			else
				iMenu = 0;
		}
		keyboard_event(key, 0, 0);
	}
	if(*(bool*)context)
		glutLeaveMainLoop();
	OSA_printf("%s %d: leave.", __func__, __LINE__);
	//exit(0);
	return NULL;
}
static void *thrdhndl_keyevent(void *context)
{
	for(;;){
		int key = getchar();//getc(stdin);
		if(iMenu == 0 && (key == 'q' || key == 'Q' || key == 27)){
			break;
		}
		//OSA_printf("key = %d\n\r", key);
		keyboard_event(key, 0, 0);
	}
	if(*(bool*)context)
		glutLeaveMainLoop();
	OSA_printf("%s %d: leave.", __func__, __LINE__);
	//exit(0);
	return NULL;
}
static int callback_process(void *handle, int chId, Mat frame, struct v4l2_buffer capInfo, int format)
{
	processFrame_core(chId, frame.data, capInfo, format);
	return 0;
}


static void mouse_event(int button, int state, int x, int y)
{
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
		gMenu->enter();		
	else if(button == GLUT_LEFT_BUTTON && state == GLUT_UP)
		;//printf("left  UP \n");
	else if(button == GLUT_RIGHT_BUTTON && state == GLUT_UP)
		;//printf("right  up \n");
	else if(button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN)
		gMenu->menuButton();
	return;
}


static void mousemove_event(GLint xMouse, GLint yMouse)
{
	gMenu->mouseMove(xMouse,yMouse);
	return;	
}


static CORE1001_INIT_PARAM initParam;
static bool bLoop = true;
static char strIpAddr[32] = "192.168.1.88";

#include "UtcTrack.h"
extern UTCTRACK_HANDLE g_track;
#include "MMTD.h"
extern CMMTD *g_mmtd;

#define    RECV_BUF_SIZE   1024

static bool exit_comParsing = false;
std::vector<unsigned char>  rcvBufQue;

typedef struct{
	int fd;
	int type;// 1. /dev/ttyTHS1   2.network
}comtype_t;


unsigned char recvcheck_sum(int len_t)
{
    unsigned char cksum = 0;
    for(int n=4; n<len_t-1; n++)
    {
        cksum ^= rcvBufQue.at(n);
    }
    return  cksum;
}


int parsingComEvent(comtype_t comtype)
{
	int ret =  -1;
	int cmdLength= (rcvBufQue.at(2)|rcvBufQue.at(3)<<8)+5;
	float value;
	unsigned char tempbuf[4];
	unsigned char contentBuff[128]={0};

	if(cmdLength<6)
	{
        	printf("Warning::Invaild frame\r\n");
        	rcvBufQue.erase(rcvBufQue.begin(),rcvBufQue.begin()+cmdLength);
        	return ret;
    	}
    	unsigned char checkSum = recvcheck_sum(cmdLength);
    	if(checkSum== rcvBufQue.at(cmdLength-1))
    	{	
    		switch(rcvBufQue.at(4))
	        {
	            case 0x00:
				gMenu->comsetPre(rcvBufQue.at(5));
	                break;
				case 0x01:
					gMenu->comsetEnh(rcvBufQue.at(5));
					break;
				case 0x02:
					gMenu->comsetStb(rcvBufQue.at(5));
					break;
				case 0x03:
					gMenu->comsetStbparam_mode(rcvBufQue.at(5));
					break;
				case 0x04:
					gMenu->comsetStbparam_filter(rcvBufQue.at(5));
					break;
	    		default:
					printf("INFO: Unknow  Control Command, please check!!!\r\n ");
        			ret =0;
        			break;
	       	}
    	}
    	else
        	printf("%s,%d, checksum error:,cal is %02x,recv is %02x\n",__FILE__,__LINE__,checkSum,rcvBufQue.at(cmdLength-1));
		
	rcvBufQue.erase(rcvBufQue.begin(),rcvBufQue.begin()+cmdLength);
	return 1;
}


void parsingframe(unsigned char *tmpRcvBuff, int sizeRcv, comtype_t comtype)
{
	unsigned int uartdata_pos = 0;
	unsigned char frame_head[]={0xEB, 0x40};
	
	static struct data_buf
	{
		unsigned int len;
		unsigned int pos;
		unsigned char reading;
		unsigned char buf[RECV_BUF_SIZE];
	}swap_data = {0, 0, 0,{0}};


	uartdata_pos = 0;
	if(sizeRcv>0)
	{
		printf("------------------(fd:%d)start recv date---------------------\n", comtype.fd);
		for(int j=0;j<sizeRcv;j++)
			printf("%02x ",tmpRcvBuff[j]);
		printf("\n");

		while (uartdata_pos< sizeRcv)
		{
	        	if((0 == swap_data.reading) || (2 == swap_data.reading))
	       	{
	    			if(frame_head[swap_data.len] == tmpRcvBuff[uartdata_pos])
	    			{
	        			swap_data.buf[swap_data.pos++] =  tmpRcvBuff[uartdata_pos++];
	        			swap_data.len++;
	        			swap_data.reading = 2;
	        			if(swap_data.len == sizeof(frame_head)/sizeof(char))
	            				swap_data.reading = 1;
	    			}
	       		else
	        		{
		            		uartdata_pos++;
		            		if(2 == swap_data.reading)
		                		memset(&swap_data, 0, sizeof(struct data_buf));
	        		}
			}
		   	else if(1 == swap_data.reading)
			{
				swap_data.buf[swap_data.pos++] = tmpRcvBuff[uartdata_pos++];
				swap_data.len++;
				if(swap_data.len>=4)
				{
					if(swap_data.len==((swap_data.buf[2]|(swap_data.buf[3]<<8))+5))
					{
						for(int i=0;i<swap_data.len;i++)
							rcvBufQue.push_back(swap_data.buf[i]);

						parsingComEvent(comtype);
						memset(&swap_data, 0, sizeof(struct data_buf));
					}
				}
			}
		}
	}
	return;
}


void* thread_comrecvEvent(void *p)
{
	int sizeRcv;
 	uint8_t dest[1024]={0};
	int read_status = 0;
	int dest_cnt = 0;
	unsigned char  tmpRcvBuff[RECV_BUF_SIZE];
	memset(tmpRcvBuff,0,sizeof(tmpRcvBuff));

	while(!exit_comParsing)
	{
		sizeRcv= pCom->crecv(fduart, (void *)tmpRcvBuff,RECV_BUF_SIZE);
		comtype_t comtype = {fduart, 1};
		parsingframe(tmpRcvBuff, sizeRcv, comtype);
	}
	return NULL;
}

void processFrame(const cv::Mat frame,const int chId)
{
	#if 0
	printf("w,h = %d ,%d ,channel = %d \n", frame.cols,frame.rows , frame.channels());
	printf("--------------\n");
	Mat dst;
	cvtColor(frame, dst, COLOR_YUV2BGR_UYVY);
	cv::imshow("test", dst);
	cv::waitKey(1);
	#endif


	//Mat dst;
	//cvtColor(frame, dst, COLOR_YUV2BGR_UYVY);
	//printf("------------+++++-----------------------------------------\n");

	struct v4l2_buffer buf;
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_USERPTR;//V4L2_MEMORY_MMAP

	int format = V4L2_PIX_FMT_UYVY;
	buf.flags = 8196;

	processFrame_core(chId, frame.data, buf, format);
	return;
}

int setInputWay()
{
	std::string cfgAvtFile;
	char avt[30] = "cfg_";
	cfgAvtFile = "Profile.yml";
	FILE *fp = fopen(cfgAvtFile.c_str(), "rt");

	if(fp != NULL)
	{
		fseek(fp, 0, SEEK_END);
		int len = ftell(fp);
		fclose(fp);
		if(len < 10) return  -1;
		else
		{
			FileStorage fr(cfgAvtFile, FileStorage::READ);
			if(fr.isOpened())
			{
				sprintf(avt, "cfg_choose");
				int choose = (int)fr[avt];

				//printf("++---------------%d\n",choose);
				return choose;
			}
		}
	}
}


void setOnvif()
{
	std::string cfgAvtFile;
	char avt[30] = "";
	cfgAvtFile = "Profile.yml";
	FILE *fp = fopen(cfgAvtFile.c_str(), "rt");

	if(fp != NULL)
	{
		fseek(fp, 0, SEEK_END);
		int len = ftell(fp);
		fclose(fp);
		if(len < 10) return ;
		else
		{
			FileStorage fr(cfgAvtFile, FileStorage::READ);
			if(fr.isOpened())
			{
				sprintf(avt, "cfg_ONVIF_url");
				std::string urlAdress = (string)fr[avt];
				sprintf(avt,"cfg_ONVIF_channel");
				int channel = (int)fr[avt];

				Capture* rtp0 = RTSPCapture_Create();
				rtp0->init(urlAdress.c_str(),channel,1920,1080,processFrame);

				//printf("%s---------------%d\n",urlAdress,channel);
			}
		}
	}
}

void setSDI()
{
	MultiChVideo *MultiCh = new MultiChVideo;
	MultiCh->m_user = NULL;
	MultiCh->m_usrFunc = callback_process;
	MultiCh->creat();
	MultiCh->run();
}

int main_core(int argc, char **argv)
{
	core = (ICore_1001 *)ICore::Qury(COREID_1001);
	memset(&initParam, 0, sizeof(initParam));
	initParam.renderRC = cv::Rect(SYS_DIS0_X, SYS_DIS0_Y, SYS_DIS0_WIDTH, SYS_DIS0_HEIGHT);
	initParam.renderFPS = SYS_DIS0_FPS;
	initParam.renderSched = 3.5f;
	initParam.nChannels = SYS_CHN_CNT;
	for(int i=0; i<SYS_CHN_CNT; i++){
		initParam.chnInfo[i].imgSize = cv::Size(SYS_CHN_WIDTH(i), SYS_CHN_HEIGHT(i));
		initParam.chnInfo[i].fps = SYS_CHN_FPS(i);
		initParam.chnInfo[i].format = SYS_CHN_FMT(i);
	}
	initParam.chnInfo[1].bZoomRender = true;
	initParam.chnInfo[1].zoomRatio.x = 1.0;
	initParam.chnInfo[1].zoomRatio.y =  1.0;
	initParam.bRender = true;
	initParam.bEncoder = false;
	initParam.bHideOSD = true;

	core->init(&initParam, sizeof(initParam));
	unsigned int mask = 0;
	core->setHideSysOsd(mask);
	//glClearColor(0.0,1.0,0,1.0);

	//added by zyb 20191014
	int choose = setInputWay();
	if (choose == 0) setSDI();
	else if (choose == 1) setOnvif();

	core->enableOSD(false);

	gMenu = new CMenu((void*)core);

	pCom = PortFactory::createProduct(3);
	fduart = pCom->copen();
	OSA_ThrHndl thrHandleDataIn;
	if(pCom != NULL)
		OSA_thrCreate(&thrHandleDataIn, thread_comrecvEvent,  0, 0, 0);
		

	if(initParam.bRender){
		start_thread(thrdhndl_keyevent, &initParam.bRender);
		glutKeyboardFunc(keyboard_event);
		glutMouseFunc(mouse_event);
		glutPassiveMotionFunc(mousemove_event);
		glutMainLoop();
	}else{
		thrdhndl_keyevent(&initParam.bRender);
	}
	bLoop = false;
	OSA_printf("%s %d: leave.", __func__, __LINE__);

	core->uninit();
	ICore::Release(core);
	core = NULL;

	return 0;
}

