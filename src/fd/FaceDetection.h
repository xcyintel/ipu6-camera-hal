/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#ifdef FACE_DETECTION
#ifdef ENABLE_SANDBOXING
#include "modules/sandboxing/client/IntelFaceDetectionClient.h"
#else
#include "modules/algowrapper/IntelFaceDetection.h"
#endif
#include "iutils/Errors.h"
#include "iutils/Thread.h"
#endif

#include <ia_types.h>

#include <memory>
#include <queue>
#include <unordered_map>

#ifdef CAL_BUILD
#include "Camera3Buffer.h"
#endif
#include "FaceBase.h"
#include "IntelCCATypes.h"
#include "Parameters.h"
#include "iutils/Utils.h"

namespace icamera {

#ifdef FACE_DETECTION
class FaceDetection : public Thread {
 public:
    FaceDetection(int cameraId, unsigned int maxFaceNum, int32_t halStreamId, int width,
                  int height);
    ~FaceDetection();

    static FaceDetection* createInstance(int cameraId, unsigned int maxFaceNum, int32_t halStreamId,
                                         int width, int height);
    static void destoryInstance(int cameraId);
    static FaceDetection* getInstance(int cameraId);

    void runFaceDetection(const std::shared_ptr<camera3::Camera3Buffer> ccBuf);
    void runFaceDetectionBySync(const std::shared_ptr<camera3::Camera3Buffer> ccBuf);
    void runFaceDetectionByAsync(const std::shared_ptr<camera3::Camera3Buffer> ccBuf);
    virtual bool threadLoop();
    static int getResult(int cameraId, cca::cca_face_state* faceState);
    static int getResult(int cameraId, CVFaceDetectionAbstractResult* result);

 private:
    bool isInitialized() { return mInitialized; }

    bool faceRunningByCondition();
    void printfFDRunRate();
    int getFaceDetectionResult(FaceDetectionResult* mResult, bool resetRes = false);
    int getFaceNum();
    FaceDetectionRunParams* acquireRunBuf();
    void returnRunBuf(FaceDetectionRunParams* memRunBuf);
    void getCurrentFrameWidthAndHight(int* frameWidth, int* frameHigth);
    void getHalStreamId(int32_t* halStreamId);

    // Guard for face engine instance
    static Mutex sLock;
    static std::unordered_map<int, FaceDetection*> sInstances;

    int mCameraId;
    bool mInitialized;

    // Guard for face detection result
    std::mutex mFaceResultLock;
    FaceDetectionResult mResult;

    std::unique_ptr<IntelFaceDetection> mFace;

    std::condition_variable mRunCondition;
    // Guard for running buffer queue of thread
    std::mutex mRunBufQueueLock;
    std::queue<FaceDetectionRunParams*> mRunBufQueue;
    const uint64_t kMaxDuration = 2000000000;  // 2000ms

    // Guard for running buffer pool of face engine
    std::mutex mMemRunPoolLock;
    std::queue<FaceDetectionRunParams*> mMemRunPool;

    int mSensorOrientation;

    int32_t mHalStreamId;
    int mWidth;
    int mHeight;

    unsigned int mFDRunDefaultInterval;  // FD running's interval frames.
    unsigned int mFDRunIntervalNoFace;   // FD running's interval frames without face.
    unsigned int mFDRunInterval;         // run 1 frame every mFDRunInterval frames.
    unsigned int mFrameCnt;  // from 0 to (mFDRunInterval - 1).

    unsigned int mRunCount;
    timeval mRequestRunTime;

    DISALLOW_COPY_AND_ASSIGN(FaceDetection);
};
#else
class FaceDetection {
 public:
    static int getResult(int cameraId, cca::cca_face_state* faceState) {
        faceState->num_faces = 0;
        return 0;
    }
    static int getResult(int cameraId, CVFaceDetectionAbstractResult* result) {
        CLEAR(*result);
        return 0;
    }
    static FaceDetection* createInstance(int cameraId, unsigned int maxFaceNum, int32_t halStreamId,
                                         int width, int height) {
        return nullptr;
    }
    static void destoryInstance(int cameraId) {}
#ifdef CAL_BUILD
    void runFaceDetection(const std::shared_ptr<camera3::Camera3Buffer> ccBuf) {}
#endif
};
#endif

}  // namespace icamera
