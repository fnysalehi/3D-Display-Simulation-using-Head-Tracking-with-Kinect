//------------------------------------------------------------------------------
// <copyright file="Tracker.h" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#pragma once

#include <FaceTrackLib.h>

class Tracker
{
public:
    Tracker();
    ~Tracker();

    void Init();
    void Destroy();

	bool LastTrackSucceeded()    { return m_LastTrackSucceeded; }
    IFTResult* GetResult()       { return m_pFTResult;}
    IFTImage* GetColorImage()    { return m_colorImage;}
    IFTFaceTracker* GetTracker() { return m_pFaceTracker;}

	void Update();

private:
    class KinectSensor*         m_KinectSensor;
    IFTFaceTracker*             m_pFaceTracker;
    IFTResult*                  m_pFTResult;
    IFTImage*                   m_colorImage;
    IFTImage*                   m_depthImage;
    FT_VECTOR3D                 m_hint3D[2];
    bool                        m_LastTrackSucceeded;
};
