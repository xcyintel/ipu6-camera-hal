/*
 * Copyright (C) 2021 Intel Corporation.
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

// !!! DO NOT EDIT THIS FILE !!!

#ifndef __MODULE_TAGS_HPP__
#define __MODULE_TAGS_HPP__

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Werror"
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#pragma clang diagnostic ignored "-Wformat-security"
#endif

enum ModuleTags {
      ST_FDFPS = 0,
      GENERATED_TAGS_3atest = 1,
      GENERATED_TAGS_ATEUnit = 2,
      GENERATED_TAGS_AiqCore = 3,
      GENERATED_TAGS_AiqEngine = 4,
      GENERATED_TAGS_AiqInitData = 5,
      GENERATED_TAGS_AiqResult = 6,
      GENERATED_TAGS_AiqResultStorage = 7,
      GENERATED_TAGS_AiqSetting = 8,
      GENERATED_TAGS_AiqUnit = 9,
      GENERATED_TAGS_AiqUtils = 10,
      GENERATED_TAGS_BufferQueue = 11,
      GENERATED_TAGS_CASE_3A_CONTROL = 12,
      GENERATED_TAGS_CASE_AIQ = 13,
      GENERATED_TAGS_CASE_API_MULTI_THREAD = 14,
      GENERATED_TAGS_CASE_BUFFER = 15,
      GENERATED_TAGS_CASE_CIPR = 16,
      GENERATED_TAGS_CASE_COMMON = 17,
      GENERATED_TAGS_CASE_CPF = 18,
      GENERATED_TAGS_CASE_DEVICE_OPS = 19,
      GENERATED_TAGS_CASE_DUAL = 20,
      GENERATED_TAGS_CASE_GRAPH = 21,
      GENERATED_TAGS_CASE_IQ_EFFECT = 22,
      GENERATED_TAGS_CASE_PAL_P2P = 23,
      GENERATED_TAGS_CASE_PARAMETER = 24,
      GENERATED_TAGS_CASE_PER_FRAME = 25,
      GENERATED_TAGS_CASE_STATIC_INFO = 26,
      GENERATED_TAGS_CASE_STREAM_OPS = 27,
      GENERATED_TAGS_CASE_THREAD = 28,
      GENERATED_TAGS_CASE_VIRTUAL_CHANNEL = 29,
      GENERATED_TAGS_CIPR_BUFFER = 30,
      GENERATED_TAGS_CIPR_COMMAND = 31,
      GENERATED_TAGS_CIPR_CONTEXT = 32,
      GENERATED_TAGS_CIPR_EVENT = 33,
      GENERATED_TAGS_Camera2Module = 34,
      GENERATED_TAGS_Camera3AMetadata = 35,
      GENERATED_TAGS_Camera3Buffer = 36,
      GENERATED_TAGS_Camera3BufferPool = 37,
      GENERATED_TAGS_Camera3Channel = 38,
      GENERATED_TAGS_Camera3Format = 39,
      GENERATED_TAGS_Camera3HAL = 40,
      GENERATED_TAGS_Camera3HALModule = 41,
      GENERATED_TAGS_Camera3HWI = 42,
      GENERATED_TAGS_Camera3Stream = 43,
      GENERATED_TAGS_CameraBuffer = 44,
      GENERATED_TAGS_CameraDevice = 45,
      GENERATED_TAGS_CameraDump = 46,
      GENERATED_TAGS_CameraEvent = 47,
      GENERATED_TAGS_CameraHal = 48,
      GENERATED_TAGS_CameraHardwareSoc = 49,
      GENERATED_TAGS_CameraLog = 50,
      GENERATED_TAGS_CameraMetadata = 51,
      GENERATED_TAGS_CameraParser = 52,
      GENERATED_TAGS_CameraShm = 53,
      GENERATED_TAGS_CameraStream = 54,
      GENERATED_TAGS_Camera_PolicyManager = 55,
      GENERATED_TAGS_CaptureUnit = 56,
      GENERATED_TAGS_ColorConverter = 57,
      GENERATED_TAGS_CscPipeline = 58,
      GENERATED_TAGS_CscProcessor = 59,
      GENERATED_TAGS_CsiMetaDevice = 60,
      GENERATED_TAGS_Customized3A = 61,
      GENERATED_TAGS_CustomizedAic = 62,
      GENERATED_TAGS_DeviceBase = 63,
      GENERATED_TAGS_Dvs = 64,
      GENERATED_TAGS_EXIFMaker = 65,
      GENERATED_TAGS_EXIFMetaData = 66,
      GENERATED_TAGS_ExifCreater = 67,
      GENERATED_TAGS_FaceDetection = 68,
      GENERATED_TAGS_FileSource = 69,
      GENERATED_TAGS_GPUExecutor = 70,
      GENERATED_TAGS_GenGfx = 71,
      GENERATED_TAGS_GfxGen = 72,
      GENERATED_TAGS_GraphConfig = 73,
      GENERATED_TAGS_GraphConfigImpl = 74,
      GENERATED_TAGS_GraphConfigImplClient = 75,
      GENERATED_TAGS_GraphConfigManager = 76,
      GENERATED_TAGS_GraphConfigPipe = 77,
      GENERATED_TAGS_GraphConfigServer = 78,
      GENERATED_TAGS_GraphUtils = 79,
      GENERATED_TAGS_HAL_FACE_DETECTION_TEST = 80,
      GENERATED_TAGS_HAL_basic = 81,
      GENERATED_TAGS_HAL_jpeg = 82,
      GENERATED_TAGS_HAL_multi_streams_test = 83,
      GENERATED_TAGS_HAL_rotation_test = 84,
      GENERATED_TAGS_HAL_supported_streams_test = 85,
      GENERATED_TAGS_HAL_yuv = 86,
      GENERATED_TAGS_HalV3Utils = 87,
      GENERATED_TAGS_I3AControlFactory = 88,
      GENERATED_TAGS_IA_CIPR_UTILS = 89,
      GENERATED_TAGS_ICamera = 90,
      GENERATED_TAGS_IPCIntelPGParam = 91,
      GENERATED_TAGS_IPC_FACE_DETECTION = 92,
      GENERATED_TAGS_IPC_GRAPH_CONFIG = 93,
      GENERATED_TAGS_ImageProcessorCore = 94,
      GENERATED_TAGS_ImageScalerCore = 95,
      GENERATED_TAGS_Intel3AParameter = 96,
      GENERATED_TAGS_IntelAEStateMachine = 97,
      GENERATED_TAGS_IntelAFStateMachine = 98,
      GENERATED_TAGS_IntelAWBStateMachine = 99,
      GENERATED_TAGS_IntelAlgoClient = 100,
      GENERATED_TAGS_IntelAlgoCommonClient = 101,
      GENERATED_TAGS_IntelAlgoServer = 102,
      GENERATED_TAGS_IntelCPUAlgoServer = 103,
      GENERATED_TAGS_IntelCca = 104,
      GENERATED_TAGS_IntelCcaClient = 105,
      GENERATED_TAGS_IntelCcaServer = 106,
      GENERATED_TAGS_IntelFDServer = 107,
      GENERATED_TAGS_IntelFaceDetection = 108,
      GENERATED_TAGS_IntelFaceDetectionClient = 109,
      GENERATED_TAGS_IntelGPUAlgoServer = 110,
      GENERATED_TAGS_IntelPGParam = 111,
      GENERATED_TAGS_IntelPGParamClient = 112,
      GENERATED_TAGS_IntelPGParamS = 113,
      GENERATED_TAGS_IntelTNR7US = 114,
      GENERATED_TAGS_IntelTNR7USClient = 115,
      GENERATED_TAGS_IntelTNRServer = 116,
      GENERATED_TAGS_IspControlUtils = 117,
      GENERATED_TAGS_IspParamAdaptor = 118,
      GENERATED_TAGS_JpegEncoderCore = 119,
      GENERATED_TAGS_JpegMaker = 120,
      GENERATED_TAGS_LensHw = 121,
      GENERATED_TAGS_LensManager = 122,
      GENERATED_TAGS_LiveTuning = 123,
      GENERATED_TAGS_Ltm = 124,
      GENERATED_TAGS_MANUAL_POST_PROCESSING = 125,
      GENERATED_TAGS_MakerNote = 126,
      GENERATED_TAGS_MediaControl = 127,
      GENERATED_TAGS_MetadataConvert = 128,
      GENERATED_TAGS_MockCamera3HAL = 129,
      GENERATED_TAGS_MockCameraHal = 130,
      GENERATED_TAGS_MockSysCall = 131,
      GENERATED_TAGS_MonoDsPipeline = 132,
      GENERATED_TAGS_MonoDsProcessor = 133,
      GENERATED_TAGS_MsgHandler = 134,
      GENERATED_TAGS_OpenSourceGFX = 135,
      GENERATED_TAGS_PGCommon = 136,
      GENERATED_TAGS_PGUtils = 137,
      GENERATED_TAGS_PSysDAG = 138,
      GENERATED_TAGS_PSysP2pLite = 139,
      GENERATED_TAGS_PSysPipe = 140,
      GENERATED_TAGS_PSysPipeBase = 141,
      GENERATED_TAGS_PSysProcessor = 142,
      GENERATED_TAGS_ParameterGenerator = 143,
      GENERATED_TAGS_ParameterHelper = 144,
      GENERATED_TAGS_ParameterResult = 145,
      GENERATED_TAGS_Parameters = 146,
      GENERATED_TAGS_ParserBase = 147,
      GENERATED_TAGS_PipeExecutor = 148,
      GENERATED_TAGS_PipeLiteExecutor = 149,
      GENERATED_TAGS_PlatformData = 150,
      GENERATED_TAGS_PnpDebugControl = 151,
      GENERATED_TAGS_PolicyParser = 152,
      GENERATED_TAGS_PostProcessor = 153,
      GENERATED_TAGS_PostProcessorBase = 154,
      GENERATED_TAGS_PostProcessorCore = 155,
      GENERATED_TAGS_PrivateStream = 156,
      GENERATED_TAGS_ProcessorManager = 157,
      GENERATED_TAGS_RequestManager = 158,
      GENERATED_TAGS_RequestThread = 159,
      GENERATED_TAGS_ResultProcessor = 160,
      GENERATED_TAGS_SWJpegEncoder = 161,
      GENERATED_TAGS_SWPostProcessor = 162,
      GENERATED_TAGS_ScalePipeline = 163,
      GENERATED_TAGS_ScaleProcessor = 164,
      GENERATED_TAGS_SensorHwCtrl = 165,
      GENERATED_TAGS_SensorManager = 166,
      GENERATED_TAGS_SensorOB = 167,
      GENERATED_TAGS_ShareRefer = 168,
      GENERATED_TAGS_SofSource = 169,
      GENERATED_TAGS_StreamBuffer = 170,
      GENERATED_TAGS_SwImageConverter = 171,
      GENERATED_TAGS_SwImageProcessor = 172,
      GENERATED_TAGS_SyncManager = 173,
      GENERATED_TAGS_SysCall = 174,
      GENERATED_TAGS_TCPServer = 175,
      GENERATED_TAGS_Thread = 176,
      GENERATED_TAGS_Trace = 177,
      GENERATED_TAGS_TunningParser = 178,
      GENERATED_TAGS_Utils = 179,
      GENERATED_TAGS_V4l2DeviceFactory = 180,
      GENERATED_TAGS_V4l2_device_cc = 181,
      GENERATED_TAGS_V4l2_subdevice_cc = 182,
      GENERATED_TAGS_V4l2_video_node_cc = 183,
      GENERATED_TAGS_VendorTags = 184,
      GENERATED_TAGS_WeavingPipeline = 185,
      GENERATED_TAGS_WeavingProcessor = 186,
      GENERATED_TAGS_camera_metadata_tests = 187,
      GENERATED_TAGS_icamera_metadata_base = 188,
      GENERATED_TAGS_metadata_test = 189,
};

#define TAGS_MAX_NUM 190

#endif
// !!! DO NOT EDIT THIS FILE !!!
