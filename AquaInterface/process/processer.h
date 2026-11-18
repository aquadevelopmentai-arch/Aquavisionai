
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

	bool InitYoloseg();
	bool InitAnomaly();
	bool initCrrop();
	bool RunYoloseg(cv::Mat& bgr, SegResult& result);
	void Draw(cv::Mat& img, const SegResult& res, const SegDrawOptions& opt) const;
	bool Runprocess(cv::Mat& bgr, SegResult& result);
private:
	cv::Scalar ObjectColor(const SegObject& o, bool showPartial) const;
	void PutTextOutlined(cv::Mat& img, const std::string& s, cv::Point org, double fs, cv::Scalar col, int th) const;
	std::string GetExeDir() const;

	Ort::Env* m_pOrtEnv;

	CYoloSeg* m_pYoloseg;
	CAnomaly* m_pAnomaly;
	CShrimpCrop* m_pCrop;
};