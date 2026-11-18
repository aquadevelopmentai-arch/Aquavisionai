#pragma once
// Anomaly.h - EfficientAD ONNX batch inference (process.dll internal, NOT exported)
//   input  "images"      [N, 3, 256, 256] float32 RGB 0..1   (ImageNet normalize is inside the model)
//   output "anomaly_map" [N, 1, 64, 64], "score" [N]          score >= threshold -> dead
//
//   session = COnnxruntime (CreateSession / GetInputShape / GetOutputNames / Run)
//   batch : chunks of maxBatch, last chunk padded -> input shape never changes (cuDNN tunes once)

#include "YoloSegTypes.h"      // AnomalyParams
#include "Onnxruntime.h"
#include <chrono>

class CAnomaly
{
public:
	CAnomaly();
	~CAnomaly();
	CAnomaly(const CAnomaly&) = delete;
	CAnomaly& operator=(const CAnomaly&) = delete;
	/*
	* @brief Open Anomaly Detection session
	* @param [IN] modelPath : Anomaly model file path
	* @param [IN] num_thread : Number of threads for inference
	* @param [IN] pEnv : ONNX Runtime environment
	* @param [IN] params : Anomaly parameters
	* @return true if session opened successfully, false otherwise
	*/
	bool Open(const std::string& modelPath, int num_thread, Ort::Env* pEnv, const AnomalyParams& params);
	/*
	* @brief Close Anomaly Detection session
	*/
	void Close();
	bool IsOpen() const;

	// crops : BGR 8UC3 (resized to the model input). scores.size() == crops.size() (-1 = not scored)
	// pMaps : optional, one CV_32F map per crop (64x64)
	/*
	* @brief Run Anomaly Detection inference
	* @param [IN] crops : Vector of input images
	* @param [OUT] scores : Vector of anomaly scores
	* @param [OUT] pMaps : Optional vector of anomaly maps
	* @return true if inference completed successfully, false otherwise
	*/
	bool Run(const std::vector<cv::Mat>& crops, std::vector<float>& scores, std::vector<cv::Mat>* pMaps = NULL);

	const AnomalyParams& Params() const { return m_params; }
	double LastMs() const { return m_lastMs; }
	void FillSlot(int slot, const cv::Mat& bgr);                 // one crop -> NCHW RGB 0..1 at slot
	/*
	* @brief Run the model on the input blob and get the outputs
	*/
	bool RunBlob(std::vector<Ort::Value>& outs);                 // m_pBlob [maxBatch,3,H,W] -> outputs
	/*
	* @brief Get the shape of an ONNX tensor
	* @param [IN] v : ONNX tensor
	* @return vector of dimensions
	*/
	std::vector<int64_t> GetShape(const Ort::Value& v) const;
	std::string ToAnsi(const std::wstring& w) const;             // modelPath -> CP_ACP (CreateSession takes std::string)
	double MsSince(const std::chrono::steady_clock::time_point& t0) const;
	void Log(const char* level, const char* fmt, ...) const;     // std::cout

private:

	COnnxruntime* m_pSession;      // new in Open, delete in Close
	Ort::MemoryInfo* m_pMemInfo;      // new in Open, delete in Close
	float* m_pBlob;         // maxBatch * 3 * inH * inW (new[] / delete[])
	AnomalyParams    m_params;
	int              m_inW, m_inH;
	int              m_idxMap, m_idxScore;   // output index of "anomaly_map" / "score"
	double           m_lastMs;
};