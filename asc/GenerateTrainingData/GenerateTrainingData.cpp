#include <iostream>


#include <CLM.h>
#include <CLMTracker.h>
#include <CLMParameters.h>
#include <CLM_utils.h>


#include <fstream>
#include <sstream>
#include <queue>

using namespace std;

int main (int argc, char **argv)
{

	// By default try webcam 1
	int device = 1;

    float fx = 500, fy = 500, cx = 0, cy = 0;
			
	CLMTracker::CLMParameters clmParameters;
	
	CLMTracker::CLM clmModel(clmParameters.model_location);	

	VideoCapture videoCapture(device);

	Mat capturedImage;
	videoCapture >> capturedImage;
	videoCapture >> capturedImage;

	// If optical centers are not defined just use center of image
	if(cx == 0 || cy == 0)
	{
		cx = capturedImage.cols / 2.0f;
		cy = capturedImage.rows / 2.0f;
	}

	queue<double> frameTimes;

	while(!capturedImage.empty()) {

		Mat_<uchar> grayscaleImage;
		Mat_<float> depthImage;

		cvtColor(capturedImage, grayscaleImage, CV_BGR2GRAY);				

		bool detectionSuccess = CLMTracker::DetectLandmarksInVideo(grayscaleImage, depthImage, clmModel, clmParameters);

		if (detectionSuccess) {
			Vec6d pose_estimate_CLM = CLMTracker::GetCorrectedPoseCameraPlane(clmModel, fx, fy, cx, cy, clmParameters);

			CLMTracker::Draw(capturedImage, clmModel);
			CLMTracker::DrawBox(capturedImage, pose_estimate_CLM, Scalar(255, 0, 0), 2, fx, fy, cx, cy);
		}

		// FPS calculation

		double now = cv::getTickCount() / (double)cv::getTickFrequency();

		frameTimes.push(now);
		while(frameTimes.front() < now - 3)
			frameTimes.pop(); // Only keep history of the last 3 seconds.

		double fps = frameTimes.size() / (now - frameTimes.front());

		char fpsC[255];
		sprintf(fpsC, "%d", (int)fps);
		string fpsSt("FPS:");
		fpsSt += fpsC;
		cv::putText(capturedImage, fpsSt, cv::Point(10,20), CV_FONT_HERSHEY_SIMPLEX, 0.5, CV_RGB(255,0,0));		

		// Display tracked image.
		imshow("Tracking", capturedImage);

		// detect key presses
		char characterPress = cv::waitKey(1);

		// quit the application
		if(characterPress == 'q')
		{
			return 0;
		}

		videoCapture >> capturedImage;
	}

	return 0;
}

