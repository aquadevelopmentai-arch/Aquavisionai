#pragma once
// ============================================================
//  BaslerCamIF - Basler line scan camera interface DLL  (PUBLIC header)
//
//  3 threads inside the object:
//      grab thread : camera -> proc queue        (TIME_CRITICAL, blocking wait)
//      proc thread : proc queue -> user callback (ABOVE_NORMAL,  blocking wait)
//      save thread : save queue -> cv::imwrite   (BELOW_NORMAL,  Sleep when empty)
//  No image copy: queues hold the pylon buffer pointer.
//
//  This header needs NO pylon / boost / OpenCV include paths.
//  All camera state lives in BaslerCamData (BaslerCamData.h, DLL-internal),
//  accessed through the pointer m_p.
// ============================================================
#include <cstddef>
#include <cstdint>
#include <string>

#ifdef BASLERCAMIF_EXPORTS
#define BASLERCAMIF_API __declspec(dllexport)
#else
#define BASLERCAMIF_API __declspec(dllimport)
#endif

// forward declarations (definitions only needed inside the DLL)
struct BaslerCamData;
struct SaveItem;
namespace Pylon { class CGrabResultPtr; class CPylonImage; }
namespace cv    { class Mat; }

// ------------------------------------------------------------
// Settings
// ------------------------------------------------------------
struct LineScanConfig
{
    int    height         = 256;      // lines per chunk
    double lineRateHz     = 10000.0;  // internal line rate (used when useEncoder == false)
    double exposureUs     = 50.0;     // must be shorter than 1 / lineRate
    bool   useEncoder     = false;    // true: LineStart from encoder / external signal
    char   lineSource[32] = "Line1";  // TriggerSource when useEncoder == true
    int    maxBuffers     = 300;      // driver buffer count
};

enum SaveMode
{
    SaveMode_Off       = 0,   // no saving
    SaveMode_All       = 1,   // save every N-th chunk
    SaveMode_Requested = 2    // save only when the callback returns true (e.g. NG only)
};

enum SaveFormat
{
    SaveFormat_Bmp  = 0,      // fastest (no compression)
    SaveFormat_Png  = 1,
    SaveFormat_Tiff = 2
};

struct SaveConfig
{
    int  mode        = SaveMode_Off;
    int  format      = SaveFormat_Bmp;
    int  everyN      = 1;                 // SaveMode_All only
    int  maxQueue    = 30;                // pending images (holds driver buffers!)
    char folder[260] = "C:\\AquaImages";  // use an ASCII path
};

// ------------------------------------------------------------
// Data passed to the callback
// ------------------------------------------------------------
struct FrameChunk
{
    const std::uint8_t* data;         // valid ONLY inside the callback
    int                 width;
    int                 height;
    int                 stride;       // bytes per line
    int                 bytesPerPixel;
    std::uint64_t       frameIndex;   // receive order (from 0)
    std::uint64_t       blockId;      // camera frame id (for drop check)
    std::uint64_t       timestamp;    // camera timestamp (ticks)
};

// Called from the processing thread (not the UI thread).
// Return true to save this chunk (used with SaveMode_Requested).
typedef bool (*FrameCallback)(const FrameChunk& chunk, void* user);

// ------------------------------------------------------------
// Camera class
// ------------------------------------------------------------
class BASLERCAMIF_API CBaslerLineScan
{
public:
    CBaslerLineScan();
    ~CBaslerLineScan();
    CBaslerLineScan(const CBaslerLineScan&) = delete;
    CBaslerLineScan& operator=(const CBaslerLineScan&) = delete;

    bool Open(const char* serialNumber = nullptr);   // nullptr: first camera found
    void Close();

    bool Configure(const LineScanConfig& cfg);       // after Open, before Start
    bool SetSaveConfig(const SaveConfig& cfg);       // any time (also while grabbing)

    bool Start(FrameCallback cb, void* user);
    void Stop();                                     // never call inside the callback

    bool IsOpen() const;
    bool IsGrabbing() const;

    std::uint64_t GetFrameCount() const;       // chunks received
    std::uint64_t GetLostCount() const;        // failed + dropped by camera/driver
    std::size_t   GetQueueDepth() const;       // waiting for processing (should stay near 0)

    std::uint64_t GetSavedCount() const;       // images written to disk
    std::uint64_t GetSaveDropCount() const;    // skipped because save queue was full
    std::size_t   GetSaveQueueDepth() const;   // waiting for disk write

    std::string   GetLastError() const;

private:
    // thread bodies
    void GrabLoop();
    void ProcLoop();
    void SaveLoop();

    // helpers
    void       EnqueueSave(const Pylon::CGrabResultPtr& r, std::uint64_t index, const SaveConfig& sc);
    void       WriteItem(const SaveItem& item);
    bool       WrapMat(const Pylon::CGrabResultPtr& r, cv::Mat& out, Pylon::CPylonImage& tmp);   // no copy
    bool       SetTrigger(const char* selector, bool on, const char* source = nullptr);
    void       SetError(const std::string& s);
    SaveConfig GetSaveConfigCopy() const;

private:
    BaslerCamData* m_p;   // created in constructor, deleted in destructor
};


