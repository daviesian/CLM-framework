#include <iostream>


#include <CLM.h>
#include <CLMTracker.h>
#include <CLMParameters.h>
#include <CLM_utils.h>


#include <fstream>
#include <sstream>

using namespace std;

int main (int argc, char **argv)
{

	// By default try webcam 0
	int device = 1;

	// cx and cy aren't necessarilly in the image center, so need to be able to override it (start with unit vals and init them if none specified)
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

	while(!capturedImage.empty()) {

		Mat_<uchar> grayscaleImage;
		Mat_<float> depthImage;

		cvtColor(capturedImage, grayscaleImage, CV_BGR2GRAY);				

		bool detection_success = CLMTracker::DetectLandmarksInVideo(grayscaleImage, depthImage, clmModel, clmParameters);

		// Work out the pose of the head from the tracked model
		Vec6d pose_estimate_CLM;
		pose_estimate_CLM = CLMTracker::GetCorrectedPoseCameraPlane(clmModel, fx, fy, cx, cy, clmParameters);

		CLMTracker::Draw(capturedImage, clmModel);
		CLMTracker::DrawBox(capturedImage, pose_estimate_CLM, Scalar(0,0, 255), 2, fx, fy, cx, cy);

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

