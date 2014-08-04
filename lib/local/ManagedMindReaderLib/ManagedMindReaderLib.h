// ManagedMindReaderLib.h
#pragma once

#pragma unmanaged
#define _SECURE_SCL 0

#include "activemq/library/ActiveMQCPP.h"
#include <cms/Connection.h>
#include <activemq/core/ActiveMQConnectionFactory.h>
#include <activemq/util/Config.h>
#include "linear.h"

#include <deque>

#include "cv.h"
#include "highgui.h"

#pragma managed
#define WC_NO_BEST_FIT_CHARS      0x00000400  // do not use best fit chars

//#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <msclr\marshal.h>
#include <msclr\marshal_cppstd.h>
#include <ostream>
#include <sstream>
#include <iostream>
#include <queue>

#include <CLM.h>
#include <CLMTracker.h>
#include <CLMParameters.h>
#include <CLM_utils.h>

using namespace msclr::interop;

using namespace OpenCVWrappers;
using namespace System;
using namespace System::Collections::Generic;
using namespace cv;
using namespace std;

namespace ManagedMindReaderLib {

		float feature_mins[33];
		float feature_maxs[33];
		const float expectancy_label_min = -0.359967f;
		const float expectancy_label_max = 87.448900f;

		cms::Connection* amqConnection;
		cms::Session* amqSession;
		cms::Destination* amqDestination;
		cms::MessageProducer* amqProducer;
		cms::MessageConsumer* amqConsumer;

		deque<Mat> poseQueue;
		deque<Mat> shapeQueue;


		model* m_arousal = load_model(".\\avec2012\\models\\arousal.model");
		model* m_valence = load_model(".\\avec2012\\models\\valence.model");
		model* m_power = load_model(".\\avec2012\\models\\power.model");
		model* m_expectancy = load_model(".\\avec2012\\models\\expectancy.model");

		Mat_<double> smoothedShape;
		Vec6d smoothedPose;

		void load_scale_params(const char* filename)
		{
			FILE* fp = fopen(filename,"r");

			char str[80];
			int dummy1, dummy2;

			fscanf(fp, "%80s", str);
			fscanf(fp, "%i %i", &dummy1, &dummy2);

			int ns[33];

			for (int i = 0; i < 33; i++)
			{
				fscanf(fp, "%i %f %f", &ns[i], &feature_mins[i], &feature_maxs[i]);
			}

		}

	public ref class MMR
	{

	private: 
		VideoCapture* vc;
		RawImage^ latestFrame;
		RawImage^ mirroredFrame;

		RawImage^ grey;
		RawImage^ depth_dummy;

		CLMTracker::CLMParameters* clmParams;
		CLMTracker::CLM* clmModel;	

		double poseSmoothingTime;
		double shapeSmoothingTime;

		deque<double>* frameTimesPose;
		deque<double>* frameTimesShape;
		deque<Vec6d>* poseQueue;
		deque<Mat_<double>>* shapeQueue;

		bool shapeAvailable;
		bool poseAvailable;

		int frameNumber;
		bool resetOnNextFrame;

		cms::Connection* amqConnection;
		cms::Session* amqSession;
		cms::Destination* amqSrc;
		cms::Destination* amqDestination;
		cms::MessageProducer* amqProducer;
		cms::MessageConsumer* amqConsumer;

		MjpegServer::Server^ server;

		long long lastAmqMsg;
		int amqInterval;
		bool amqRunning;

		int cameraFrameRate;
		double poseSdWindowSeconds;
		double shapeAvWindowSeconds;

		double now;

		void InitActiveMQ()
		{
			activemq::library::ActiveMQCPP::initializeLibrary();

			// Create a ConnectionFactory
            auto_ptr<cms::ConnectionFactory> connectionFactory(cms::ConnectionFactory::createCMSConnectionFactory("failover:(tcp://localhost:61616)"));

            // Create a Connection
            amqConnection = connectionFactory->createConnection();
            amqConnection->start();

            amqSession = amqConnection->createSession(cms::Session::AUTO_ACKNOWLEDGE);


            amqDestination = amqSession->createTopic("ASC");


            // Create a MessageProducer from the Session to the Topic or Queue
            amqProducer = amqSession->createProducer(amqDestination);
            amqProducer->setDeliveryMode(cms::DeliveryMode::NON_PERSISTENT);
			amqProducer->setTimeToLive(1000);

            string text = (string) "Hello world! From UCAM";

			//std::auto_ptr<cms::MapMessage> mmsg(amqSession->createMapMessage());

			//mmsg->setInt("The best number", 42);
			//mmsg->setString("Some text", "<xml>");
			//std::auto_ptr<cms::TextMessage> message(amqSession->createTextMessage(text));
			//message->setText("Hello");
			//amqProducer->send(mmsg.get(),cms::DeliveryMode::NON_PERSISTENT,1,1000);

			
            amqSrc = amqSession->createQueue("queue/face");
			amqConsumer = amqSession->createConsumer(amqSrc);
			while(true)
			{
				cms::Message* m = amqConsumer->receive(1000);
				if (m)
				{
					cout << "MSG: " << ((cms::TextMessage*)m)->getText() << endl;

					std::stringstream ss(((cms::TextMessage*)m)->getText());
					std::istream_iterator<std::string> begin(ss);
					std::istream_iterator<std::string> end;
					std::vector<std::string> vstrings(begin, end);
					
					for(auto& v: vstrings)
					{
						if (v.size() > 9 && v.substr(0, 9) == "Interval:")
						{
							std::string newInterval = v.substr(9);
							amqInterval = atoi(newInterval.c_str());
							cout << "NEW INTERVAL: " << amqInterval << endl;
						}
						else if (v == "MsgName:start")
						{
							amqRunning = true;
						}
						else if (v == "MsgName:stop")
						{
							amqRunning = false;
						}
					}
					
					delete m;
				}
			}
		    //activemq::library::ActiveMQCPP::shutdownLibrary();
		}
		
	public:

		ref class DimensionResult
		{
		public:
			double a,e,p,v;

			DimensionResult(double a, double e, double p, double v) : a(a), e(e), p(p), v(v) { }
		};

		MMR(int mjpegServerPort) : lastAmqMsg(0), amqInterval(100), amqRunning(false)
		{

			System::Threading::ThreadStart^ ts = gcnew System::Threading::ThreadStart(this, &MMR::InitActiveMQ);
			System::Threading::Thread^ t = gcnew System::Threading::Thread(ts);
			t->IsBackground = true;
			t->Start();


			latestFrame = gcnew RawImage();
			mirroredFrame = gcnew RawImage();
			grey = gcnew RawImage();

			depth_dummy = gcnew RawImage();

			load_scale_params(".\\avec2012\\features\\scale_params.expectancy");

			clmParams = new CLMTracker::CLMParameters();
			clmModel = new CLMTracker::CLM(clmParams->model_location);

			shapeQueue = new deque<Mat_<double>>();
			poseQueue = new deque<Vec6d>();

			frameTimesPose = new deque<double>();
			frameTimesShape = new deque<double>();

			poseSmoothingTime = 3;
			shapeSmoothingTime = 0.2;

			frameNumber = 0;
			resetOnNextFrame = false;

			poseSdWindowSeconds = 3;
			shapeAvWindowSeconds = 0.2;

			if (mjpegServerPort > 0)
			{
				server = gcnew MjpegServer::Server(mjpegServerPort);
				server->Start();
			}
		}

		void Init(int videoInputDevice)
		{
			vc = new VideoCapture(videoInputDevice);
		}

		void Quit()
		{
			delete vc;
			delete clmParams;
			delete clmModel;
			delete shapeQueue;
			delete poseQueue;
			delete frameTimesShape;
			delete frameTimesPose;
		}

		RawImage^ GetNextFrame()
		{
			*vc >> (mirroredFrame->Mat);

			now = cv::getTickCount() / cv::getTickFrequency();

			flip(mirroredFrame->Mat, latestFrame->Mat, 1);

			cvtColor(latestFrame->Mat, grey->Mat, CV_BGR2GRAY);

			frameNumber++;
			
			if(server != nullptr)
			{
				vector<uchar> buf;
				vector<int> param = vector<int>(2);
				param[0]=CV_IMWRITE_JPEG_QUALITY;
				param[1]=75;

				imencode(".jpg", latestFrame->Mat, buf, param);

				array<Byte>^ data_array = gcnew array<Byte>(buf.size());
				for( int i = 0; i < data_array->Length; ++i )
					data_array[i] = buf[i];

				server->EnqueueImage(data_array);
			}
			return latestFrame;
		}

		void ResetMR()
		{
			resetOnNextFrame = true;
		}

		void ImShow(RawImage^ img)
		{
			imshow("Image", img->Mat);
			cvWaitKey(1);
		}

		void ImShow()
		{
			this->ImShow(latestFrame);
		}

		bool TrackCLM()
		{
			bool success = CLMTracker::DetectLandmarksInVideo(grey->Mat, depth_dummy->Mat, *clmModel, *clmParams);

			float fx = 500, fy = 500, cx = latestFrame->Width / 2, cy = latestFrame->Height / 2;

			Vec6d pose_estimate_CLM = CLMTracker::GetCorrectedPoseCameraPlane(*clmModel, fx, fy, cx, cy, *clmParams);

			if (success) {

				// Add shape and pose params to queues.
				poseQueue->push_back(pose_estimate_CLM);
				shapeQueue->push_back(clmModel->params_local.clone());

				double n = this->now;
				frameTimesPose->push_back(n);
				frameTimesShape->push_back(n);

				if(frameTimesPose->front() < now - poseSmoothingTime) {
					poseQueue->pop_front();
					frameTimesPose->pop_front();
					poseAvailable = true;
				}

				if(frameTimesShape->front() < now - shapeSmoothingTime) {
					shapeQueue->pop_front();
					frameTimesShape->pop_front();
					shapeAvailable = true;
				}
			} else {
				shapeQueue->clear();
				poseQueue->clear();
				frameTimesShape->clear();
				frameTimesPose->clear();
				shapeAvailable = false;
				poseAvailable = false;
			}

			return success;
		}

		void DrawCLMResult()
		{
			CLMTracker::Draw(latestFrame->Mat, *clmModel);

			float fx = 500, fy = 500, cx = latestFrame->Width / 2, cy = latestFrame->Height / 2;

			Vec6d pose_estimate_CLM = CLMTracker::GetCorrectedPoseCameraPlane(*clmModel, fx, fy, cx, cy, *clmParams);
			CLMTracker::DrawBox(latestFrame->Mat, pose_estimate_CLM, Scalar(255,0,0), 2, fx, fy, cx, cy);
		}


		void DoSmoothing()
		{
			// Do smoothing

			if (poseAvailable && shapeAvailable) {

				Mat_<double> f = shapeQueue->front();

				// Take average of shape params over smoothing window.
				smoothedShape = Mat::zeros(f.rows, f.cols, f.type());
			
				for (int i = 0; i < shapeQueue->size(); i++)
				{
					smoothedShape += (*shapeQueue)[i];
				}

				smoothedShape /= shapeQueue->size();

				// Calculate SD of pose params over smoothing window.
				Vec6d means;
				for (int i = 0; i < poseQueue->size(); i++)
				{
					means += (*poseQueue)[i];
				}
				means /= (double)poseQueue->size();

				Vec6d diffs;
				for (int i = 0; i < poseQueue->size(); i++)
				{
					Vec6d diff = ((*poseQueue)[i] - means);
					diffs += diff.mul(diff);
				}

				diffs /= (double)poseQueue->size();

				cv::sqrt(diffs, smoothedPose);
			}
		}

		property bool PredictionAvailable
		{
			bool get() {
				return shapeAvailable && poseAvailable;
			}
		}

		property bool IsRunning
		{
			bool get() {
				return amqRunning;
			}
		}

		long long unixNow()
		{
			System::DateTime^ unixRef = gcnew System::DateTime(1970, 1, 1, 0, 0, 0);
			long long now = System::DateTime::Now.Ticks;
			long long ts = (now - unixRef->Ticks) / 10000;
			return ts;
		}

		DimensionResult^ PredictDimensions()
		{
			DimensionResult^ r = nullptr;
			
			if (shapeAvailable && poseAvailable)
			{
				//cout<< "DIMENSIONS AVAILABLE" << endl;
				feature_node* x = Malloc(feature_node, 34);

				int j = 0;
				for (int i = 0; i < smoothedShape.rows; i++)
				{
					double val = *smoothedShape[i];
					double scaled_val = (((val - feature_mins[j]) / (feature_maxs[j] - feature_mins[j]))-0.5) * 2;
					x[j].index = j+1;
					x[j].value = val;
					j++;
				}
				for (int i = 0; i < smoothedPose.rows; i++)
				{
					double val = smoothedPose[i];
					double scaled_val = (((val - feature_mins[j]) / (feature_maxs[j] - feature_mins[j]))-0.5) * 2;
					x[j].index = j+1;
					x[j].value = val;
					j++;
				}
				x[j].index=-1;

				double a = predict(m_arousal, x) / 10.0;
				double e = (((predict(m_expectancy, x) - expectancy_label_min) / (expectancy_label_max - expectancy_label_min)) - 0.5) * 2;
				double p = predict(m_power, x);
				double v = predict(m_valence, x) / 10.0;
				
				r = gcnew DimensionResult(a,e,p,v);

				//cout << "A: " << a  << " E: " << e << " P: " << p << " V: " << v << endl;
				//cout << "Sending" << endl;

				long long now = unixNow();

				if (amqSession && (now - lastAmqMsg) > amqInterval)
				{
					std::auto_ptr<cms::TextMessage> tmsg(amqSession->createTextMessage());
					std::ostringstream ss;
					ss << "<emotionml version=\"1.0\" xmlns=\"http://www.w3.org/2009/10/emotionml\">" <<
									"	<emotion dimension-set=\"http://somewhere.com/ucamFaceVocab.xml#dimensions\" start=\"" << "\">"
									<< "		<dimension name=\"Arousal\" value=\"" << a << "\"/>"
									<< "		<dimension name=\"Expectancy\" value=\"" << e << "\"/>"
									<< "		<dimension name=\"Power\" value=\"" << p << "\"/>"
									<< "		<dimension name=\"Valence\" value=\"" << v << "\"/>"
									<< "	</emotion>"
									<< "</emotionml>";

					tmsg->setText( ss.str().c_str() );
					amqProducer->send(tmsg.get(),cms::DeliveryMode::NON_PERSISTENT,1,10000);
					lastAmqMsg = now;
				}
			}
			
			return r;
		}

	};
}
