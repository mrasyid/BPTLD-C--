/*  Copyright 2011 Ben Pryke.
    This file is part of Ben Pryke's TLD Implementation available under the
    terms of the GNU General Public License as published by the Free Software
    Foundation. This software is provided without warranty of ANY kind. */

#include "Detector.h"


Detector::Detector(int frameWidth, int frameHeight, double *bb, Classifier *classifier) {
    width = frameWidth;
    height = frameHeight;
    initBBWidth = (float)bb[2];
    initBBHeight = (float)bb[3];
    this->classifier = classifier;
}


double Detector::bbOverlap(double *bb1, double *bb2) {
    // Check whether the bounding-boxes overlap at all
    if (bb1[0] > bb2[0] + bb2[2]) {
        return 0;
    }
    else if (bb1[1] > bb2[1] + bb2[3]) {
        return 0;
    }
    else if (bb2[0] > bb1[0] + bb1[2]) {
        return 0;
    }
    else if (bb2[1] > bb1[1] + bb1[3]) {
        return 0;
    }
    
    // If we got this far, the bounding-boxes overlap
    double overlapWidth = min(bb1[0] + bb1[2], bb2[0] + bb2[2]) - max(bb1[0], bb2[0]);
    double overlapHeight = min(bb1[1] + bb1[3], bb2[1] + bb2[3]) - max(bb1[1], bb2[1]);
    double overlapArea = overlapWidth * overlapHeight;
    double bb1Area = bb1[2] * bb1[3];
    double bb2Area = bb2[2] * bb2[3];
    
    return overlapArea / (bb1Area + bb2Area - overlapArea);
}


vector<double *> *Detector::detect(IntegralImage *frame, double *tbb) {
    // Set the width and height that are used as 1 * scale.
    // If tbb is NULL, we are not tracking and use the first-frame
    // bounding-box size, otherwise we use the tracked bounding-box size
    float baseWidth, baseHeight;
    
    if (tbb != NULL) {
        baseWidth = (float)tbb[2];
        baseHeight = (float)tbb[3];
        //initBBWidth = baseWidth;
        //initBBHeight = baseHeight;
    } else {
        baseWidth = initBBWidth;
        baseHeight = initBBHeight;
    }
    
    if (baseWidth < 40 || baseHeight < 40) {
		//cout<<"基础宽高有一个小于40."<<endl;
        return new vector<double *>();
    }
	//cout<<"基础宽高全都大于40."<<endl;

    
    // Using the sliding-window approach, find positive matches to our object
    // Vector of positive patch matches' bounding-boxes
    vector<double *> *bbs = new vector<double *>();
    
    // Minimum and maximum scales for the bounding-box, the number of scale
    // iterations to make, and the amount to increment scale by each iteration
    float minScale = 0.5f;
    float maxScale = 1.5f;
    int iterationsScale = 6;
    float scaleInc = (maxScale - minScale) / (iterationsScale - 1);
    
    // Loop through a range of bounding-box scales
    for (float scale = minScale; scale <= maxScale; scale += scaleInc) {
        int minX = 0;
        int currentWidth = (int)(scale * (float)baseWidth);
        if(currentWidth>=width)currentWidth=width-1;
        int maxX = width - currentWidth;
        float iterationsX = 30.0;
        int incX = (int)floor((float)(maxX - minX) / (iterationsX - 1.0f));
        if(incX==0)incX=1;

	          // Same for y
            int minY = 0;
            int currentHeight = (int)(scale * (float)baseHeight);
            if(currentHeight>=height)currentHeight=height-1;
            int maxY = height - currentHeight;
            float iterationsY = 30.0;
            int incY = (int)floor((float)(maxY - minY) / (iterationsY - 1.0f));
            if(incY==0)incY=1;
	
        // Loop through all bounding-box top-left x-positions
        for (int x = minX; x <= maxX; x += incX) {

  
            
            // Loop through all bounding-box top-left x-positions
            for (int y = minY; y <= maxY; y += incY) {
                // Classify the patch
                float p = classifier->classify(frame, x, y, currentWidth, currentHeight);
                
                // Store the patch data in an array
                // [x, y, width, height, confidence, overlapping], where
                // overlapping is 1 if the bounding-box overlaps with the
                // tracked bounding box, otherwise 0
                double *bb = new double[6];
                bb[0] = (double)x;
                bb[1] = (double)y;
                bb[2] = (double)currentWidth;
                bb[3] = (double)currentHeight;
                bb[4] = (double)p;
                
                if (tbb != NULL && bbOverlap(bb, tbb) > MIN_LEARNING_OVERLAP) {
                    bb[5] = 1;
                } else {
                    bb[5] = 0;
                }
                
                // If positive, or negative and overlapping with the tracked
                // bounding-box, add this bounding-box to our return list
                if (p > 0.5f || bb[5] == 1) {
                    bbs->push_back(bb);
                } else {
                    delete [] bb;
                }
            }
        }
    }
    
    return bbs;
}


Detector::~Detector() {
}
