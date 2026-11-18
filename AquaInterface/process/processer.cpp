#include "pch.h"
#include "processer.h"
#include "Yoloseg.h"
#include "Anomaly.h"
#include "shrimp_crop.h"
//#include "Onnxuntime.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <mutex>


Cprocesser::Cprocesser()
{ 
	m_pYoloseg = NULL;
	m_pOrtEnv = NULL;
	m_pAnomaly = NULL;
	m_pCrop = NULL;
}
Cprocesser::~Cprocesser()
{
	if (m_pYoloseg)
	{
		delete m_pYoloseg;
		m_pYoloseg = NULL;
	}
	if (m_pOrtEnv)
	{
		delete m_pOrtEnv;
		m_pOrtEnv = NULL;
	}
	if (m_pAnomaly)
	{
		delete m_pAnomaly;
		m_pAnomaly = NULL;
	}
	if (m_pCrop)
	{
		delete m_pCrop;
		m_pCrop = NULL;
	}
}

std::string Cprocesser::GetExeDir() const
{
	char path[MAX_PATH] = { 0 };
	::GetModuleFileNameA(NULL, path, MAX_PATH);     // exe 전체 경로 (CP_ACP, 한글 OK)
	std::string dir(path);
	const size_t pos = dir.find_last_of("\\/");
	return (pos == std::string::npos) ? std::string() : dir.substr(0, pos + 1);
}
bool Cprocesser::initCrrop()
{
	if (m_pCrop == NULL)
	{
		m_pCrop = new CShrimpCrop();
	}
	return true;
}
bool Cprocesser::InitAnomaly()
{
	if (m_pOrtEnv == NULL)
	{
		m_pOrtEnv = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "AquaInterface");
	}
	if (m_pAnomaly == NULL)
	{
		m_pAnomaly = new CAnomaly();
	}
	const std::string modelPath = GetExeDir() + "\\efficientad_medium.onnx";
	std::cout << "[Processer] anomaly model : " << modelPath << std::endl;
	if (::GetFileAttributesA(modelPath.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		std::cout << "[Processer] model file not found : " << modelPath << std::endl;
		return false;
	}
	AnomalyParams ap;
	ap.maxBatch = 4;
	ap.threshold = 0.20f;
	ap.warmup = 1;
	if (!m_pAnomaly->Open(modelPath, 2, m_pOrtEnv, ap))
	{
		std::cout << "[Processer] InitAnomaly FAIL" << std::endl;
		return false;
	}
	std::cout << "[Processer] Anomaly initialized successfully" << std::endl;
	return true;
}

bool Cprocesser::InitYoloseg()
{
	if (m_pOrtEnv == NULL)
	{
		m_pOrtEnv = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "AquaInterface");
	}
	if (m_pYoloseg == NULL)
	{
		m_pYoloseg = new CYoloSeg();
	}
	const std::string modelPath = GetExeDir() + "best1560.onnx";
	std::cout << "[Processer] seg model : " << modelPath << std::endl;
	if (::GetFileAttributesA(modelPath.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		std::cout << "[Processer] model file not found : " << modelPath << std::endl;
		return false;
	}
	SegParams sp;
	m_pYoloseg->SetParams(sp);
	if (!m_pYoloseg->CreateYoloSeg(modelPath, 2, m_pOrtEnv))
	{
		std::cout << "[Processer] InitYoloseg FAIL" << std::endl;
		return false;
	}
	m_pYoloseg->Warmup(2);
	std::cout << "[Processer] Yoloseg initialized successfully" << std::endl;
	return true;
}

bool Cprocesser::RunYoloseg(cv::Mat& bgr, SegResult& result)
{
	if (m_pYoloseg)
	{
		result.Clear();
		m_pYoloseg->Run(bgr, result);
		std::cout << "Yoloseg result: " << result.Count() << std::endl;
	}
	else
	{
		result.Clear();
		std::cout << "Yoloseg not initialized." << std::endl;
		return false;
	}
	return true;
}


bool Cprocesser::Runprocess(cv::Mat& bgr, SegResult& result)
{
	if (!RunYoloseg(bgr, result))
	{
		return false;
	}
	if (m_pAnomaly == NULL || !m_pAnomaly->IsOpen() || m_pCrop == NULL)
	{
		std::cout << "[Processer] count " << result.Count() << " (no anomaly model)" << std::endl;
		return true;
	}
	std::vector<cv::Mat> crops;
	std::vector<int> objIdx;
	if (!m_pCrop->Make(bgr, result, crops, objIdx))
	{
		std::cout << "[Processer] crop FAIL -> count only" << std::endl;
		return true;
	}
	if (!crops.empty())
	{
		std::vector<float> scores;
		if (m_pAnomaly->Run(crops, scores))
		{
			const float th = m_pAnomaly->Params().threshold;
			for (size_t i = 0; i < objIdx.size(); ++i)
			{
				const int k = objIdx[i];
				if (k < 0 || k >= (int)result.objects.size() || scores[i] < 0.f)
					continue;
				SegObject& o = result.objects[k];
				o.anomalyScore = scores[i];
				o.verdict = (scores[i] >= th) ? Verdict::Dead : Verdict::Normal;
			}
			result.anomalyMs = m_pAnomaly->LastMs();
		}
		else
		{
			std::cout << "[Processer] anomaly FAIL -> count only" << std::endl;
		}
	}
	int nPartial = 0;
	for (const SegObject& o : result.objects)
		if (o.cropClass == CropClass::Partial) ++nPartial;
	std::cout << "[Processer] count " << result.Count()
		<< "  dead " << result.CountVerdict(Verdict::Dead)
		<< "  judged " << crops.size()
		<< "  partial " << nPartial
		<< " | seg " << (result.preMs + result.inferMs + result.postMs)
		<< " crop " << result.cropMs
		<< " anomaly " << result.anomalyMs << " ms "
		<< (result.cuda ? "GPU" : "CPU") << std::endl;
	return true;
	return true;
}




cv::Scalar Cprocesser::ObjectColor(const SegObject& o, bool showPartial) const
{
	if (o.verdict == Verdict::Dead)                       return cv::Scalar(0, 0, 255);     // red
	if (showPartial && o.cropClass == CropClass::Skip)    return cv::Scalar(160, 160, 160); // gray
	if (showPartial && o.cropClass == CropClass::Partial) return cv::Scalar(0, 220, 255);   // yellow
	return cv::Scalar(0, 200, 0);                                                           // green
}

void Cprocesser::PutTextOutlined(cv::Mat& img, const std::string& s, cv::Point org, double fs, cv::Scalar col, int th) const
{
	cv::putText(img, s, org, cv::FONT_HERSHEY_SIMPLEX, fs, cv::Scalar(0, 0, 0), th + 2, cv::LINE_AA);   // 검은 테두리
	cv::putText(img, s, org, cv::FONT_HERSHEY_SIMPLEX, fs, col, th, cv::LINE_AA);
}

void Cprocesser::Draw(cv::Mat& img, const SegResult& res, const SegDrawOptions& opt) const
{
	if (img.empty() || img.type() != CV_8UC3)
	{
		std::cout << "[Processer] Draw: image empty or not BGR 8UC3" << std::endl;
		return;
	}
	const double sc = opt.scale > 0.0 ? opt.scale : 1.0;
	const int    th = opt.thickness > 0 ? opt.thickness
		: (std::max)(1, (int)((std::max)(img.cols, img.rows) / 900.0 + 0.5));
	const double fs = 0.35 + 0.15 * th;

	int nDead = 0;
	std::vector<std::vector<cv::Point>> one(1);

	for (const SegObject& o : res.objects)
	{
		const cv::Scalar col = ObjectColor(o, opt.showPartial);
		if (o.verdict == Verdict::Dead) ++nDead;

		const cv::Rect r((int)(o.box.x * sc), (int)(o.box.y * sc),
			(std::max)(1, (int)(o.box.width * sc)), (std::max)(1, (int)(o.box.height * sc)));

		// 윤곽선 (마스크 없으면 박스)
		if (!o.polygon.empty())
		{
			one[0].resize(o.polygon.size());
			for (size_t i = 0; i < o.polygon.size(); ++i)
				one[0][i] = cv::Point((int)(o.polygon[i].x * sc + 0.5), (int)(o.polygon[i].y * sc + 0.5));
			cv::polylines(img, one, true, col, th, cv::LINE_AA);
		}
		else
		{
			cv::rectangle(img, r, col, th, cv::LINE_AA);
		}

		// 번호 / 점수
		std::string label;
		if (opt.drawNumber) label = std::to_string(o.id);
		if (opt.drawScore)
		{
			char b[32];
			sprintf_s(b, " %.2f", o.score);
			label += b;
		}
		if (!label.empty())
			PutTextOutlined(img, label, cv::Point(r.x, (std::max)(12, r.y - 3)), fs, cv::Scalar(255, 255, 255), (std::max)(1, th - 1));
	}

	// 상단 배너
	if (opt.drawBanner)
	{
		char banner[160];
		if (res.ok)
			sprintf_s(banner, "count %d   dead %d   (%.0f ms%s)", res.Count(), nDead,
				res.preMs + res.inferMs + res.postMs + res.cropMs + res.anomalyMs, res.cuda ? ", GPU" : ", CPU");
		else
			sprintf_s(banner, "FAIL: %s", res.error.c_str());
		int base = 0;
		const double bfs = 0.5 + 0.2 * th;
		const cv::Size ts = cv::getTextSize(banner, cv::FONT_HERSHEY_SIMPLEX, bfs, th, &base);
		cv::rectangle(img, cv::Rect(0, 0, (std::min)(img.cols, ts.width + 16), ts.height + base + 12), cv::Scalar(0, 0, 0), cv::FILLED);
		cv::putText(img, banner, cv::Point(8, ts.height + 6), cv::FONT_HERSHEY_SIMPLEX, bfs,
			res.ok ? cv::Scalar(255, 255, 255) : cv::Scalar(0, 0, 255), th, cv::LINE_AA);
	}

	std::cout << "[Processer] Draw: " << res.Count() << " object(s), dead " << nDead
		<< ", scale " << sc << ", thickness " << th << std::endl;
}