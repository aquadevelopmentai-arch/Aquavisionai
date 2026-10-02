#pragma once
// ============================================================
//  BaslerCamData.h  (DLL-INTERNAL header, include only from BaslerLinescanCam.cpp)
//  Needs pylon / boost / threadsafe_queue include paths (BaslerCamIF project only).
// ============================================================
#include <pylon/PylonIncludes.h>
#include <boost/thread.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "BaslerLinescanCam.h"
#include "ThreadSafeQueue.h"   // include LAST (it has 'using namespace std;')

// ------------------------------------------------------------
// Queue items (a null unique_ptr pushed into a queue = wake-up signal)
// ------------------------------------------------------------
struct ProcItem
{
    Pylon::CGrabResultPtr result;     // zero copy: holds the driver buffer
    std::uint64_t         index = 0;
};

struct SaveItem
{
    Pylon::CGrabResultPtr result;     // zero copy: holds the driver buffer until written
    std::uint64_t         index = 0;
};


// ------------------------------------------------------------
// All camera state. CBaslerLineScan owns one instance on the heap
// and accesses it through the pointer m_p (m_p->camera, m_p->err, ...).
// ------------------------------------------------------------
struct BaslerCamData
{
    // pylon init/term (ref-counted by pylon). MUST be declared first:
    // constructed before the camera, destroyed after it.
    Pylon::PylonAutoInitTerm     autoInitTerm;

    Pylon::CInstantCamera        camera;
    FrameCallback                cb   = nullptr;
    void*                        user = nullptr;

    // threads + run flags
    boost::thread                grabThread;
    boost::thread                procThread;
    boost::thread                saveThread;
    std::atomic<bool>            bGrabRun{ false };
    std::atomic<bool>            bProcRun{ false };
    std::atomic<bool>            bSaveRun{ false };

    // queues
    threadsafe_queue<ProcItem>   procQ{ 1000 };   // bounded by MaxNumBuffer anyway
    threadsafe_queue<SaveItem>   saveQ{ 1000 };   // limit = min(SaveConfig.maxQueue, maxBuffers / 2)

    // save config
    SaveConfig                   saveCfg;
    mutable std::mutex           cfgMtx;
    std::string                  sessionTag;      // yyyymmdd_hhmmss at Start
    Pylon::CImageFormatConverter converter;       // used only in save thread (non Mono8/Mono16/BGR8)
    int                          maxBuffers = 100;

    // statistics
    std::atomic<std::uint64_t>   frames{ 0 };
    std::atomic<std::uint64_t>   lost{ 0 };
    std::atomic<std::uint64_t>   saved{ 0 };
    std::atomic<std::uint64_t>   saveDrop{ 0 };
    std::uint64_t                lastBlock = 0;
    bool                         haveLast  = false;

    // error
    mutable std::mutex           errMtx;
    std::string                  err;
};

