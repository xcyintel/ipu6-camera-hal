/*
 * Copyright (C) 2015-2021 Intel Corporation.
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

#define LOG_TAG CameraDevice

#include <vector>
#include <ia_pal_types_isp_ids_autogen.h>

#include "iutils/Utils.h"
#include "iutils/CameraLog.h"

#include "IGraphConfig.h"
#include "ICamera.h"
#include "AiqUtils.h"
#include "PlatformData.h"
#include "CameraDevice.h"
#include "V4l2DeviceFactory.h"
#include "I3AControlFactory.h"
#include "CaptureUnit.h"

using std::vector;

namespace icamera {
CameraDevice::CameraDevice(int cameraId) :
    mState(DEVICE_UNINIT),
    mCameraId(cameraId),
    mStreamNum(0),
    mLastSceneMode(SCENE_MODE_AUTO),
    mCallback(nullptr)
{
    PERF_CAMERA_ATRACE();
    LOG1("@%s, cameraId:%d", __func__, mCameraId);

    CLEAR(mStreams);

    V4l2DeviceFactory::createDeviceFactory(mCameraId);
    CLEAR(mInputConfig);
    mInputConfig.format = -1;

    mProducer = createBufferProducer();

    mSofSource = new SofSource(mCameraId);

    mPerframeControlSupport = PlatformData::isFeatureSupported(mCameraId, PER_FRAME_CONTROL);
    LOG2("%s: support perframe %d", __func__, mPerframeControlSupport);
    mParamGenerator = new ParameterGenerator(mCameraId);

    mLensCtrl = new LensHw(mCameraId);
    mSensorCtrl = SensorHwCtrl::createSensorCtrl(mCameraId);

    m3AControl = I3AControlFactory::createI3AControl(mCameraId, mSensorCtrl, mLensCtrl,
                                                     mParamGenerator);
    mRequestThread = new RequestThread(mCameraId, m3AControl, mParamGenerator);
    mRequestThread->registerListener(EVENT_PROCESS_REQUEST, this);
    mRequestThread->registerListener(EVENT_DEVICE_RECONFIGURE, this);

    mProcessorManager = new ProcessorManager(mCameraId);

    if (PlatformData::getGraphConfigNodes(mCameraId)) {
        mGCM = IGraphConfigManager::getInstance(mCameraId);
    } else {
        mGCM = nullptr;
    }
}

CameraDevice::~CameraDevice()
{
    PERF_CAMERA_ATRACE();
    LOG1("@%s, mCameraId:%d", __func__, mCameraId);
    AutoMutex   m(mDeviceLock);

    // Clear the media control when close the device.
    MediaControl *mc = MediaControl::getInstance();
    MediaCtlConf *mediaCtl = PlatformData::getMediaCtlConf(mCameraId);
    if (mc && mediaCtl) {
        mc->mediaCtlClear(mCameraId, mediaCtl);
    }

    mRequestThread->removeListener(EVENT_PROCESS_REQUEST, this);
    mRequestThread->removeListener(EVENT_DEVICE_RECONFIGURE, this);

    delete mProcessorManager;

    for (int i = 0; i < MAX_STREAM_NUMBER; i++)
        delete mStreams[i];

    delete mLensCtrl;
    delete m3AControl;
    delete mSensorCtrl;
    delete mParamGenerator;
    delete mSofSource;
    delete mProducer;
    delete mRequestThread;

    V4l2DeviceFactory::releaseDeviceFactory(mCameraId);
    IGraphConfigManager::releaseInstance(mCameraId);
}

int CameraDevice::init()
{
    PERF_CAMERA_ATRACE();
    LOG1("@%s, mCameraId:%d, mState:%d", __func__, mCameraId, mState);
    AutoMutex   m(mDeviceLock);

    int ret = mProducer->init();
    CheckAndLogError(ret < 0, ret, "%s: Init capture unit failed", __func__);

    ret = mSofSource->init();
    CheckAndLogError(ret != OK, ret, "@%s: init sync manager failed", __func__);

    initDefaultParameters();

    ret = m3AControl->init();
    CheckAndLogError((ret != OK), ret, "%s: Init 3A Unit falied", __func__);

    ret = mLensCtrl->init();
    CheckAndLogError((ret != OK), ret, "%s: Init Lens falied", __func__);

    mRequestThread->run("RequestThread", PRIORITY_NORMAL);

    mState = DEVICE_INIT;
    return ret;
}

void CameraDevice::deinit()
{
    PERF_CAMERA_ATRACE();
    LOG1("@%s, mCameraId:%d, mState:%d", __func__, mCameraId, mState);
    AutoMutex   m(mDeviceLock);

    //deinit should not be call in UNINIT or START STATE
    if (mState == DEVICE_UNINIT) {
        //Do nothing
        return;
    }

    m3AControl->stop();

    if (mState == DEVICE_START) {
        //stop first
        stopLocked();
    }

    // stop request thread
    mRequestThread->requestExit();
    mRequestThread->join();

    deleteStreams();

    mProcessorManager->deleteProcessors();

    m3AControl->deinit();

    mSofSource->deinit();

    mProducer->deinit();

    mState = DEVICE_UNINIT;
}

void CameraDevice::callbackRegister(const camera_callback_ops_t* callback)
{
    mCallback = const_cast<camera_callback_ops_t*>(callback);
}

StreamSource* CameraDevice::createBufferProducer()
{

    return new CaptureUnit(mCameraId);
}

void CameraDevice::bindListeners()
{
    LOG1("@%s, mCameraId:%d", __func__, mCameraId);

    vector<EventListener*> statsListenerList = m3AControl->getStatsEventListener();
    for (auto statsListener : statsListenerList) {

        for (auto& item : mProcessors) {
            // Subscribe PSys statistics.
            item->registerListener(EVENT_PSYS_STATS_BUF_READY, statsListener);
            item->registerListener(EVENT_PSYS_STATS_SIS_BUF_READY, statsListener);
        }
    }
        for (auto& item : mProcessors) {
            item->registerListener(EVENT_PSYS_STATS_BUF_READY, mRequestThread);
        }

    vector<EventListener*> sofListenerList = m3AControl->getSofEventListener();
    for (auto sofListener : sofListenerList) {
        mSofSource->registerListener(EVENT_ISYS_SOF, sofListener);
    }

    if (PlatformData::psysAlignWithSof(mCameraId)) {
        for (auto& item : mProcessors) {
            mSofSource->registerListener(EVENT_ISYS_SOF, item);
        }
    }

    if (mPerframeControlSupport || !PlatformData::isIsysEnabled(mCameraId)) {
        mProcessors.back()->registerListener(EVENT_PSYS_FRAME, mRequestThread);
    } else {
        mProducer->registerListener(EVENT_ISYS_FRAME, mRequestThread);
    }

    if (!mProcessors.empty()) {
        mProcessors.front()->registerListener(EVENT_PSYS_REQUEST_BUF_READY, this);
        mProcessors.front()->registerListener(EVENT_REQUEST_METADATA_READY, this);
    }

    mSofSource->registerListener(EVENT_ISYS_SOF, mRequestThread);
}

void CameraDevice::unbindListeners()
{
    LOG1("@%s, mCameraId:%d", __func__, mCameraId);

    vector<EventListener*> statsListenerList = m3AControl->getStatsEventListener();
    for (auto statsListener : statsListenerList) {

        for (auto& item : mProcessors) {
            item->removeListener(EVENT_PSYS_STATS_BUF_READY, statsListener);
            item->removeListener(EVENT_PSYS_STATS_SIS_BUF_READY, statsListener);
        }
    }
        for (auto& item : mProcessors) {
            item->removeListener(EVENT_PSYS_STATS_BUF_READY, mRequestThread);
        }

    vector<EventListener*> sofListenerList = m3AControl->getSofEventListener();
    for (auto sofListener : sofListenerList) {
        mSofSource->removeListener(EVENT_ISYS_SOF, sofListener);
    }

    if (PlatformData::psysAlignWithSof(mCameraId)) {
        for (auto& item : mProcessors) {
            mSofSource->removeListener(EVENT_ISYS_SOF, item);
        }
    }

    if (!mProcessors.empty()) {
        mProcessors.front()->removeListener(EVENT_PSYS_REQUEST_BUF_READY, this);
        mProcessors.front()->removeListener(EVENT_REQUEST_METADATA_READY, this);
    }

    if (mPerframeControlSupport || !PlatformData::isIsysEnabled(mCameraId)) {
        mProcessors.back()->removeListener(EVENT_PSYS_FRAME, mRequestThread);
    } else {
        mProducer->removeListener(EVENT_ISYS_FRAME, mRequestThread);
    }

    mSofSource->removeListener(EVENT_ISYS_SOF, mRequestThread);
}

int CameraDevice::configureInput(const stream_t *inputConfig)
{
    AutoMutex lock(mDeviceLock);
    mInputConfig = *inputConfig;

    return OK;
}

int CameraDevice::configure(stream_config_t *streamList)
{
    PERF_CAMERA_ATRACE();

    int numOfStreams = streamList->num_streams;
    CheckAndLogError(!streamList->streams, BAD_VALUE, "%s: No valid stream config", __func__);
    CheckAndLogError(numOfStreams > MAX_STREAM_NUMBER || numOfStreams <= 0, BAD_VALUE,
                     "%s: The requested stream number(%d) is invalid. Should be between [1-%d]",
                     __func__, numOfStreams, MAX_STREAM_NUMBER);

    AutoMutex lock(mDeviceLock);

    CheckAndLogError((mState != DEVICE_STOP) && (mState != DEVICE_INIT) &&
                     (mState != DEVICE_CONFIGURE),
                     INVALID_OPERATION, "%s: Add streams in wrong state %d", __func__, mState);

    mRequestThread->configure(streamList);

    // Use concrete ISP mode from request thread for full and auto switch
    if (PlatformData::getAutoSwitchType(mCameraId) == AUTO_SWITCH_FULL &&
        (ConfigMode)(streamList->operation_mode) == CAMERA_STREAM_CONFIGURATION_MODE_AUTO) {
        stream_config_t requestStreamList = mRequestThread->getStreamConfig();
        LOG2("%s: for full and auto switch, use concrete config mode %u from request thread.",
             __func__, requestStreamList.operation_mode);
        return configureL(&requestStreamList);
    }

    return configureL(streamList);
}

int CameraDevice::configureL(stream_config_t *streamList, bool clean)
{
    LOG1("@%s, mCameraId:%d, operation_mode %x", __func__, mCameraId, (ConfigMode)streamList->operation_mode);

    int ret = analyzeStream(streamList);
    CheckAndLogError(ret != OK, ret, "@%s, analyzeStream failed", __func__);

    // If configured before, destroy current streams first.
    if (mStreamNum > 0 && clean) {
        deleteStreams();
    }
    mProcessorManager->deleteProcessors();

    // Clear all previous added listeners.
    mProducer->removeAllFrameAvailableListener();
    if (clean) {
        ret = createStreams(streamList);
        CheckAndLogError(ret < 0, ret, "@%s create stream failed with %d", __func__, ret);
    }
    mRequestThread->postConfigure(streamList);

    int mcId = -1;
    if (mGCM != nullptr) {
        ret = mGCM->configStreams(streamList);
        CheckAndLogError(ret != OK, INVALID_OPERATION, "No matching graph config found");

        mcId = mGCM->getSelectedMcId();
    }

    std::map<Port, stream_t> producerConfigs = selectProducerConfig(streamList, mcId);
    CheckAndLogError(producerConfigs.empty(), BAD_VALUE, "The config for producer is invalid.");

    bool needProcessor = isProcessorNeeded(streamList, producerConfigs[MAIN_PORT]);
    for (auto& item : producerConfigs) {
        LOG1("Producer config for port:%d, fmt:%s (%dx%d), needProcessor=%d", item.first,
             CameraUtils::format2string(item.second.format).c_str(),
             item.second.width, item.second.height, needProcessor);
        // Only V4L2_MEMORY_MMAP is supported when using post processor
        if (needProcessor) {
            item.second.memType = V4L2_MEMORY_MMAP;
        }
    }

    vector<ConfigMode> configModes;
    PlatformData::getConfigModesByOperationMode(mCameraId, streamList->operation_mode, configModes);

    ret = mProducer->configure(producerConfigs, configModes);
    CheckAndLogError(ret < 0, BAD_VALUE, "@%s Device Configure failed", __func__);

    ret = mSofSource->configure();
    CheckAndLogError(ret != OK, ret, "@%s failed to configure SOF source device", __func__);

    m3AControl->configure(streamList);

    if (needProcessor) {
        mProcessors = mProcessorManager->createProcessors(mInputConfig.format, producerConfigs,
                                                          mStreamIdToPortMap,
                                                          streamList, mParameter, mParamGenerator);
        ret = mProcessorManager->configureProcessors(configModes,
                                                     mProducer, mParameter);
        CheckAndLogError(ret != OK, ret, "@%s configure post processor failed with:%d", __func__,
                         ret);
    }

    ret = bindStreams(streamList);
    CheckAndLogError(ret < 0, ret, "@%s bind stream failed with %d", __func__, ret);

    mState = DEVICE_CONFIGURE;
    return OK;
}

/**
 * Select the producer's config from the supported list.
 *
 * How to decide the producer's config?
 * 1. Select the input stream if it's provided
 * 2. Use user's cropRegion or CSI output in graph to select the MC and producerConfigs
 * 3. Try to use the same config as user's required.
 * 4. Select the producerConfigs of SECOND_PORT if DOL enabled
 */
std::map<Port, stream_t> CameraDevice::selectProducerConfig(const stream_config_t *streamList, int mcId)
{
    std::map<Port, stream_t> producerConfigs;
    if (!PlatformData::isIsysEnabled(mCameraId)) {
        // Input stream id is the last one of mSortedStreamIds
        const stream_t& tmp = streamList->streams[mSortedStreamIds.back()];
        if (tmp.streamType == CAMERA_STREAM_INPUT) {
            producerConfigs[MAIN_PORT] = tmp;
            return producerConfigs;
        }
    }

    stream_t biggestStream = streamList->streams[mSortedStreamIds[0]];
    camera_crop_region_t cropRegion;
    int ret = mParameter.getCropRegion(cropRegion);
    if ((ret == OK) && (cropRegion.flag == 1)) {
        // Use crop region to select MC config
        PlatformData::selectMcConf(mCameraId, mInputConfig,
                                  (ConfigMode)streamList->operation_mode, mcId);
    } else {
        // Use CSI output to select MC config
        vector <ConfigMode> configModes;
        PlatformData::getConfigModesByOperationMode(mCameraId, streamList->operation_mode,
                                                    configModes);
        stream_t matchedStream = biggestStream;
        if (!configModes.empty() && mGCM != nullptr) {
            std::shared_ptr<IGraphConfig> gc = mGCM->getGraphConfig(configModes[0]);
            if (gc) {
                camera_resolution_t csiOutput = {0, 0};
                gc->getCSIOutputResolution(csiOutput);
                if (csiOutput.width > 0 && csiOutput.height > 0) {
                    matchedStream.width = csiOutput.width;
                    matchedStream.height = csiOutput.height;
                }
            }
        }
        PlatformData::selectMcConf(mCameraId, matchedStream,
                                   (ConfigMode)streamList->operation_mode, mcId);
    }

    // Select the output format.
    int iSysFmt = biggestStream.format;
    if (mInputConfig.format != -1) {
        if (!PlatformData::isISysSupportedFormat(mCameraId, mInputConfig.format)) {
            LOGE("The given ISYS format %s is unsupported.",
                 CameraUtils::pixelCode2String(mInputConfig.format));
            return producerConfigs;
        }
        iSysFmt = mInputConfig.format;
    }
    PlatformData::selectISysFormat(mCameraId, iSysFmt);

    // Use the ISYS output if it's provided in media config section of config file.
    stream_t mainConfig = PlatformData::getISysOutputByPort(mCameraId, MAIN_PORT);
    mainConfig.memType = biggestStream.memType;
    mainConfig.field = biggestStream.field;

    if (mainConfig.width != 0 && mainConfig.height != 0) {
        producerConfigs[MAIN_PORT] = mainConfig;
        return producerConfigs;
    }

    // Filter the ISYS best resolution with input stream
    int inputWidth = mInputConfig.width;
    int inputHeight = mInputConfig.height;
    camera_resolution_t producerRes = {inputWidth, inputHeight};
    if (inputWidth == 0 && inputHeight == 0) {
        // Only get the ISYS resolution when input config is not specified.
        producerRes = PlatformData::getISysBestResolution(mCameraId, biggestStream.width,
                                                          biggestStream.height, biggestStream.field);
    } else if (!PlatformData::isISysSupportedResolution(mCameraId, producerRes)) {
        LOGE("The stream config: (%dx%d) is not supported.", inputWidth, inputHeight);
        return producerConfigs;
    }

    // Update the height according to the field(interlaced).
    mainConfig.format = PlatformData::getISysFormat(mCameraId);
    mainConfig.width = producerRes.width;
    mainConfig.height = CameraUtils::getInterlaceHeight(mainConfig.field, producerRes.height);

    // configuration with main port
    producerConfigs[MAIN_PORT] = mainConfig;

    return producerConfigs;
}

/**
 * Check if post processor is needed.
 * The processor is needed when:
 * 1. At least one of the given streams does not match with the producer's output.
 * 2. To support specific features such as HW weaving or dewarping.
 */
bool CameraDevice::isProcessorNeeded(const stream_config_t *streamList,
                                     const stream_t &producerConfig)
{
    camera_crop_region_t cropRegion;
    int ret = mParameter.getCropRegion(cropRegion);
    if ((ret == OK) && (cropRegion.flag == 1)) return true;

    if (producerConfig.field != V4L2_FIELD_ANY) {
        camera_deinterlace_mode_t mode = DEINTERLACE_OFF;
        mParameter.getDeinterlaceMode(mode);
        if (mode == DEINTERLACE_WEAVING) {
            return true;
        }
    }

    if (producerConfig.field != V4L2_FIELD_ALTERNATE) {
        int streamCounts = streamList->num_streams;
        for (int streamId = 0; streamId < streamCounts; streamId++) {
            if (producerConfig.width != streamList->streams[streamId].width ||
                producerConfig.height != streamList->streams[streamId].height ||
                producerConfig.format != streamList->streams[streamId].format) {
                return true;
            }
        }
    }

    camera_mono_downscale_mode_t monoDsMode = MONO_DS_MODE_OFF;
    mParameter.getMonoDsMode(monoDsMode);
    if (monoDsMode != MONO_DS_MODE_OFF) {
        return true;
    }

    return false;
}

int CameraDevice::createStreams(stream_config_t *streamList)
{
    LOG1("@%s, mCameraId:%d", __func__, mCameraId);

    int streamCounts = streamList->num_streams;
    for (int streamId = 0; streamId < streamCounts; streamId++) {
        stream_t& streamConf = streamList->streams[streamId];
        LOG1("@%s, stream_number:%d, stream configure: format:%s (%dx%d)", __func__, streamCounts,
             CameraUtils::pixelCode2String(streamConf.format), streamConf.width, streamConf.height);

        streamConf.id = streamId;
        streamConf.max_buffers = PlatformData::getMaxRequestsInflight(mCameraId);
        CameraStream *stream = new CameraStream(mCameraId, streamId, streamConf);
        stream->registerListener(EVENT_FRAME_AVAILABLE, mRequestThread);
        mStreams[streamId] = stream;
        mStreamNum++;

        LOG2("@%s: automation checkpoint: interlaced: %d", __func__, streamConf.field);
    }

    return OK;
}

/**
 * According resolution to store the streamId in descending order.
 * Use this order to bind stream to port, and set output Port mapping
 */
int CameraDevice::analyzeStream(stream_config_t *streamList)
{
    LOG1("@%s, mCameraId:%d", __func__, mCameraId);

    mSortedStreamIds.clear();
    mStreamIdToPortMap.clear();

    int inputStreamId = -1;
    int opaqueRawStreamId = -1;
    for (int i = 0; i < streamList->num_streams; i++) {
        const stream_t& stream = streamList->streams[i];

        if (stream.streamType == CAMERA_STREAM_INPUT) {
            CheckAndLogError(inputStreamId >= 0, BAD_VALUE, "Don't support two input streams!");
            inputStreamId = i;
            continue;
        }

        if (stream.usage == CAMERA_STREAM_OPAQUE_RAW) {
            CheckAndLogError(opaqueRawStreamId >= 0, BAD_VALUE, "Don't support two RAW streams!");
            opaqueRawStreamId = i;
            continue;
        }

        camera_crop_region_t cropRegion;
        int ret = mParameter.getCropRegion(cropRegion);
        if (ret != OK || cropRegion.flag == 0) {
            bool valid = PlatformData::isSupportedStream(mCameraId, stream);
            CheckAndLogError(!valid, BAD_VALUE, "Stream config is not supported. format:%s (%dx%d)",
                             CameraUtils::pixelCode2String(stream.format), stream.width,
                             stream.height);
        }

        bool saved = false;
        // Store the streamId in descending order.
        for (size_t j = 0; j < mSortedStreamIds.size(); j++) {
            const stream_t& tmp = streamList->streams[mSortedStreamIds[j]];
            if (stream.width * stream.height > tmp.width * tmp.height) {
                mSortedStreamIds.insert((mSortedStreamIds.begin() + j), i);
                saved = true;
                break;
            }
        }
        if (!saved)
            mSortedStreamIds.push_back(i);
    }

    // Set opaque RAW stream as last one
    if (opaqueRawStreamId >= 0) {
        mSortedStreamIds.push_back(opaqueRawStreamId);
        // Ignore input raw stream for ZSL case
        inputStreamId = -1;
    }

    const Port kPorts[] = {MAIN_PORT, SECOND_PORT, THIRD_PORT, FORTH_PORT};
    for (size_t i = 0; i < mSortedStreamIds.size(); i++) {
        mStreamIdToPortMap[mSortedStreamIds[i]] = kPorts[i];

        // Dump the stream info by descending order.
        const stream_t& stream = streamList->streams[mSortedStreamIds[i]];
        LOG1("%s  streamId: %d, %dx%d(%s)", __func__, mSortedStreamIds[i],
                stream.width, stream.height, CameraUtils::format2string(stream.format).c_str());
    }

    bool checkInput = !PlatformData::isIsysEnabled(mCameraId);
    if (checkInput) {
        CheckAndLogError(inputStreamId < 0, BAD_VALUE, "Input stream was missing");
    }
    // Handle input stream
    if (inputStreamId >= 0) {
        CheckAndLogError(mSortedStreamIds.empty(), BAD_VALUE, "There is no output stream!");
        // Check if input stream is supported or not
        const stream_t& stream = streamList->streams[inputStreamId];
        camera_resolution_t inputResolution = {stream.width, stream.height};
        bool valid = PlatformData::isISysSupportedResolution(mCameraId, inputResolution);
        CheckAndLogError(!valid, BAD_VALUE, "Stream config is not supported. format:%s (%dx%d)",
                         CameraUtils::pixelCode2String(stream.format), stream.width, stream.height);
        // Push input stream index to the end of vector mSortedStreamIds
        mSortedStreamIds.push_back(inputStreamId);
        // Use MAIN PORT for input stream
        mStreamIdToPortMap[inputStreamId] = MAIN_PORT;
    }

    return OK;
}

/**
 * Bind all streams to their producers and to the correct port.
 *
 * Bind the streams to Port in resolution descending order:
 * Stream with max resolution            --> MAIN_PORT
 * Stream with intermediate resolution   --> SECOND_PORT
 * Stream with min resolution            --> THIRD_PORT
 */
int CameraDevice::bindStreams(stream_config_t *streamList)
{
    for (auto& iter : mStreamIdToPortMap) {
        mStreams[iter.first]->setPort(iter.second);

        // If no post processors, bind the stream to the producer.
        if (mProcessors.empty()) {
            mStreams[iter.first]->setBufferProducer(mProducer);
        } else {
            mStreams[iter.first]->setBufferProducer(mProcessors.back());
        }
    }

    return OK;
}

int CameraDevice::start()
{
    PERF_CAMERA_ATRACE();
    LOG1("@%s, mCameraId:%d, mState:%d", __func__, mCameraId, mState);

    // Not protected by mDeviceLock because it is required in qbufL()
    mRequestThread->wait1stRequestDone();

    AutoMutex   m(mDeviceLock);
    CheckAndLogError(mState != DEVICE_BUFFER_READY, BAD_VALUE, "start camera in wrong status %d",
                     mState);
    CheckAndLogError(mStreamNum == 0, BAD_VALUE, "@%s: device doesn't add any stream yet.",
                     __func__);

    int ret = startLocked();
    if (ret != OK) {
        LOGE("Camera device starts failed.");
        stopLocked();  // There is error happened, stop all related units.
        return INVALID_OPERATION;
    }

    mState = DEVICE_START;
    return OK;
}

int CameraDevice::stop()
{
    PERF_CAMERA_ATRACE();
    LOG1("@%s, mCameraId:%d, mState:%d", __func__, mCameraId, mState);
    AutoMutex   m(mDeviceLock);

    mRequestThread->clearRequests();

    m3AControl->stop();

    if (mState == DEVICE_START)
        stopLocked();

    mState = DEVICE_STOP;

    return OK;
}

//No Lock for this fuction as it doesn't update any class member
int CameraDevice::allocateMemory(camera_buffer_t *ubuffer)
{
    PERF_CAMERA_ATRACE();
    LOG1("@%s, mCameraId:%d", __func__, mCameraId);
    CheckAndLogError(mState < DEVICE_CONFIGURE, BAD_VALUE, "@%s: Wrong state id %d", __func__,
                     mState);
    CheckAndLogError(ubuffer->s.id < 0 || ubuffer->s.id >= mStreamNum, BAD_VALUE,
                     "@%s: Wrong stream id %d", __func__, ubuffer->s.id);

    int ret = mStreams[ubuffer->s.id]->allocateMemory(ubuffer);
    CheckAndLogError(ret < 0, ret, "@%s: failed, index: %d", __func__, ubuffer->index);

    return ret;
}

/**
 * Delegate it to RequestThread, make RequestThread manage all buffer related actions.
 */
int CameraDevice::dqbuf(int streamId, camera_buffer_t **ubuffer, Parameters* settings)
{
    CheckAndLogError(streamId < 0 || streamId > mStreamNum, BAD_VALUE,
                     "@%s: the given stream(%d) is invalid.", __func__, streamId);
    PERF_CAMERA_ATRACE();
    LOG2("@%s, camera id:%d, stream id:%d", __func__, mCameraId, streamId);

    int ret = mRequestThread->waitFrame(streamId, ubuffer);
    while (ret == TIMED_OUT)
        ret = mRequestThread->waitFrame(streamId, ubuffer);

    if (ret == NO_INIT) return ret;

    CheckAndLogError(!*ubuffer || ret != OK, ret, "failed to get ubuffer from stream %d", streamId);

    const AiqResult *aiqResult
        = AiqResultStorage::getInstance(mCameraId)->getAiqResult((*ubuffer)->sequence);
    if (aiqResult != nullptr) {
        mLastSceneMode = aiqResult->mSceneMode;
    }

    if (settings) {
        ret = mParamGenerator->getParameters((*ubuffer)->sequence, settings);
    }

    return ret;
}

int CameraDevice::handleQueueBuffer(int bufferNum, camera_buffer_t **ubuffer, long sequence)
{
    LOG2("@%s, mCameraId:%d, sequence = %ld", __func__, mCameraId, sequence);
    CheckAndLogError(mState < DEVICE_CONFIGURE, BAD_VALUE,"@%s: Wrong state id %d", __func__,
                     mState);

    // All streams need to be queued with either a real buffer from user or an empty buffer.
    for (int streamId = 0; streamId < mStreamNum; streamId++) {
        bool isBufferQueued = false;
        CheckAndLogError(mStreams[streamId] == nullptr, BAD_VALUE,
                         "@%s: stream %d is nullptr", __func__, streamId);

        // Find if user has queued a buffer for mStreams[streamId].
        for (int bufferId = 0; bufferId < bufferNum; bufferId++) {
            camera_buffer_t* buffer = ubuffer[bufferId];
            int streamIdInBuf = buffer->s.id;
            CheckAndLogError(streamIdInBuf < 0 || streamIdInBuf > mStreamNum, BAD_VALUE,
                             "@%s: Wrong stream id %d", __func__, streamIdInBuf);

            if (streamIdInBuf == streamId) {
                int ret = mStreams[streamId]->qbuf(buffer, sequence);
                CheckAndLogError(ret < 0, ret, "@%s: queue buffer:%p failed:%d", __func__, buffer,
                                 ret);
                isBufferQueued = true;
                break;
            }
        }

        // If streamId is not found in buffers queued by user, then we need to queue
        // an empty buffer to keep the BufferQueue run.
        if (!isBufferQueued) {
            int ret = mStreams[streamId]->qbuf(nullptr, sequence);
            CheckAndLogError(ret < 0, ret, "@%s: queue empty buffer failed:%d", __func__, ret);
        }
    }

    return OK;
}

int CameraDevice::registerBuffer(camera_buffer_t **ubuffer, int bufferNum)
{
    LOG1("@%s, mCameraId:%d", __func__, mCameraId);
    CheckAndLogError(mState < DEVICE_CONFIGURE, BAD_VALUE,"@%s: Wrong state id %d", __func__,
                     mState);
    if (mProcessors.empty()) return OK;

    for (int bufferId = 0; bufferId < bufferNum; bufferId++) {
        camera_buffer_t *buffer = ubuffer[bufferId];
        CheckAndLogError(buffer == nullptr, BAD_VALUE, "@%s, the queue ubuffer %d is NULL",
                         __func__, bufferId);
        int streamIdInBuf = buffer->s.id;
        CheckAndLogError(streamIdInBuf < 0 || streamIdInBuf > mStreamNum, BAD_VALUE,
                         "@%s: Wrong stream id %d", __func__, streamIdInBuf);
        std::shared_ptr<CameraBuffer> camBuffer =
            mStreams[streamIdInBuf]->userBufferToCameraBuffer(buffer);
        for (auto& iter : mStreamIdToPortMap) {
            // Register buffers to the last processor
            if (iter.first == streamIdInBuf) {
                BufferQueue *processor = mProcessors.back();
                processor->registerUserOutputBufs(iter.second, camBuffer);
                break;
            }
        }
    }

    return OK;
}

int CameraDevice::qbuf(camera_buffer_t **ubuffer,
                       int bufferNum, const Parameters *settings)
{
    PERF_CAMERA_ATRACE();
    LOG2("@%s, mCameraId:%d", __func__, mCameraId);

    {
        AutoMutex   m(mDeviceLock);
        if (mState == DEVICE_CONFIGURE || mState == DEVICE_STOP) {
            // Start 3A here then the HAL can run 3A for request
            int ret = m3AControl->start();
            CheckAndLogError((ret != OK), BAD_VALUE, "Start 3a unit failed with ret:%d.", ret);

            mState = DEVICE_BUFFER_READY;
        }
    }

    if (mState != DEVICE_START && PlatformData::isNeedToPreRegisterBuffer(mCameraId)) {
        registerBuffer(ubuffer, bufferNum);
    }

    // Make sure request's configure mode is updated by latest result param if no settings
    if (!settings) {
        mRequestThread->setConfigureModeByParam(mLastSceneMode);
    }

    return mRequestThread->processRequest(bufferNum, ubuffer, settings);
}

int CameraDevice::getParameters(Parameters& param, long sequence)
{
    PERF_CAMERA_ATRACE();
    LOG2("<id%d:seq%ld>@%s", mCameraId, sequence, __func__);
    AutoMutex   m(mDeviceLock);

    if (sequence >= 0 && mState != DEVICE_STOP) {
        // fetch target parameter and results
        return mParamGenerator->getParameters(sequence, &param);
    }

    param = mParameter;

    for (auto& item : mProcessors) {
        item->getParameters(param);
    }

    return OK;
}

int CameraDevice::setParameters(const Parameters& param)
{
    PERF_CAMERA_ATRACE();
    LOG1("@%s mCameraId:%d", __func__, mCameraId);
    AutoMutex   m(mDeviceLock);
    return setParametersL(param);
}

int CameraDevice::setParametersL(const Parameters& param)
{
    // Merge given param into internal unique mParameter
    mParameter.merge(param);

    int ret = m3AControl->setParameters(param);
    for (auto& item : mProcessors) {
        item->setParameters(mParameter);
    }

    // Set test pattern mode
    camera_test_pattern_mode_t testPatternMode = TEST_PATTERN_OFF;
    if (PlatformData::isTestPatternSupported(mCameraId)
            && param.getTestPatternMode(testPatternMode) == OK) {
        int32_t sensorTestPattern = PlatformData::getSensorTestPattern(mCameraId, testPatternMode);
        if (sensorTestPattern >= 0) {
            ret |= mSensorCtrl->setTestPatternMode(sensorTestPattern);
        }
    }

    return ret;
}

//Private Functions, these functions are called with device lock hold

//Destroy all the streams
void CameraDevice::deleteStreams()
{
    PERF_CAMERA_ATRACE();
    LOG1("@%s mCameraId:%d, streams:%d", __func__, mCameraId, mStreamNum);

    for (int streamId = 0; streamId < mStreamNum; streamId++) {
        mStreams[streamId]->stop();
        delete mStreams[streamId];
        mStreams[streamId] = nullptr;
    }
    mStreamNum = 0;
}

// Internal start without lock hold
int CameraDevice::startLocked()
{
    int ret = OK;

    bindListeners();

    //Start all the streams
    for(int i = 0; i < mStreamNum; i++) {
        ret = mStreams[i]->start();
        CheckAndLogError(ret < 0, BAD_VALUE, "Start stream %d failed with ret:%d.", i, ret);
    }

    for (auto& item : mProcessors) {
        ret = item->start();
        CheckAndLogError((ret < 0), BAD_VALUE, "Start image processor failed with ret:%d.", ret);
    }

    //Start the CaptureUnit for streamon
    ret = mProducer->start();
    CheckAndLogError((ret < 0), BAD_VALUE, "Start capture unit failed with ret:%d.", ret);

    ret = mSofSource->start();
    CheckAndLogError((ret != OK), BAD_VALUE, "Start SOF event source failed with ret:%d.", ret);

    return OK;
}

// Internal stop without lock hold
int CameraDevice::stopLocked()
{
    PERF_CAMERA_ATRACE();
    LOG1("@%s, mCameraId:%d", __func__, mCameraId);

    mSofSource->stop();

    //Stop the CaptureUnit for streamon
    mProducer->stop();

    for (auto& item : mProcessors) {
        item->stop();
    }

    unbindListeners();
    mParamGenerator->reset();

    return OK;
}

int CameraDevice::reconfigure(stream_config_t *streamList)
{
    AutoMutex   m(mDeviceLock);

    int ret = OK;

    LOG2("%s: switch type: %d, new mode:%d", __func__,
        PlatformData::getAutoSwitchType(mCameraId), streamList->operation_mode);

    if (PlatformData::getAutoSwitchType(mCameraId) == AUTO_SWITCH_FULL) {
        // Wait and return all user buffers in all streams firstly
        for (int streamId = 0; streamId < mStreamNum; streamId++) {
            mStreams[streamId]->waitToReturnAllUserBufffers();
        }

        LOG2("%s: all streams stopped", __func__);

        // Stop and clean what needed.
        m3AControl->stop();

        if (mState == DEVICE_START)
            stopLocked();

        mState = DEVICE_STOP;

        for (int streamId = 0; streamId < mStreamNum; streamId++) {
            mStreams[streamId]->stop();
        }

        mProcessorManager->deleteProcessors();

        m3AControl->deinit();

        mSofSource->deinit();

        mProducer->deinit();

        /* Currently kernel have issue and need reopen subdevices
         * when stream off and on. Remove below delete and recreate code
         * when all kernel issues got fixed.
         */
        // Delete related components and v4l2 devices
        delete mLensCtrl;
        delete m3AControl;
        delete mSensorCtrl;
        delete mSofSource;
        delete mProducer;

        V4l2DeviceFactory::releaseDeviceFactory(mCameraId);

        // Re-create related components and v4l2 devices
        mProducer = createBufferProducer();
        mSofSource = new SofSource(mCameraId);
        mLensCtrl = new LensHw(mCameraId);
        mSensorCtrl = SensorHwCtrl::createSensorCtrl(mCameraId);
        m3AControl = I3AControlFactory::createI3AControl(mCameraId, mSensorCtrl, mLensCtrl,
                                                         mParamGenerator);

        // Init and config with new mode
        int ret = mProducer->init();
        CheckAndLogError(ret < 0, ret, "%s: Init capture unit failed", __func__);

        ret = mSofSource->init();
        CheckAndLogError(ret != OK, ret, "@%s: init sync manager failed", __func__);

        initDefaultParameters();

        ret = m3AControl->init();
        CheckAndLogError((ret != OK), ret, "%s: Init 3A Unit falied", __func__);

        ret = mLensCtrl->init();
        CheckAndLogError((ret != OK), ret, "%s: Init Lens falied", __func__);

        mState = DEVICE_INIT;

        // Auto switch do not recreate streams.
        configureL(streamList, false);

        ret = m3AControl->setParameters(mParameter);
        for (auto& item : mProcessors) {
            item->setParameters(mParameter);
        }
        CheckAndLogError((ret != OK), ret, "%s: set parameters falied", __func__);

        ret = m3AControl->start();
        CheckAndLogError((ret != OK), BAD_VALUE, "Start 3a unit failed with ret:%d.", ret);

        mState = DEVICE_BUFFER_READY;

        ret = startLocked();
        if (ret != OK) {
            LOGE("Camera device starts failed.");
            stopLocked();  // There is error happened, stop all related units.
            return INVALID_OPERATION;
        }

        mState = DEVICE_START;

        LOG2("%s: reconfigure CameraDevice done", __func__);
    } else {

        /* Scene mode based psys-only auto switch here will be used to
         * replace current auto-switch mechanism in AiqSetting:updateTuningMode,
         * which is for non-DOL sensor auto-switch. The switch stabilization
         * counting in AiqSetting:updateTuningMode will also be replaced by the
         * same mechanism in RequestThread.
         */
        LOG2("%s: reconfigure CameraDevice to new mode %d with psys-only switch",
             __func__, streamList->operation_mode);
    }

    return ret;
}

void CameraDevice::handleEvent(EventData eventData)
{
    LOG2("%s, event type:%d", __func__, eventData.type);

    switch (eventData.type) {
    case EVENT_PROCESS_REQUEST: {
        const EventRequestData& request = eventData.data.request;
        if (request.param) {
            for (auto& item : mProcessors) {
                 item->setParameters(*request.param);
            }

            // Set test pattern mode
            camera_test_pattern_mode_t testPatternMode = TEST_PATTERN_OFF;
            if (PlatformData::isTestPatternSupported(mCameraId)
                    && request.param->getTestPatternMode(testPatternMode) == OK) {
                int32_t sensorTestPattern =
                            PlatformData::getSensorTestPattern(mCameraId, testPatternMode);
                if (sensorTestPattern >= 0) {
                    if (mSensorCtrl->setTestPatternMode(sensorTestPattern) < 0) {
                        LOGE("%s, set testPatternMode failed", __func__);
                    }
                }
            }
        }

        handleQueueBuffer(request.bufferNum, request.buffer, request.settingSeq);
        break;
    }

    case EVENT_DEVICE_RECONFIGURE: {
        const EventConfigData& config = eventData.data.config;
        reconfigure(config.streamList);
        break;
    }

    case EVENT_PSYS_REQUEST_BUF_READY: {
        if (mCallback) {
            camera_msg_data_t data = {CAMERA_ISP_BUF_READY, {}};
            int32_t userRequestId = 0;
            int ret = mParamGenerator->getUserRequestId(eventData.data.requestReady.sequence,
                                                        userRequestId);
            CheckAndLogError(ret != OK, VOID_VALUE, "failed to find request id,  seq %ld",
                             eventData.data.requestReady.sequence);

            data.data.buffer_ready.timestamp = eventData.data.requestReady.timestamp;
            data.data.buffer_ready.frameNumber = static_cast<uint32_t>(userRequestId);
            mCallback->notify(mCallback, data);
            PlatformData::updateMakernoteTimeStamp(mCameraId, eventData.data.requestReady.sequence,
                                                   data.data.buffer_ready.timestamp);
        }
        break;
    }

    case EVENT_REQUEST_METADATA_READY: {
        if (mCallback) {
            camera_msg_data_t data = {CAMERA_METADATA_READY, {}};
            int32_t userRequestId = 0;
            int ret = mParamGenerator->getUserRequestId(eventData.data.requestReady.sequence,
                                                        userRequestId);
            CheckAndLogError(ret != OK, VOID_VALUE, "failed to find request id,  seq %ld",
                             eventData.data.requestReady.sequence);

            data.data.metadata_ready.sequence = eventData.data.requestReady.sequence;
            data.data.metadata_ready.frameNumber = static_cast<uint32_t>(userRequestId);
            mCallback->notify(mCallback, data);
        }
        break;
    }

    default:
        LOGE("Not supported event type:%d", eventData.type);
        break;
    }
}

int CameraDevice::initDefaultParameters()
{
    LOG1("@%s mCameraId:%d", __func__, mCameraId);

    camera_info_t info;
    CLEAR(info);
    PlatformData::getCameraInfo(mCameraId, info);

    // Init mParameter to camera's capability first and then add some others default settings
    mParameter = *info.capability;

    camera_range_t fps = {10, 60};
    camera_range_array_t fpsRanges;
    if (mParameter.getSupportedFpsRange(fpsRanges) == OK) {
        fps = fpsRanges.back();
    }
    mParameter.setFpsRange(fps);
    mParameter.setFrameRate(30);

    camera_image_enhancement_t enhancement;
    CLEAR(enhancement); // All use 0 as default
    mParameter.setImageEnhancement(enhancement);

    mParameter.setWeightGridMode(WEIGHT_GRID_AUTO);

    mParameter.setWdrLevel(100);

    mParameter.setFlipMode(FLIP_MODE_NONE);

    mParameter.setRun3ACadence(1);

    mParameter.setYuvColorRangeMode(PlatformData::getYuvColorRangeMode(mCameraId));

    mParameter.setFocusDistance(0.0f);
    mParameter.setTonemapMode(TONEMAP_MODE_FAST);

    return OK;
}

} //namespace icamera

