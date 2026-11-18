#pragma once
// YoloSegTypes.h - YOLO-seg input / output structs (process.dll <-> AquaInterface)
//   * same compiler / same CRT for both projects -> std::vector / cv::Mat can cross the DLL boundary
//   * coordinates are ORIGINAL image pixels (letterbox already undone)
//   * one SegObject per shrimp; the counting / sorter / anomaly steps fill their own fields later

#include <string>
#include <vector>
#include <opencv2/core.hpp>

// ------------------------------------------------------------------ settings (from Infer.ini [Counting.Yolo] / [Sorter.Yolo])
struct SegParams
{
    std::wstring modelPath;
    bool   useCuda = true;
    int    deviceId = 0;
    float  confTh = 0.25f;
    float  iouTh = 0.70f;
    int    maxDet = 1000;
    float  maskTh = 0.50f;
    bool   useMask = true;
    int    warmup = 2;
    double minPolyArea = 2.0;
    int    inputSize = 0;     // dynamic ONNX만 사용 (학습 imgsz, 32의 배수). 0 = ONNX 입력 크기 사용
};

// ------------------------------------------------------------------ anomaly settings (Infer.ini [Counting.Anomaly])
struct AnomalyParams
{
    std::wstring modelPath;          // models\efficientad_shrimp.onnx
    bool  useCuda = true;
    int   deviceId = 0;
    int   maxBatch = 1;            // crops per GPU call (last chunk padded -> fixed shape)
    float threshold = 0.5f;          // score >= threshold -> dead (export json: th_good_p99 / th_youden)
    int   warmup = 2;
};

// ------------------------------------------------------------------ crop settings (Infer.ini [Counting.Crop])
//   MUST match the crops the anomaly model was trained with (prepare_crops.py defaults)
struct CropParams
{
    int    size = 256;         // output crop (square, letterboxed)
    int    margin = 4;           // px around the visible region
    float  minVisible = 0.6f;        // visible ratio below this -> Partial (not judged)
    int    border = 2;           // px from the image edge counted as "cut"
    int    erode = 3;           // px eroded before color features
    int    bgB = 0, bgG = 0, bgR = 0;// background color outside the shrimp
    bool   align = true;        // rotate the long axis horizontal
    double minArea = 0.0;         // polygon area below this -> Skip (0 = off)
};

// ------------------------------------------------------------------ one shrimp
enum class CropClass : int
{
    None = 0,     // crop not made yet
    Judge = 1,     // visible enough -> color rule + anomaly
    Partial = 2,     // heavily covered / cut by the border -> counted only
    Skip = 3      // fully covered by others
};

enum class Verdict : int
{
    Unknown = 0,     // not judged (partial / blur / not run)
    Normal = 1,
    Dead = 2
};

struct SegObject
{
    // --- detector (CYoloSeg)
    int                    id = 0;        // 1..N (drawn number)
    cv::Rect2f             box;                  // x, y, w, h (original image)
    float                  score = 0.f;
    int                    classId = 0;        // always 0 (shrimp)
    std::vector<cv::Point> polygon;              // outline, empty if no mask -> use box
    double                 area = 0.0;      // polygon area (px^2), box area if no polygon
    bool                   hasMask = false;

    // --- crop step (overlap removed, filled by the crop / anomaly stage)
    CropClass              cropClass = CropClass::None;
    float                  visibleRatio = 1.f;   // visible area / own area
    bool                   onBorder = false;
    int                    overlaps = 0;     // number of shrimp overlapping this one
    float                  labL = 0.f, labA = 0.f, labB = 0.f;   // color features (Lab, OpenCV 0..255)
    float                  texture = 0.f;      // Laplacian variance (transparent live shrimp -> high)

    // --- anomaly / decision (CAnomaly + color rule)
    float                  anomalyScore = -1.f;  // -1 = not run
    Verdict                verdict = Verdict::Unknown;

    // --- sorter only (AquaDrift)
    float                  lengthMm = 0.f;      // from polygon + mm/px scale
    float                  weightG = 0.f;      // from the length-weight table
    int                    grade = -1;       // 0: <2g, 1: 2~4g, 2: >4g, -1: not graded
};

// ------------------------------------------------------------------ one image
struct SegResult
{
    bool                   ok = false;
    std::string            error;                // filled when ok == false
    int                    imgW = 0, imgH = 0;
    std::vector<SegObject> objects;

    // timing (ms) -> log + UI
    double preMs = 0.0, inferMs = 0.0, postMs = 0.0, cropMs = 0.0, anomalyMs = 0.0;
    bool   cuda = false;                        // provider actually used

    int Count() const { return (int)objects.size(); }
    int CountVerdict(Verdict v) const
    {
        int n = 0;
        for (const SegObject& o : objects) if (o.verdict == v) ++n;
        return n;
    }
    void Clear()
    {
        ok = false; error.clear(); imgW = imgH = 0; objects.clear();
        preMs = inferMs = postMs = cropMs = anomalyMs = 0.0; cuda = false;
    }
};


struct SegDrawOptions
{
    double scale = 1.0;          // display / original (축소 이미지에 그릴 때)
    bool   drawNumber = false;   // 객체 번호
    bool   drawBanner = false;   // 상단 "count N dead D (ms)"
    bool   drawScore = false;   // detector score (debug)
    bool   showPartial = false;  // partial: 노랑, skip: 회색
    int    thickness = 0;      // 0 = 이미지 크기에 맞춰 자동
};