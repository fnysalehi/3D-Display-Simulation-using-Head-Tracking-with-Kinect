//------------------------------------------------------------------------------
// <copyright file="KinectSensor.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#include "KinectSensor.h"
#include <cmath>

KinectSensor::KinectSensor()
{
    m_hNextDepthFrameEvent = NULL;
    m_hNextVideoFrameEvent = NULL;
    m_hNextSkeletonEvent = NULL;
    m_pDepthStreamHandle = NULL;
    m_pVideoStreamHandle = NULL;
    m_hThNuiProcess=NULL;
    m_hEvNuiProcessStop=NULL;
    m_bNuiInitialized = false;
    m_FramesTotal = 0;
    m_SkeletonTotal = 0;
    m_VideoBuffer = NULL;
    m_DepthBuffer = NULL;
    m_ZoomFactor = 1.0f;
    m_ViewOffset.x = 0;
    m_ViewOffset.y = 0;
}

KinectSensor::~KinectSensor()
{
    Release();
}

void KinectSensor::Init()
{
    Release(); // Deal with double initializations.

    m_VideoBuffer = FTCreateImage();
    m_VideoBuffer->Allocate(640, 480, FTIMAGEFORMAT_UINT8_B8G8R8X8);

    m_DepthBuffer = FTCreateImage();
    m_DepthBuffer->Allocate(320, 240, FTIMAGEFORMAT_UINT16_D13P3);
    
    m_FramesTotal = 0;
    m_SkeletonTotal = 0;

    for (int i = 0; i < NUI_SKELETON_COUNT; ++i)
    {
        m_HeadPoint[i] = m_NeckPoint[i] = FT_VECTOR3D(0, 0, 0);
        m_SkeletonTracked[i] = false;
    }

    m_hNextDepthFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_hNextVideoFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_hNextSkeletonEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    
    NuiInitialize(NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_SKELETON | NUI_INITIALIZE_FLAG_USES_COLOR);
    m_bNuiInitialized = true;

    NuiSkeletonTrackingEnable( m_hNextSkeletonEvent, NUI_SKELETON_TRACKING_FLAG_ENABLE_IN_NEAR_RANGE | NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT);

    NuiImageStreamOpen(
		NUI_IMAGE_TYPE_COLOR,
		NUI_IMAGE_RESOLUTION_640x480,
        0,
        2,
        m_hNextVideoFrameEvent,
        &m_pVideoStreamHandle );
    
	NuiImageStreamOpen(
		NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX,
		NUI_IMAGE_RESOLUTION_320x240,
        NUI_IMAGE_STREAM_FLAG_ENABLE_NEAR_MODE,
        2,
        m_hNextDepthFrameEvent,
        &m_pDepthStreamHandle );
    
	// Start the Nui processing thread
    m_hEvNuiProcessStop=CreateEvent(NULL,TRUE,FALSE,NULL);
    m_hThNuiProcess=CreateThread(NULL,0,ProcessThread,this,0,NULL);
}

void KinectSensor::Release()
{
    // Stop the Nui processing thread
    if(m_hEvNuiProcessStop!=NULL)
    {
        // Signal the thread
        SetEvent(m_hEvNuiProcessStop);

        // Wait for thread to stop
        if(m_hThNuiProcess!=NULL)
        {
            WaitForSingleObject(m_hThNuiProcess,INFINITE);
            CloseHandle(m_hThNuiProcess);
            m_hThNuiProcess = NULL;
        }
        CloseHandle(m_hEvNuiProcessStop);
        m_hEvNuiProcessStop = NULL;
    }

    if (m_bNuiInitialized)
    {
        NuiShutdown();
    }
    m_bNuiInitialized = false;

    if (m_hNextSkeletonEvent && m_hNextSkeletonEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hNextSkeletonEvent);
        m_hNextSkeletonEvent = NULL;
    }
    if (m_hNextDepthFrameEvent && m_hNextDepthFrameEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hNextDepthFrameEvent);
        m_hNextDepthFrameEvent = NULL;
    }
    if (m_hNextVideoFrameEvent && m_hNextVideoFrameEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hNextVideoFrameEvent);
        m_hNextVideoFrameEvent = NULL;
    }
    if (m_VideoBuffer)
    {
        m_VideoBuffer->Release();
        m_VideoBuffer = NULL;
    }
    if (m_DepthBuffer)
    {
        m_DepthBuffer->Release();
        m_DepthBuffer = NULL;
    }
}

DWORD WINAPI KinectSensor::ProcessThread(LPVOID pParam)
{
    KinectSensor*  pthis=(KinectSensor *) pParam;
    HANDLE          hEvents[4];

    // Configure events to be listened on
    hEvents[0]=pthis->m_hEvNuiProcessStop;
    hEvents[1]=pthis->m_hNextDepthFrameEvent;
    hEvents[2]=pthis->m_hNextVideoFrameEvent;
    hEvents[3]=pthis->m_hNextSkeletonEvent;

    // Main thread loop
    while (true)
    {
        // Wait for an event to be signaled
        WaitForMultipleObjects(sizeof(hEvents)/sizeof(hEvents[0]),hEvents,FALSE,100);

        // If the stop event is set, stop looping and exit
        if (WAIT_OBJECT_0 == WaitForSingleObject(pthis->m_hEvNuiProcessStop, 0))
        {
            break;
        }

        // Process signal events
        if (WAIT_OBJECT_0 == WaitForSingleObject(pthis->m_hNextDepthFrameEvent, 0))
        {
            pthis->GotDepthAlert();
            pthis->m_FramesTotal++;
        }
        if (WAIT_OBJECT_0 == WaitForSingleObject(pthis->m_hNextVideoFrameEvent, 0))
        {
            pthis->GotVideoAlert();
        }
        if (WAIT_OBJECT_0 == WaitForSingleObject(pthis->m_hNextSkeletonEvent, 0))
        {
            pthis->GotSkeletonAlert();
            pthis->m_SkeletonTotal++;
        }
    }

    return 0;
}

void KinectSensor::GotVideoAlert( )
{
    const NUI_IMAGE_FRAME* pImageFrame = NULL;

    bool hr = NuiImageStreamGetNextFrame(m_pVideoStreamHandle, 0, &pImageFrame);
    if (FAILED(hr))
    {
        return;
    }

    INuiFrameTexture* pTexture = pImageFrame->pFrameTexture;
    NUI_LOCKED_RECT LockedRect;
    pTexture->LockRect(0, &LockedRect, NULL, 0);
    if (LockedRect.Pitch)
    {   // Copy video frame to face tracking
        memcpy(m_VideoBuffer->GetBuffer(), PBYTE(LockedRect.pBits), min(m_VideoBuffer->GetBufferSize(), UINT(pTexture->BufferLen())));
    }
    else
    {
        OutputDebugString(L"Buffer length of received texture is bogus\r\n");
    }

    hr = NuiImageStreamReleaseFrame(m_pVideoStreamHandle, pImageFrame);
}


void KinectSensor::GotDepthAlert( )
{
    const NUI_IMAGE_FRAME* pImageFrame = NULL;

    bool hr = NuiImageStreamGetNextFrame(m_pDepthStreamHandle, 0, &pImageFrame);

    if (FAILED(hr))
    {
        return;
    }

    INuiFrameTexture* pTexture = pImageFrame->pFrameTexture;
    NUI_LOCKED_RECT LockedRect;
    pTexture->LockRect(0, &LockedRect, NULL, 0);
    if (LockedRect.Pitch)
    {   // Copy depth frame to face tracking
        memcpy(m_DepthBuffer->GetBuffer(), PBYTE(LockedRect.pBits), min(m_DepthBuffer->GetBufferSize(), UINT(pTexture->BufferLen())));
    }
    else
    {
        OutputDebugString( L"Buffer length of received depth texture is bogus\r\n" );
    }

    hr = NuiImageStreamReleaseFrame(m_pDepthStreamHandle, pImageFrame);
}

void KinectSensor::GotSkeletonAlert()
{
    NUI_SKELETON_FRAME SkeletonFrame = {0};

    bool hr = NuiSkeletonGetNextFrame(0, &SkeletonFrame);
    if(FAILED(hr))
    {
        return;
    }

    for( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
    {
        if( SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED &&
            NUI_SKELETON_POSITION_TRACKED == SkeletonFrame.SkeletonData[i].eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HEAD] &&
            NUI_SKELETON_POSITION_TRACKED == SkeletonFrame.SkeletonData[i].eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_CENTER])
        {
            m_SkeletonTracked[i] = true;
            m_HeadPoint[i].x = SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HEAD].x;
            m_HeadPoint[i].y = SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HEAD].y;
            m_HeadPoint[i].z = SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HEAD].z;
            m_NeckPoint[i].x = SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].x;
            m_NeckPoint[i].y = SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].y;
            m_NeckPoint[i].z = SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].z;
        }
        else
        {
            m_HeadPoint[i] = m_NeckPoint[i] = FT_VECTOR3D(0, 0, 0);
            m_SkeletonTracked[i] = false;
        }
    }
}

bool KinectSensor::GetClosestHint(FT_VECTOR3D* pHint3D)
{
    int selectedSkeleton = -1;
    float smallestDistance = 0;

    if (pHint3D[1].x == 0 && pHint3D[1].y == 0 && pHint3D[1].z == 0)
    {
        // Get the skeleton closest to the camera
        for (int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
        {
            if (m_SkeletonTracked[i] && (smallestDistance == 0 || m_HeadPoint[i].z < smallestDistance))
            {
                smallestDistance = m_HeadPoint[i].z;
                selectedSkeleton = i;
            }
        }
    }
    else
    {   // Get the skeleton closest to the previous position
        for (int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
        {
            if (m_SkeletonTracked[i])
            {
                float d = fabs(m_HeadPoint[i].x - pHint3D[1].x) +
                    fabs(m_HeadPoint[i].y - pHint3D[1].y) +
                    fabs(m_HeadPoint[i].z - pHint3D[1].z);
                if (smallestDistance == 0 || d < smallestDistance)
                {
                    smallestDistance = d;
                    selectedSkeleton = i;
                }
            }
        }
    }
    if (selectedSkeleton == -1)
    {
        return false;
    }

    pHint3D[0] = m_NeckPoint[selectedSkeleton];
    pHint3D[1] = m_HeadPoint[selectedSkeleton];

    return true;
}

