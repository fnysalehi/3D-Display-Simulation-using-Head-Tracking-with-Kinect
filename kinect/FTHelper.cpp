//------------------------------------------------------------------------------
// <copyright file="Tracker.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#include "Tracker.h"
#include "KinectSensor.h"

Tracker::Tracker()
{
    m_pFaceTracker = 0;
    m_pFTResult = NULL;
    m_colorImage = NULL;
    m_depthImage = NULL;
    m_LastTrackSucceeded = false;
}

Tracker::~Tracker()
{
}

void Tracker::Init()
{
	FT_CAMERA_CONFIG videoConfig = { 640, 480, NUI_CAMERA_COLOR_NOMINAL_FOCAL_LENGTH_IN_PIXELS };
	FT_CAMERA_CONFIG depthConfig = { 320, 240, NUI_CAMERA_DEPTH_NOMINAL_FOCAL_LENGTH_IN_PIXELS };

	// Try to get the Kinect camera to work
	m_KinectSensor = new KinectSensor();
	m_KinectSensor->Init();

	m_hint3D[0] = m_hint3D[1] = FT_VECTOR3D(0, 0, 0);

	// Try to start the face tracker.
	m_pFaceTracker = FTCreateFaceTracker();
	m_pFaceTracker->Initialize(&videoConfig, &depthConfig, NULL, NULL);
	m_pFaceTracker->CreateFTResult(&m_pFTResult);

	// Initialize the RGB image.
	m_colorImage = FTCreateImage();
	m_depthImage = FTCreateImage();

	m_LastTrackSucceeded = false;
}

void Tracker::Destroy()
{
	m_pFaceTracker->Release();
	m_pFaceTracker = NULL;

	m_colorImage->Release();
	m_colorImage = NULL;

	m_depthImage->Release();
	m_depthImage = NULL;

	m_pFTResult->Release();
	m_pFTResult = NULL;

	m_KinectSensor->Release();
	delete m_KinectSensor;
}

void Tracker::SubmitFraceTrackingResult(IFTResult* pResult)
{
}

// Get a video image and process it.
void Tracker::CheckCameraInput()
{
    HRESULT hrFT = E_FAIL;

    if (m_KinectSensor->GetVideoBuffer())
    {
        m_KinectSensor->GetVideoBuffer()->CopyTo(m_colorImage, NULL, 0, 0);
        m_KinectSensor->GetDepthBuffer()->CopyTo(m_depthImage, NULL, 0, 0);
        
    	// Do face tracking
        FT_SENSOR_DATA sensorData(m_colorImage, m_depthImage, m_KinectSensor->GetZoomFactor(), m_KinectSensor->GetViewOffSet());

        FT_VECTOR3D* hint = NULL;
        if (SUCCEEDED(m_KinectSensor->GetClosestHint(m_hint3D)))
        {
            hint = m_hint3D;
        }
        if (m_LastTrackSucceeded)
        {
            hrFT = m_pFaceTracker->ContinueTracking(&sensorData, hint, m_pFTResult);
        }
        else
        {
            hrFT = m_pFaceTracker->StartTracking(&sensorData, NULL, hint, m_pFTResult);
        }
    }

    m_LastTrackSucceeded = SUCCEEDED(hrFT) && SUCCEEDED(m_pFTResult->GetStatus());
    if (m_LastTrackSucceeded)
    {
        SubmitFraceTrackingResult(m_pFTResult);
    }
    else
    {
        m_pFTResult->Reset();
    }
}
