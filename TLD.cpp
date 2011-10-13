/*  Copyright 2011 Ben Pryke.
    This file is part of Ben Pryke's TLD Implementation available under the
    terms of the GNU General Public License as published by the Free Software
    Foundation. This software is provided without warranty of ANY kind. */

#include "cv.h"
#include "highgui.h"
#include "Detector.h"
#include "Classifier.h"
#include "Tracker.h"
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <iostream>
using namespace std;


///ȫ�ֱ��� ==================================================================
// ����      -----------------------------------------------------------------
// ��������ާ������
#define TOTAL_FERNS 13

// ÿ��ާ�Ľڵ���
#define TOTAL_NODES 7

// ����С��������ͼ�����С�ĸ߶ȿ�Ȱٷֱ�
#define MIN_FEATURE_SCALE 0.1f

// ����С��������ͼ������ĸ߶ȿ�Ȱٷֱ�
#define MAX_FEATURE_SCALE 0.5f

// ��һ֡�켣�ϵ�С�����С�����ζȣ����ڱ�֡��ѧϰ
#define MIN_LEARNING_CONF 0.8

// ��̽�����С������һ�����ζȸ��ڸ��������ٵ�С��,
// ������Ҳ�������ֵ��Ȼ������������³�ʼ��������
// ע��: MIN_REINIT_CONF Ӧ�� <= MIN_LEARNING_CONF
#define MIN_REINIT_CONF 0.7

// ��һ֡���������ٳ���С�����С�����ζ�,������һ֡�м�������
#define MIN_TRACKING_CONF 0.15


// ���� -----------------------------------------------------------------
// ���ǵķ���������������̽����
static Classifier *classifier;
static Tracker *tracker;
static Detector *detector;

// ������֪��TLD�Ƿ񱻳�ʼ����
static bool initialised = false;

// ��ʼ���߿�еĴ�С
static float initBBWidth;
static float initBBHeight;

// ÿһ֡�Ĵ�С
int frameWidth;
int frameHeight;
CvSize *frameSize;

// ��һ֡�Ĺ켣�ϵ�С������ζ�
double confidence;



/// ���� ==================================================================
/*  ��Matlabͼ��ת��ΪIplImage
    Returns: the converted image.
    mxImage: the image straight from Matlab */
//IplImage *imageFromMatlab(const mxArray *mxImage) {
//    // Get pointer
//    unsigned char *values = (unsigned char *)mxGetPr(mxImage);
//    
//    // Create our return image
//    IplImage *image = cvCreateImage(*frameSize, IPL_DEPTH_8U, 1);
//    
//    // Loop through the new image getting values from the old one
//    // Note: values are effectively rotated 90 degrees
//    for (int i = 0; i < frameWidth; i++) {
//        for (int j = 0; j < frameHeight; j++) {
//            image->imageData[j * frameWidth + i] = values[i * frameHeight + j];
//        }
//    }
//    
//    return image;
//}


//  �ñ߿��Χס��С��ķ���任��ѵ��������
//  frame: ��������ڷ���任��ͼ��
//  bb: ��һ֡�еı߿��[x,y,width,height]
void bbWarpPatch(IntegralImage *frame, double *bb) {
    // �任����
    float *m = new float[4];
    
	// ѭ������������ת��б������
    for (float r = -0.1f; r < 0.1f; r += 0.005f) {
        float sine = sin(r);
        float cosine = cos(r);
        
        for (float sx = -0.1f; sx < 0.1f; sx += 0.05f) {
            for (float sy = -0.1f; sy < 0.1f; sy += 0.05f) {
                // ����ת������
                /*  Rotation matrix * skew matrix =
                    
                    | cos r   sin r | * | 1   sx | = 
                    | -sin r  cos r |   | sy   1 |
                    
                    | cos r + sy * sin r   sx * cos r + sin r |
                    | sy * cos r - sin r   cos r - sx * sin r | */
                m[0] = cosine + sy * sine;
                m[1] = sx * cosine + sine;
                m[2] = sy * cosine - sine;
                m[3] = cosine - sx * sine;
                
				// ��������任��Ȼ��ѵ��������
                IntegralImage *warp = new IntegralImage();
                warp->createWarp(frame, bb, m);
                classifier->train(warp, 1, 1, (int)bb[2], (int)bb[3], 1);
                delete warp;
            }
        }
    }
    
    delete m;
}


//  �ø���ѵ��С��ѵ��������,Ҳ���ǵ�һ֡�����ʼ�߿�и�����С��һ���̶ȵ�С��
//  frame: ��������ڷ���任��ͼ��
//  tbb: ��һ֡�еı߿��[x,y,width,height]
void trainNegative(IntegralImage *frame, double *tbb) {
	// �߿�е���С�����߶�
    float minScale = 0.5f;
    float maxScale = 1.5f;
	// �߶ȵ���������
    int iterationsScale = 5;
	// ÿ�ε���������
    float scaleInc = (maxScale - minScale) / (iterationsScale - 1);
    
	// ѭ��ͨ��һϵ�еı߿�г߶�
    for (float scale = minScale; scale <= maxScale; scale += scaleInc) {
        int minX = 0;
        int currentWidth = (int)(scale * initBBWidth);
        if(currentWidth>=initBBWidth)currentWidth=initBBWidth-1;
        int maxX = initBBWidth - currentWidth;
        float iterationsX = 20.0;
        int incX = (int)floor((float)(maxX - minX) / (iterationsX - 1.0));
        if(incX==0)incX=1;
        
	            // Same for y
            int minY = 0;
            int currentHeight = (int)(scale * (float)initBBHeight);
            if(currentHeight>=initBBHeight)currentHeight=initBBHeight-1;
            int maxY = initBBHeight - currentHeight;
            float iterationsY = 20.0;
            int incY = (int)floor((float)(maxY - minY) / (iterationsY - 1));
            if(incY==0)incY=1;
        // Loop through all bounding-box top-left x-positions
        for (int x = minX; x <= maxX; x += incX) {

            
            // Loop through all bounding-box top-left x-positions
            for (int y = minY; y <= maxY; y += incY) {
				// ����С�飬���������ʼС��ĸ������Ƿ�����MIN_LEARNING_OVERLAP,
				// ����ǣ���ô�͵���������������ѵ��
                double *bb = new double[4];
                bb[0] = (double)x;
                bb[1] = (double)y;
                bb[2] = (double)currentWidth;
                bb[3] = (double)currentHeight;
                
                if (Detector::bbOverlap(tbb, bb) < MIN_LEARNING_OVERLAP) {
                    classifier->train(frame, x, y, currentWidth, currentHeight, 0);
                } else {
                    //classifier->train(frame, x, y, currentWidth, currentHeight, 1);
                }
                
                delete [] bb;
            }
        }
    }
}

// ��ʼ�� --------------------------------------------------------
// ����1֡��,����2֡��,����3��ʼ��һ֡,����4��ʼ�߿��
void BpTld_Init(int Width,int Height,IplImage * firstImage,double * firstbb)
{
	// ��ȡͼ�����
	frameWidth = Width;
	frameHeight = Height;
	frameSize = (CvSize *)malloc(sizeof(CvSize));
	*frameSize = cvSize(frameWidth, frameHeight);
	IntegralImage *firstFrame = new IntegralImage();
	firstFrame->createFromIplImage(firstImage);
	IplImage *firstFrameIplImage = firstImage;
	double *bb = firstbb;
	initBBWidth = (float)bb[2];
	initBBHeight = (float)bb[3];
	//��ʼ�����ζ�
	confidence = 1.0f;

	// ��ʼ��������,��������̽����
	srand((unsigned int)time(0));
	classifier = new Classifier(TOTAL_FERNS, TOTAL_NODES, MIN_FEATURE_SCALE, MAX_FEATURE_SCALE);
	tracker = new Tracker(frameWidth, frameHeight, frameSize, firstFrameIplImage, classifier);
	detector = new Detector(frameWidth, frameHeight, bb, classifier);

	// �ó�ʼͼ��С������ķ��������ѵ��������
	classifier->train(firstFrame, (int)bb[0], (int)bb[1], (int)initBBWidth, (int)initBBHeight, 1);
	bbWarpPatch(firstFrame, bb);
	trainNegative(firstFrame, bb);

	// �ͷ��ڴ�
	delete firstFrame;
	// ����boolֵinitialised
	initialised = true;

	return;
}

/*  MEX��ڵ�.
    ����ʹ�÷���:
    ��ʼ��:
        TLD(֡��, ֡��, ��һ֡, ѡ����ı߿��)
    ����ÿһ֡:
        �µĹ켣�߿�� = TLD(��ǰ֡, �켣�߿��)
*/
void BpTld_Process(IplImage * NewImage,double * ttbb,double * outPut) {
    
    // ������� -------------------------------------------------------------
    // ��ǰ֡
    IplImage *nextFrame = NewImage;
    IntegralImage *nextFrameIntImg = new IntegralImage();
    nextFrameIntImg->createFromIplImage(NewImage);
    
	// �켣�߿��[x, y, width, height]
    double *bb = ttbb;
    
    
    // ���� + ̽�� ------------------------------------------------------
	// ֻ�����ϸ������������㹻���ŵ�ʱ��,�Ÿ���
	// ���⿪ʼ��������������һ֡�ڴ�
    double *tbb;
    vector<double *> *dbbs;
    
    if (confidence > MIN_TRACKING_CONF) {
		//cout<<"����ǰ."<<endl;
        tbb = tracker->track(nextFrame, nextFrameIntImg, bb);
		//cout<<"���ٺ�,̽��ǰ."<<endl;
		if (tbb[4] > MIN_TRACKING_CONF)
		{
			dbbs = detector->detect(nextFrameIntImg, tbb);
		} 
		else
		{
			dbbs = detector->detect(nextFrameIntImg, NULL);
		}
        
		//cout<<"̽���."<<endl;
    } else {
        dbbs = detector->detect(nextFrameIntImg, NULL);
        tracker->setPrevFrame(nextFrame);
        tbb = new double[5];
        tbb[0] = 0;
        tbb[1] = 0;
        tbb[2] = 0;
        tbb[3] = 0;
        tbb[4] = MIN_TRACKING_CONF;
    }
    
    
    // ѧϰ -----------------------------------------------------------------
    // �����õ�̽�����С������ζ�
    double dbbMaxConf = 0.0f;
    int dbbMaxConfIndex = -1;
    
    for (int i = 0; i < dbbs->size(); i++) {
        double dbbConf = dbbs->at(i)[4];
        
        if (dbbConf > dbbMaxConf) {
            dbbMaxConf = dbbConf;
            dbbMaxConfIndex = i;
        }
    }
    
	// ���̽�����С������һ�����ζ���ߣ����Ҵ���MIN_REINIT_CONF
	// ��ô�����ø������ı߿��
    if (dbbMaxConf > tbb[4] && dbbMaxConf > MIN_REINIT_CONF) {
        delete tbb;
        tbb = new double[5];
        double *dbb = dbbs->at(dbbMaxConfIndex);
        tbb[0] = dbb[0];
        tbb[1] = dbb[1];
        tbb[2] = dbb[2];
        tbb[3] = dbb[3];
        tbb[4] = dbb[4];
    }
    
	// ������ٳ���С������ζ���߲������һ֡ʱ�㹻���ţ���ô������Լ����
    else if (tbb[4] > dbbMaxConf && confidence > MIN_LEARNING_CONF) {
        for (int i = 0; i < dbbs->size(); i++) {
			// ������͸���С��ѵ��������
			// ����-�뱻���ٵ�С���غ�
			// ����-������Ϊ�����뱻���ٵ�С�鲻�غ�
            double *dbb = dbbs->at(i);
            
            if (dbb[5] == 1) {
                classifier->train(nextFrameIntImg, (int)dbb[0], (int)dbb[1], (int)dbb[2], (int)dbb[3], 1);
            }
            else if (dbb[5] == 0) {
                classifier->train(nextFrameIntImg, (int)dbb[0], (int)dbb[1], (int)dbb[2], (int)dbb[3], 0);
            }
        }
    }
    
	// Ϊ�¸������������ζ�
    confidence = tbb[4];

	//�������
	outPut[0] = tbb[0];
	outPut[1] = tbb[1];
	outPut[2] = tbb[2];
	outPut[3] = tbb[3];

    
    
	//////////////////////////////////////////////////////////////////////////
	//�˴�tbb[0],tbb[1],tbb[2],tbb[3]�������յı߿��
	//���tbb[2],tbb[3]ȫ������0,,�ͱ������������λ��
	//////////////////////////////////////////////////////////////////////////
    // ������� ------------------------------------------------------------
	// �������һϵ�б߿��;��һ���Ǹ��ٳ���С�飬ʣ�µ���̽�����С��
    // Rows correspond to individual bounding boxes
    // Columns correspond to [x, y, width, height, confidence, overlapping]
    
    // �ͷ��ڴ�
    free(tbb);
    dbbs->clear();
    delete nextFrameIntImg;
}


// ���������ڽ�����ȫC++��ʱ�����
//////////////////////////////////////////////////////////////////////////
Rect box;
bool drawing_box = false;
bool gotBB = false;

//bounding box mouse callback
void mouseHandler(int event, int x, int y, int flags, void *param){
	switch( event ){
  case CV_EVENT_MOUSEMOVE:
	  if( drawing_box ){
		  box.width = x-box.x;
		  box.height = y-box.y;
	  }
	  break;
  case CV_EVENT_LBUTTONDOWN:
	  drawing_box = true;
	  box = Rect( x, y, 0, 0 );
	  break;
  case CV_EVENT_LBUTTONUP:
	  drawing_box = false;
	  if( box.width < 0 ){
		  box.x += box.width;
		  box.width *= -1;
	  }
	  if( box.height < 0 ){
		  box.y += box.height;
		  box.height *= -1;
	  }
	  gotBB = true;
	  break;
	}
}
void drawBox(IplImage * image, CvRect box, cv::Scalar color = cvScalarAll(255), int thick=1); 
//////////////////////////////////////////////////////////////////////////
int main() 
{
	CvSize dst_size;
	dst_size.width = 640;
	dst_size.height = 480;
	//////////////////////////////////////////////////////
	CvCapture* m_pCapture = cvCreateCameraCapture(CV_CAP_ANY);
	if( NULL == m_pCapture)
	{
		assert(!"����ͷ��ʼ��ʧ��");
		return false;
	}
	//////////////////////////////////////////////////////
	//Register mouse callback to draw the bounding box
	cvNamedWindow("TLD",CV_WINDOW_AUTOSIZE);
	cvSetMouseCallback( "TLD", mouseHandler, NULL );

	//��ȡ��һ֡
	IplImage *pNewCapImg = cvQueryFrame(m_pCapture);
	IplImage *dst = cvCreateImage(dst_size,pNewCapImg->depth,pNewCapImg->nChannels);
	//�Ҷ�ͼ,��������
	IplImage *dst_gray = cvCreateImage(dst_size,IPL_DEPTH_8U,1);
	cvResize(pNewCapImg,dst);


GETBOUNDINGBOX:
	while(!gotBB)
	{

		pNewCapImg = cvQueryFrame(m_pCapture);
		cvResize(pNewCapImg,dst);
		drawBox(dst,box);
		cvShowImage("TLD",dst);

		if (cvWaitKey(33) == 'q')
			return 0;
	}
	if (min(box.width,box.height)<24){
		cout << "Bounding box too small, try again." << endl;
		gotBB = false;
		goto GETBOUNDINGBOX;
	}
	//Remove callback
	cvSetMouseCallback( "TLD", NULL, NULL );
	printf("Initial Bounding Box = x:%d y:%d h:%d w:%d\n",box.x,box.y,box.width,box.height);

	//////////////////////////////////////////////////////////////////////////
	//TLD�ĳ�ʼ������
	double * BBox = new double[4];
	double * BBox_out = new double[4];
	BBox[0] = box.x;BBox[1] = box.y;BBox[2] = box.width;BBox[3] = box.height;
	BBox_out[0] = 0;BBox_out[1] = 0;BBox_out[2] = 0;BBox_out[3] = 0;

	cvCvtColor(dst,dst_gray,CV_RGB2GRAY);
	BpTld_Init(dst_size.width,dst_size.height,dst_gray,BBox);
	//////////////////////////////////////////////////////////////////////////
	

	///Run-time
	//BoundingBox pbox;
	int frames = 0;
	int detections = 0;
	while(true)
	{
		IplImage *dst_loop = cvCreateImage(dst_size,IPL_DEPTH_8U,1);
		//��ȡ��ǰʱ��
		double t = (double)getTickCount();
		//��ȡͼ��
		//��������
		pNewCapImg = cvQueryFrame(m_pCapture);
		cvResize(pNewCapImg,dst);
		cvCvtColor(dst,dst_loop,CV_RGB2GRAY);
		//����ÿһ֡
		BpTld_Process(dst_loop,BBox,BBox_out);
		BBox[0] = BBox_out[0];
		BBox[1] = BBox_out[1];
		BBox[2] = BBox_out[2];
		BBox[3] = BBox_out[3];

		if (BBox_out[2]>0 && BBox_out[3]>0)
		{
			CvRect pBox;
			pBox.x = BBox_out[0];
			pBox.y = BBox_out[1];
			pBox.width = BBox_out[2];
			pBox.height = BBox_out[3];
			drawBox(dst,pBox);
			detections++;
		}

		//Display
		cvShowImage("TLD",dst);
		frames++;
		printf("Detection rate: %d/%d\n",detections,frames);
		//��ȡʱ���
		t=(double)getTickCount()-t;
		t=getTickFrequency()/t;
		printf("֡��: %g \n", t);
		if (cvWaitKey(33) == 'q')
			break;
		cvReleaseImage(&dst_loop);
	}
	cvReleaseImage(&pNewCapImg);
	cvReleaseImage(&dst);
	cvReleaseImage(&dst_gray);
	return 0;
}

void drawBox(IplImage * image, CvRect box, Scalar color, int thick){
	cvRectangle( image, cvPoint(box.x, box.y), cvPoint(box.x+box.width,box.y+box.height),color, thick);
} 