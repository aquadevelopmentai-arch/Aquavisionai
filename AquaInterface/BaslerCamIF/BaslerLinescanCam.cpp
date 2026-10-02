#include "pch.h"
#include "BaslerCamData.h"      // includes BaslerLinescanCam.h

#include <cstdio>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

// ============================================================
//  Construction
// ============================================================
CBaslerLineScan::CBaslerLineScan()
    : m_p(new BaslerCamData())   // autoInitTerm inside calls PylonInitialize()
{
}

CBaslerLineScan::~CBaslerLineScan()
{
    Close();
    delete m_p;                  // camera destroyed, then autoInitTerm calls PylonTerminate()
    m_p = nullptr;
}

// ============================================================
//  Open / Close
// ============================================================
bool CBaslerLineScan::Open(const char* serialNumber)
{
    if (IsOpen())
        return true;

    try
    {
        Pylon::CTlFactory&   factory = Pylon::CTlFactory::GetInstance();
        Pylon::IPylonDevice* dev     = nullptr;

        if (serialNumber != nullptr && serialNumber[0] != '\0')
        {
            Pylon::CDeviceInfo di;
            di.SetSerialNumber(serialNumber);
            dev = factory.CreateFirstDevice(di);
        }
        else
        {
            dev = factory.CreateFirstDevice();
        }

        m_p->camera.Attach(dev);
        m_p->camera.Open();
        return true;
    }
    catch (const Pylon::GenericException& e)
    {
        SetError(e.GetDescription());
        return false;
    }
}

void CBaslerLineScan::Close()
{
    Stop();
    try
    {
        if (m_p->camera.IsPylonDeviceAttached())
        {
            m_p->camera.Close();
            m_p->camera.DestroyDevice();
        }
    }
    catch (const Pylon::GenericException& e)
    {
        SetError(e.GetDescription());
    }
}

// ============================================================
//  Configuration
// ============================================================
bool CBaslerLineScan::Configure(const LineScanConfig& cfg)
{
    if (!IsOpen())
    {
        SetError("Camera not open");
        return false;
    }
    if (IsGrabbing())
    {
        SetError("Stop grabbing before Configure");
        return false;
    }

    try
    {
        GenApi::INodeMap& nm = m_p->camera.GetNodeMap();

        Pylon::CEnumParameter(nm, "AcquisitionMode").TrySetValue("Continuous");
        Pylon::CIntegerParameter(nm, "Height").SetValue(cfg.height, Pylon::IntegerValueCorrection_Nearest);

        // Exposure (new: ExposureTime / old racer: ExposureTimeAbs)
        if (!Pylon::CFloatParameter(nm, "ExposureTime").TrySetValue(cfg.exposureUs))
            Pylon::CFloatParameter(nm, "ExposureTimeAbs").TrySetValue(cfg.exposureUs);

        // All frame-level triggers OFF -> continuous acquisition
        SetTrigger("AcquisitionStart", false);   // old racer
        SetTrigger("FrameBurstStart",  false);
        SetTrigger("FrameStart",       false);

        // Line-level trigger
        if (cfg.useEncoder)
        {
            if (!SetTrigger("LineStart", true, cfg.lineSource))
            {
                SetError("Failed to set LineStart trigger");
                return false;
            }
        }
        else
        {
            SetTrigger("LineStart", false);
            Pylon::CBooleanParameter(nm, "AcquisitionLineRateEnable").TrySetValue(true);   // only some models

            if (!Pylon::CFloatParameter(nm, "AcquisitionLineRate").TrySetValue(cfg.lineRateHz) &&
                !Pylon::CFloatParameter(nm, "AcquisitionLineRateAbs").TrySetValue(cfg.lineRateHz))
            {
                SetError("Failed to set line rate (exposure too long or out of range)");
                return false;
            }
        }

        m_p->camera.MaxNumBuffer = cfg.maxBuffers;
        m_p->maxBuffers          = cfg.maxBuffers;
        return true;
    }
    catch (const Pylon::GenericException& e)
    {
        SetError(e.GetDescription());
        return false;
    }
}

bool CBaslerLineScan::SetSaveConfig(const SaveConfig& cfg)
{
    SaveConfig c = cfg;
    c.folder[sizeof(c.folder) - 1] = '\0';
    if (c.everyN   < 1) c.everyN   = 1;
    if (c.maxQueue < 1) c.maxQueue = 1;

    if (c.mode != SaveMode_Off)
    {
        // creates one level only; parent folder must exist
        if (!::CreateDirectoryA(c.folder, nullptr) && ::GetLastError() != ERROR_ALREADY_EXISTS)
        {
            SetError(std::string("Cannot create save folder: ") + c.folder);
            return false;
        }
    }

    std::lock_guard<std::mutex> lk(m_p->cfgMtx);
    m_p->saveCfg = c;
    return true;
}

// Some trigger selectors do not exist on every model -> ignore failure
bool CBaslerLineScan::SetTrigger(const char* selector, bool on, const char* source)
{
    GenApi::INodeMap& nm = m_p->camera.GetNodeMap();
    if (!Pylon::CEnumParameter(nm, "TriggerSelector").TrySetValue(selector))
        return false;
    if (on && source)
        Pylon::CEnumParameter(nm, "TriggerSource").TrySetValue(source);
    return Pylon::CEnumParameter(nm, "TriggerMode").TrySetValue(on ? "On" : "Off");
}

// ============================================================
//  Start / Stop
// ============================================================
bool CBaslerLineScan::Start(FrameCallback cb, void* user)
{
    if (!IsOpen())
    {
        SetError("Camera not open");
        return false;
    }
    if (IsGrabbing())
        return true;

    // session tag for file names (avoids overwriting previous runs)
    SYSTEMTIME st;
    ::GetLocalTime(&st);
    char tag[32];
    sprintf_s(tag, sizeof(tag), "%04d%02d%02d_%02d%02d%02d",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    m_p->sessionTag = tag;

    try
    {
        m_p->cb       = cb;
        m_p->user     = user;
        m_p->frames   = 0;
        m_p->lost     = 0;
        m_p->saved    = 0;
        m_p->saveDrop = 0;
        m_p->haveLast = false;

        // No GrabLoop argument -> we run the RetrieveResult loop ourselves
        m_p->camera.StartGrabbing(Pylon::GrabStrategy_OneByOne);
    }
    catch (const Pylon::GenericException& e)
    {
        SetError(e.GetDescription());
        return false;
    }

    m_p->bSaveRun = true;
    m_p->bProcRun = true;
    m_p->bGrabRun = true;
    m_p->saveThread = boost::thread(&CBaslerLineScan::SaveLoop, this);
    m_p->procThread = boost::thread(&CBaslerLineScan::ProcLoop, this);
    m_p->grabThread = boost::thread(&CBaslerLineScan::GrabLoop, this);

    ::SetThreadPriority(m_p->grabThread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
    ::SetThreadPriority(m_p->procThread.native_handle(), THREAD_PRIORITY_ABOVE_NORMAL);
    ::SetThreadPriority(m_p->saveThread.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
    return true;
}

void CBaslerLineScan::Stop()
{
    if (!m_p->bGrabRun && !m_p->procThread.joinable() && !m_p->saveThread.joinable() && !m_p->camera.IsGrabbing())
        return;

    // 1) grab thread: exits within RetrieveResult timeout
    m_p->bGrabRun = false;
    if (m_p->grabThread.joinable())
        m_p->grabThread.join();

    // 2) proc thread: flag off + wake-up signal (pop_unique blocks)
    m_p->bProcRun = false;
    if (m_p->procThread.joinable())
    {
        m_p->procQ.push_unique(std::unique_ptr<ProcItem>());
        m_p->procThread.join();
    }
    while (!m_p->procQ.empty())             // release leftover driver buffers
        m_p->procQ.pop_unique();

    // 3) save thread: flag off (it polls, no wake-up needed), flushes the rest, then exits
    //    (must finish BEFORE StopGrabbing: save items hold driver buffers)
    m_p->bSaveRun = false;
    if (m_p->saveThread.joinable())
        m_p->saveThread.join();

    // 4) all driver buffers released -> stop camera
    try
    {
        if (m_p->camera.IsGrabbing())
            m_p->camera.StopGrabbing();
    }
    catch (const Pylon::GenericException& e)
    {
        SetError(e.GetDescription());
    }
}

// ============================================================
//  Grab thread
// ============================================================
void CBaslerLineScan::GrabLoop()
{
    while (m_p->bGrabRun)
    {
        Pylon::CGrabResultPtr r;
        try
        {
            // 500 ms timeout so the loop can notice m_p->bGrabRun == false
            if (!m_p->camera.RetrieveResult(500, r, Pylon::TimeoutHandling_Return))
                continue;
        }
        catch (const Pylon::GenericException& e)
        {
            SetError(e.GetDescription());   // e.g. camera disconnected
            continue;
        }

        if (!r.IsValid() || !r->GrabSucceeded())
        {
            ++m_p->lost;
            continue;
        }

        // BlockID gap = dropped frames
        const std::uint64_t bid = r->GetBlockID();
        if (bid != UINT64_MAX)
        {
            if (m_p->haveLast && bid > m_p->lastBlock + 1)
                m_p->lost += bid - m_p->lastBlock - 1;
            m_p->lastBlock = bid;
            m_p->haveLast  = true;
        }

        std::unique_ptr<ProcItem> item(new ProcItem());
        item->result = r;
        item->index  = m_p->frames++;
        m_p->procQ.push_unique(std::move(item));
    }
}

// ============================================================
//  Processing thread
// ============================================================
void CBaslerLineScan::ProcLoop()
{
    while (m_p->bProcRun)
    {
        std::unique_ptr<ProcItem> item = m_p->procQ.pop_unique();
        if (!item)
            continue;           // wake-up signal -> re-check m_p->bProcRun

        const Pylon::CGrabResultPtr& r = item->result;

        size_t stride = 0;
        r->GetStride(stride);

        FrameChunk c = {};
        c.data          = static_cast<const std::uint8_t*>(r->GetBuffer());
        c.width         = static_cast<int>(r->GetWidth());
        c.height        = static_cast<int>(r->GetHeight());
        c.stride        = static_cast<int>(stride);
        c.bytesPerPixel = static_cast<int>((Pylon::BitPerPixel(r->GetPixelType()) + 7) / 8);
        c.frameIndex    = item->index;
        c.blockId       = r->GetBlockID();
        c.timestamp     = r->GetTimeStamp();

        // 1) classification (user callback)
        bool wantSave = false;
        if (m_p->cb != nullptr)
        {
            try
            {
                wantSave = m_p->cb(c, m_p->user);
            }
            catch (...)
            {
                SetError("Exception in frame callback");
            }
        }

        // 2) hand over to save thread (pointer only, no copy)
        const SaveConfig sc     = GetSaveConfigCopy();
        const int        everyN = (sc.everyN < 1) ? 1 : sc.everyN;
        const bool       doSave = (sc.mode == SaveMode_All       && (item->index % everyN) == 0) ||
                                  (sc.mode == SaveMode_Requested && wantSave);
        if (doSave)
            EnqueueSave(r, item->index, sc);

        // item destroyed here -> buffer goes back to the driver (unless the save queue still holds it)
    }
}

void CBaslerLineScan::EnqueueSave(const Pylon::CGrabResultPtr& r, std::uint64_t index, const SaveConfig& sc)
{
    // Saved chunks keep their driver buffer until written.
    // Never let the save queue take more than half of the buffers,
    // otherwise grabbing would starve.
    int limit = sc.maxQueue;
    if (limit > m_p->maxBuffers / 2) limit = m_p->maxBuffers / 2;
    if (limit < 1)                limit = 1;

    if (static_cast<int>(m_p->saveQ.get_size()) >= limit)
    {
        ++m_p->saveDrop;           // disk too slow -> skip, never block processing
        return;
    }

    std::unique_ptr<SaveItem> item(new SaveItem());
    item->result = r;           // ref-count +1, no image copy
    item->index  = index;
    m_p->saveQ.push_unique(std::move(item));
}

// cv::Mat header over the pylon buffer (NO copy)
//   Mono8           -> CV_8UC1
//   Mono10/12/16    -> CV_16UC1
//   BGR8            -> CV_8UC3
//   anything else   -> converted into 'tmp' (temporary, freed after write)
bool CBaslerLineScan::WrapMat(const Pylon::CGrabResultPtr& r, cv::Mat& out, Pylon::CPylonImage& tmp)
{
    try
    {
        const Pylon::EPixelType pt = r->GetPixelType();
        const int w = static_cast<int>(r->GetWidth());
        const int h = static_cast<int>(r->GetHeight());

        size_t stride = 0;
        r->GetStride(stride);
        void* buf = r->GetBuffer();

        if (pt == Pylon::PixelType_Mono8)
        {
            out = cv::Mat(h, w, CV_8UC1, buf, stride);
        }
        else if (pt == Pylon::PixelType_Mono10 || pt == Pylon::PixelType_Mono12 || pt == Pylon::PixelType_Mono16)
        {
            out = cv::Mat(h, w, CV_16UC1, buf, stride);
        }
        else if (pt == Pylon::PixelType_BGR8packed)
        {
            out = cv::Mat(h, w, CV_8UC3, buf, stride);
        }
        else
        {
            const bool mono = Pylon::IsMonoImage(pt);
            m_p->converter.OutputPixelFormat = mono ? Pylon::PixelType_Mono8 : Pylon::PixelType_BGR8packed;
            m_p->converter.Convert(tmp, r);

            size_t tmpStride = 0;
            tmp.GetStride(tmpStride);
            out = cv::Mat(h, w, mono ? CV_8UC1 : CV_8UC3, tmp.GetBuffer(), tmpStride);
        }
        return true;
    }
    catch (const Pylon::GenericException& e)
    {
        SetError(e.GetDescription());
    }
    catch (const cv::Exception& e)
    {
        SetError(e.what());
    }
    return false;
}

// ============================================================
//  Save thread
// ============================================================
void CBaslerLineScan::SaveLoop()
{
    while (m_p->bSaveRun)
    {
        if (m_p->saveQ.empty())
        {
            ::Sleep(5);         // nothing to save -> rest (disk latency does not matter here)
            continue;
        }
        std::unique_ptr<SaveItem> item = m_p->saveQ.pop_unique();
        if (item)
            WriteItem(*item);
    }

    // flush: write everything still queued before exiting
    while (!m_p->saveQ.empty())
    {
        std::unique_ptr<SaveItem> item = m_p->saveQ.pop_unique();
        if (item)
            WriteItem(*item);
    }
}

void CBaslerLineScan::WriteItem(const SaveItem& item)
{
    const SaveConfig sc = GetSaveConfigCopy();

    const char*      ext = "bmp";
    std::vector<int> params;
    if (sc.format == SaveFormat_Png)
    {
        ext = "png";
        params.push_back(cv::IMWRITE_PNG_COMPRESSION);
        params.push_back(1);                          // 0..9, 1 = fast
    }
    else if (sc.format == SaveFormat_Tiff)
    {
        ext = "tif";
    }
    // BMP cannot store 16-bit -> use PNG or TIFF for Mono10/12/16

    char path[512];
    sprintf_s(path, sizeof(path), "%s\\%s_%010llu.%s",
              sc.folder, m_p->sessionTag.c_str(),
              static_cast<unsigned long long>(item.index), ext);

    cv::Mat            mat;
    Pylon::CPylonImage tmp;             // used only when a format conversion is needed
    if (!WrapMat(item.result, mat, tmp))
        return;

    try
    {
        if (cv::imwrite(path, mat, params))
            ++m_p->saved;
        else
            SetError(std::string("imwrite failed: ") + path);
    }
    catch (const cv::Exception& e)
    {
        SetError(e.what());
    }
}

// ============================================================
//  Helpers / status
// ============================================================
void CBaslerLineScan::SetError(const std::string& s)
{
    std::lock_guard<std::mutex> lk(m_p->errMtx);
    m_p->err = s;
}

SaveConfig CBaslerLineScan::GetSaveConfigCopy() const
{
    std::lock_guard<std::mutex> lk(m_p->cfgMtx);
    return m_p->saveCfg;
}

bool CBaslerLineScan::IsOpen() const
{
    return m_p->camera.IsPylonDeviceAttached() && m_p->camera.IsOpen();
}

bool CBaslerLineScan::IsGrabbing() const
{
    return m_p->bGrabRun;
}

std::uint64_t CBaslerLineScan::GetFrameCount() const
{
    return m_p->frames;
}

std::uint64_t CBaslerLineScan::GetLostCount() const
{
    return m_p->lost;
}

std::size_t CBaslerLineScan::GetQueueDepth() const
{
    return m_p->procQ.get_size();
}

std::uint64_t CBaslerLineScan::GetSavedCount() const
{
    return m_p->saved;
}

std::uint64_t CBaslerLineScan::GetSaveDropCount() const
{
    return m_p->saveDrop;
}

std::size_t CBaslerLineScan::GetSaveQueueDepth() const
{
    return m_p->saveQ.get_size();
}

std::string CBaslerLineScan::GetLastError() const
{
    std::lock_guard<std::mutex> lk(m_p->errMtx);
    return m_p->err;   // copy -> safe even if another thread updates it
}
