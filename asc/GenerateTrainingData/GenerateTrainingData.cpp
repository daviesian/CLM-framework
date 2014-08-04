#include <iostream>


#include <CLM.h>
#include <CLMTracker.h>
#include <CLMParameters.h>
#include <CLM_utils.h>


#include <fstream>
#include <sstream>
#include <deque>

using namespace std;

double readLineDouble(FILE* fp)
{
	char nextLine[1024];
	char* endPtr;
	fgets(nextLine, 1024,fp);
	double d = strtod(nextLine, &endPtr);
	return d;
}

void fprintf4(FILE* fp1, FILE* fp2, FILE* fp3, FILE* fp4, const char* _format, ...)
{
	va_list argptr;
	va_start(argptr, _format);
	vfprintf(fp1, _format, argptr);
	vfprintf(fp2, _format, argptr);
	vfprintf(fp3, _format, argptr);
	vfprintf(fp4, _format, argptr);
	va_end(argptr);
}


void extractFeatures(const char* videoFile,
					// LABEL FILES
					const char* labelFileA,
					const char* labelFileE,
					const char* labelFileP,
					const char* labelFileV,
					// OUTPUT FILES
					const char* outputFileA,
					const char* outputFileE,
					const char* outputFileP,
					const char* outputFileV) {

	FILE* labelFpA = fopen(labelFileA, "r");
	FILE* labelFpE = fopen(labelFileE, "r");
	FILE* labelFpP = fopen(labelFileP, "r");
	FILE* labelFpV = fopen(labelFileV, "r");

	FILE* fpA = fopen(outputFileA,"a");
	FILE* fpE = fopen(outputFileE,"a");
	FILE* fpP = fopen(outputFileP,"a");
	FILE* fpV = fopen(outputFileV,"a");

    float fx = 500, fy = 500, cx = 0, cy = 0;

	double poseSmoothingTime = 3;
	double shapeSmoothingTime = 0.2;
			
	CLMTracker::CLMParameters clmParameters;
	
	CLMTracker::CLM clmModel(clmParameters.model_location);	

	VideoCapture videoCapture(videoFile);

	double videoFps = videoCapture.get(CV_CAP_PROP_FPS);
	long totalFrames = videoCapture.get(CV_CAP_PROP_FRAME_COUNT);

	Mat capturedImage;
	videoCapture >> capturedImage;

	// If optical centers are not defined just use center of image
	if(cx == 0 || cy == 0)
	{
		cx = capturedImage.cols / 2.0f;
		cy = capturedImage.rows / 2.0f;
	}

	deque<double> frameTimesFps;
	deque<double> frameTimesPose;
	deque<double> frameTimesShape;
	deque<Vec6d> poseQueue;
	deque<Mat_<double>> shapeQueue;

	bool shapeAvailable = false;
	bool poseAvailable = false;
	Vec6d smoothedPose;
	Mat_<double> smoothedShape;

	int frameCount = 0;

	while(true) {

		videoCapture >> capturedImage;

		if (capturedImage.empty())
			break;

		double currentFrameLabelA = readLineDouble(labelFpA);
		double currentFrameLabelE = readLineDouble(labelFpE);
		double currentFrameLabelP = readLineDouble(labelFpP);
		double currentFrameLabelV = readLineDouble(labelFpV);
		frameCount++;

		// Fix frame rate as we're using video.
		double now = frameCount / videoFps; //cv::getTickCount() / (double)cv::getTickFrequency();

		Mat_<uchar> grayscaleImage;
		Mat_<float> depthImage;

		cvtColor(capturedImage, grayscaleImage, CV_BGR2GRAY);				

		bool detectionSuccess = CLMTracker::DetectLandmarksInVideo(grayscaleImage, depthImage, clmModel, clmParameters);

		if (detectionSuccess) {
			Vec6d pose_estimate_CLM = CLMTracker::GetCorrectedPoseCameraPlane(clmModel, fx, fy, cx, cy, clmParameters);

			CLMTracker::Draw(capturedImage, clmModel);
			CLMTracker::DrawBox(capturedImage, pose_estimate_CLM, Scalar(255, 0, 0), 2, fx, fy, cx, cy);

			// Add shape and pose params to queues.
			poseQueue.push_back(pose_estimate_CLM);
			shapeQueue.push_back(clmModel.params_local.clone());

			frameTimesPose.push_back(now);
			frameTimesShape.push_back(now);

			if(frameTimesPose.front() < now - poseSmoothingTime) {
				poseQueue.pop_front();
				frameTimesPose.pop_front();
				poseAvailable = true;
			}

			if(frameTimesShape.front() < now - shapeSmoothingTime) {
				shapeQueue.pop_front();
				frameTimesShape.pop_front();
				shapeAvailable = true;
			}

		} else {
			// Tracking failed. Clear the smoothing queues.

			frameTimesPose.clear();
			frameTimesShape.clear();
			poseQueue.clear();
			shapeQueue.clear();

			poseAvailable = false;
			shapeAvailable = false;
		}

		// Do smoothing

		if (poseAvailable && shapeAvailable) {

			// Take average of shape params over smoothing window.
			smoothedShape = Mat::zeros(shapeQueue[0].rows, shapeQueue[0].cols, shapeQueue[0].type());
			
			for (int i = 0; i < shapeQueue.size(); i++)
			{
				smoothedShape += shapeQueue[i];
			}

			smoothedShape /= shapeQueue.size();

			// Calculate SD of pose params over smoothing window.
			Vec6d means;
			for (int i = 0; i < poseQueue.size(); i++)
			{
				means += poseQueue[i];
			}
			means /= (double)poseQueue.size();

			Vec6d diffs;
			for (int i = 0; i < poseQueue.size(); i++)
			{
				Vec6d diff = (poseQueue[i] - means);
				diffs += diff.mul(diff);
			}

			diffs /= (double)poseQueue.size();

			cv::sqrt(diffs,smoothedPose);

			// Write features to files.
			fprintf(fpA, "%f", currentFrameLabelA);
			fprintf(fpE, "%f", currentFrameLabelE);
			fprintf(fpP, "%f", currentFrameLabelP);
			fprintf(fpV, "%f", currentFrameLabelV);
			int j = 1;
			for (int i = 0; i < smoothedShape.rows; i++)
			{
				double val = smoothedShape.at<double>(i);
				fprintf4(fpA, fpE, fpP, fpV, " %d: %f", j++, val);
			}
			for (int i = 0; i < smoothedPose.rows; i++)
			{
				double val = smoothedPose[i];
				fprintf4(fpA, fpE, fpP, fpV, " %d: %f", j++, val);
			}

			fprintf4(fpA, fpE, fpP, fpV, "\n");

		}

		// FPS calculation

		frameTimesFps.push_back(now);
		while(frameTimesFps.front() < now - 3)
			frameTimesFps.pop_front(); // Only keep history of the last 3 seconds.

		double fps = frameTimesFps.size() / (now - frameTimesFps.front());

		char fpsC[255];
		sprintf(fpsC, "Frame %d of %d (%d %%, %d FPS)", frameCount, totalFrames, ((int)(100*(frameCount / (double)totalFrames))), (int)fps);
		string fpsSt(fpsC);

		cv::putText(capturedImage, fpsSt, cv::Point(10,20), CV_FONT_HERSHEY_SIMPLEX, 0.5, CV_RGB(255,0,0));		

		// Display tracked image.
		imshow("Tracking", capturedImage);

		// detect key presses
		char characterPress = cv::waitKey(1);

		// quit the application
		if(characterPress == 'q')
		{
			break;
		}
	}

}

int main (int argc, char **argv)
{
	for (int i = 1; i < 33; i++)
	{
		char title[1024];
		sprintf(title, "Title Video %d of 63", i);
		system(title);

		char vid[1024];
		char labelA[1024];
		char labelE[1024];
		char labelP[1024];
		char labelV[1024];

		sprintf(vid, "E:\\localdata\\ASC-Inclusion\\avec2012\\data\\devel_video%03d.avi", i);
		sprintf(labelA, "E:\\localdata\\ASC-Inclusion\\avec2012\\data\\labels_continuous_devel%03d_arousal.dat", i);
		sprintf(labelE, "E:\\localdata\\ASC-Inclusion\\avec2012\\data\\labels_continuous_devel%03d_expectancy.dat", i);
		sprintf(labelP, "E:\\localdata\\ASC-Inclusion\\avec2012\\data\\labels_continuous_devel%03d_power.dat", i);
		sprintf(labelV, "E:\\localdata\\ASC-Inclusion\\avec2012\\data\\labels_continuous_devel%03d_valence.dat", i);

		extractFeatures(vid,
						// LABEL FILES
						labelA,
						labelE,
						labelP,
						labelV,
						// OUTPUT FILES
						"E:\\localdata\\ASC-Inclusion\\avec2012\\featuresFast\\ipd21_features.arousal.libsvm_raw",
						"E:\\localdata\\ASC-Inclusion\\avec2012\\featuresFast\\ipd21_features.expectancy.libsvm_raw",
						"E:\\localdata\\ASC-Inclusion\\avec2012\\featuresFast\\ipd21_features.power.libsvm_raw",
						"E:\\localdata\\ASC-Inclusion\\avec2012\\featuresFast\\ipd21_features.valence.libsvm_raw");
	}

	for (int i = 1; i < 32; i++)
	{
		char title[1024];
		sprintf(title, "Title Video %d of 63", i + 32);
		system(title);

		char vid[1024];
		char labelA[1024];
		char labelE[1024];
		char labelP[1024];
		char labelV[1024];

		sprintf(vid, "E:\\localdata\\ASC-Inclusion\\avec2012\\data\\train_video%03d.avi", i);
		sprintf(labelA, "E:\\localdata\\ASC-Inclusion\\avec2012\\data\\labels_continuous_train%03d_arousal.dat", i);
		sprintf(labelE, "E:\\localdata\\ASC-Inclusion\\avec2012\\data\\labels_continuous_train%03d_expectancy.dat", i);
		sprintf(labelP, "E:\\localdata\\ASC-Inclusion\\avec2012\\data\\labels_continuous_train%03d_power.dat", i);
		sprintf(labelV, "E:\\localdata\\ASC-Inclusion\\avec2012\\data\\labels_continuous_train%03d_valence.dat", i);

		extractFeatures(vid,
						// LABEL FILES
						labelA,
						labelE,
						labelP,
						labelV,
						// OUTPUT FILES
						"E:\\localdata\\ASC-Inclusion\\avec2012\\featuresFast\\ipd21_features.arousal.libsvm_raw",
						"E:\\localdata\\ASC-Inclusion\\avec2012\\featuresFast\\ipd21_features.expectancy.libsvm_raw",
						"E:\\localdata\\ASC-Inclusion\\avec2012\\featuresFast\\ipd21_features.power.libsvm_raw",
						"E:\\localdata\\ASC-Inclusion\\avec2012\\featuresFast\\ipd21_features.valence.libsvm_raw");
	}

	cout << "Done. Now run scale_features.bat then train.bat" << endl;
	system("pause");
	return 0;
}

