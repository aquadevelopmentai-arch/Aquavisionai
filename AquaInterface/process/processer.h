
#pragma once

#include "YoloSegtypes.h"
#ifdef PROCESS_EXPORTS
#define PROCESS_API __declspec(dllexport)
#else
#define PROCESS_API __declspec(dllimport)
#endif

class COnnxruntime;
class CYoloSeg;
class CAnomaly;
class CShrimpCrop;

namespace Ort { struct Env; }

class PROCESS_API Cprocesser
{
public:
	Cprocesser();
	~Cprocesser();
	/*
	* @brief Initialize YoloSeg
	* @param [IN] modelPath : YoloSeg model file path
	* @param [IN] sp        : YoloSeg parameters
	* @return true if initialized successfully, false otherwise
	*/
	bool InitYoloseg(const std::string& modelPath, const SegParams& sp = SegParams());
	/*
	* @brief Initialize Anomaly Detection
	* @param [IN] modelPath : Anomaly model file path
	* @param [IN] ap        : Anomaly parameters
	* @return true if initialized successfully, false otherwise
	*/
	bool InitAnomaly(const std::string& modelPath, const AnomalyParams& ap = AnomalyParams());
	/*
	* @brief Initialize Shrimp Crop
	* @return true if initialized successfully, false otherwise
	*/
	bool initCrrop();
	/*
	* @brief Run YoloSeg inference
	* @param [IN] bgr  : Input image
	* @param [OUT] result : Inference result
	* @return true if inference completed successfully, false otherwise
	*/
	bool RunYoloseg(cv::Mat& bgr, SegResult& result);
	/*
	* @brief Draw segmentation results on the image
	* @param [IN] img  : Input image
	* @param [IN] res  : Segmentation result
	* @param [IN] opt  : Drawing options
	*/
	void Draw(cv::Mat& img, const SegResult& res, const SegDrawOptions& opt) const;

	bool InitFromSetting(const std::string& iniPath, std::string& segmodel, std::string& admodel);
	bool IsAnomalyReady() const;
	bool IsFile(const std::string& path) const;
	int   ReadIniInt(const std::string& iniPath, const char* section, const char* key, int def) const;
	float ReadIniFloat(const std::string& iniPath, const char* section, const char* key, float def) const;

private:
	cv::Scalar ObjectColor(const SegObject& o, bool showPartial) const;
	void PutTextOutlined(cv::Mat& img, const std::string& s, cv::Point org, double fs, cv::Scalar col, int th) const;
	std::string GetExeDir() const;

	Ort::Env* m_pOrtEnv;

	CYoloSeg* m_pYoloseg;
	CAnomaly* m_pAnomaly;
	CShrimpCrop* m_pCrop;

	std::string ReadIniString(const std::string& iniPath, const char* section, const char* key) const;
	std::string CleanPath(const std::string& value) const;   // \\ -> \, 상대 경로 -> exe 폴더
	//std::string GetExeDir() const;
};