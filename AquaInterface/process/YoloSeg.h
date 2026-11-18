#pragma once
#include "Onnxruntime.h"
#include "YoloSegtypes.h"

struct Letterbox 
{ 
	float r = 1.f; 
	int px = 0; 
	int py = 0; 
};

class CYoloSeg
{
public:
	CYoloSeg();
	~CYoloSeg();
	CYoloSeg(const CYoloSeg&) = delete;
	CYoloSeg& operator=(const CYoloSeg&) = delete;
	/*2026/11/03 kimju
	* @brief Initialize YOLO segmentation model
	* 
	*/
	bool CreateYoloSeg(std::string weight_path, int num_thread, Ort::Env* env);
	void ReleaseYoloSeg();
	bool IsCreated() const { return m_pOnnxruntime != NULL && m_pBlob != NULL; }
	bool IsSeg() const { return m_bSeg; }

	void SetParams(const SegParams& params) { m_params = params; }
	const SegParams& GetParams() const { return m_params; }

	void Warmup(int nCount);
	/*2026/11/04 kimju
	* @brief YOLO-seg inference
	* @param [IN]  bgr : original image (CV_8UC3)
	* @param [OUT] out : objects (box / polygon / score) in original image pixels, timing
	* @return false on error (out.error)
	*/
	bool Run(const cv::Mat& bgr, SegResult& out);

private:

	void Preprocess(const cv::Mat& bgr, Letterbox& lb);
	void Postprocess(const float* out0, const std::vector<int64_t>& s0, const float* protos, const std::vector<int64_t>& sp, int nmIn0, const Letterbox& lb, int W, int H, SegResult& out);
	double MsSince(const std::chrono::steady_clock::time_point& t0) const;
	std::vector<int64_t> GetShape(const Ort::Value& v) const;
	void Log(const char* level, const char* fmt, ...) const;

	COnnxruntime* m_pOnnxruntime;
	Ort::MemoryInfo* m_pMemInfo;

	SegParams    m_params;
	int          m_inW; 
	int          m_inH;
	bool         m_bSeg;

	float* m_pBlob;          // NCHW input buffer, size 3 * m_inH * m_inW (new[] in Open, delete[] in Close)
	cv::Mat* m_pCanvas;        // letterbox canvas (reused)
};	