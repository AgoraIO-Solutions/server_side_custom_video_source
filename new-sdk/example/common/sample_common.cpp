
//  Agora RTC/MEDIA SDK
//
//  Created by Jay Zhang in 2020-06.
//  Copyright (c) 2019 Agora.io. All rights reserved.
//

#include "sample_common.h"

#include "log.h"
#include <iostream>
#include <string>
#include <fstream>
#include <cstring>

#include "AgoraBase.h"

#if defined(TARGET_OS_LINUX)
#define DEFAULT_LOG_PATH ("/tmp/agorasdk.log")
#else
#define DEFAULT_LOG_PATH ("/data/local/tmp/agorasdk.log")
#endif

#define DEFAULT_LOG_SIZE (512 * 1024)  // default log size is 512 kb

agora::base::IAgoraService* createAndInitAgoraService(bool enableAudioDevice,
                                                      bool enableAudioProcessor, bool enableVideo,bool enableuseStringUid) {
  auto service = createAgoraService();
  agora::base::AgoraServiceConfiguration scfg;
  scfg.enableAudioProcessor = enableAudioProcessor;
  scfg.enableAudioDevice = enableAudioDevice;
  scfg.enableVideo = enableVideo;
  scfg.useStringUid = enableuseStringUid;
  if (service->initialize(scfg) != agora::ERR_OK) {
    return nullptr;
  }

  AG_LOG(INFO, "Created log file at %s", DEFAULT_LOG_PATH);
  if (service->setLogFile(DEFAULT_LOG_PATH, DEFAULT_LOG_SIZE) != 0) {
    return nullptr;
  }
 if(verifyLicense() != 0) return nullptr;
  return service;
}

class LicenseCallbackImpl : public agora::base::LicenseCallback
{
public:
  LicenseCallbackImpl() {}
  virtual ~LicenseCallbackImpl() {}

  virtual void onCertificateRequired() {
    AG_LOG(INFO, "[%s, line: %d]", __FUNCTION__, __LINE__);
  }

  virtual void onLicenseRequest() {
    AG_LOG(INFO, "[%s, line: %d]", __FUNCTION__, __LINE__);
  }

  virtual void onLicenseValidated() {
    AG_LOG(INFO, "[%s, line: %d]", __FUNCTION__, __LINE__);
  }

  virtual void onLicenseError(int result) {
    AG_LOG(ERROR, "[%s, line: %d] result: %d", __FUNCTION__, __LINE__, result);
  }
};

static const std::string CERTIFICATE_FILE = "certificate.bin";
static const std::string CREDENTIAL_FILE = "deviceID.bin";

int verifyLicense()
{
#ifdef SDK_LICENSE_ENABLED
  // Step1: read the certificate buffer from the certificate.bin file
  char* cert_buffer = NULL;
  int cert_length = 0;
  std::ifstream f_cert(CERTIFICATE_FILE.c_str(), std::ios::binary);
  if (f_cert) {
    f_cert.seekg(0, f_cert.end);
    cert_length = f_cert.tellg();
    f_cert.seekg(0, f_cert.beg);

    cert_buffer = new char[cert_length + 1];
    AG_LOG(INFO, "cert_length: %d", cert_length);
    memset(cert_buffer, 0, cert_length + 1);
    f_cert.read(cert_buffer, cert_length);
    if (!cert_buffer || f_cert.gcount() < cert_length) {
      f_cert.close();
      if (cert_buffer) {
        delete[] cert_buffer;
        cert_buffer = NULL;
      }
      AG_LOG(ERROR, "read %s failed", CERTIFICATE_FILE.c_str());
      return -1;
    }
    else {
      f_cert.close();
    }
  }
  else {
    AG_LOG(ERROR, "%s doesn't exist", CERTIFICATE_FILE.c_str());
    return -1;
  }
  AG_LOG(INFO, "certificate: %s", cert_buffer);

  // Step2: read the credential buffer from the deviceID.bin file
  char* cred_buffer = NULL;
  int cred_length = 0;
  std::ifstream f_cred(CREDENTIAL_FILE.c_str(), std::ios::binary);
  if (f_cred) {
    f_cred.seekg(0, f_cred.end);
    cred_length = f_cred.tellg();
    f_cred.seekg(0, f_cred.beg);

    cred_buffer = new char[cred_length + 1];
    AG_LOG(INFO, "cred_length: %d", cred_length);
    memset(cred_buffer, 0, cred_length + 1);
    f_cred.read(cred_buffer, cred_length);
    if (!cred_buffer || f_cred.gcount() < cred_length) {
      f_cred.close();
      if (cred_buffer) {
        delete[] cred_buffer;
        cred_buffer = NULL;
      }
      if (cert_buffer) {
        delete[] cert_buffer;
        cert_buffer = NULL;
      }
      AG_LOG(ERROR, "read %s failed", CREDENTIAL_FILE.c_str());
      return -1;
    }
    else {
      f_cred.close();
    }
  }
  else {
    if (cert_buffer) {
      delete[] cert_buffer;
      cert_buffer = NULL;
    }
    AG_LOG(ERROR, "%s doesn't exist", CREDENTIAL_FILE.c_str());
    return -1;
  }
  AG_LOG(INFO, "credential: %s", cred_buffer);

  // Step3: register callback of license state
  LicenseCallbackImpl *cb = static_cast<LicenseCallbackImpl *>(getAgoraLicenseCallback());
  if (!cb) {
    cb = new LicenseCallbackImpl();
    setAgoraLicenseCallback(static_cast<agora::base::LicenseCallback *>(cb));
  }

  // Step4: verify the license with credential and certificate
  int result = getAgoraCertificateVerifyResult(cred_buffer, cred_length, cert_buffer, cert_length);
  AG_LOG(INFO, "verify result: %d", result);

  if (cert_buffer) {
    delete[] cert_buffer;
    cert_buffer = NULL;
  }
  if (cred_buffer) {
    delete[] cred_buffer;
    cred_buffer = NULL;
  }

  return result;
#else
  return 0;
#endif
}