// Anomaly.cpp - EfficientAD batch inference (process.dll internal), session = COnnxruntime
#include "pch.h"
#include "Anomaly.h"

#include <opencv2/imgproc.hpp>
#include <windows.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iostream>

CAnomaly::CAnomaly()
	: m_pSession(NULL), m_pMemInfo(NULL), m_pBlob(NULL), m_inW(256), m_inH(256),
	m_idxMap(0), m_idxScore(1), m_lastMs(0.0)
{
}

CAnomaly::~CAnomaly()
{
	Close();
}

// ------------------------------------------------------------------ helpers
void CAnomaly::Log(const char* level, const char* fmt, ...) const
{
	char msg[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf_s(msg, sizeof(msg), _TRUNCATE, fmt, ap);
	va_end(ap);
	std::cout << "[" << level << "] " << msg << std::endl;
}

std::string CAnomaly::ToAnsi(const std::wstring& w) const
{
	if (w.empty())
		return std::string();
	const int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, NULL, 0, NULL, NULL);
	std::string s(n > 0 ? n - 1 : 0, '\0');
	if (n > 0)
		WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, &s[0], n, NULL, NULL);
	return s;
}

double CAnomaly::MsSince(const std::chrono::steady_clock::time_point& t0) const
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

std::vector<int64_t> CAnomaly::GetShape(const Ort::Value& v) const
{
	return v.GetTensorTypeAndShapeInfo().GetShape();
}

// ------------------------------------------------------------------ open / close
bool CAnomaly::IsOpen() const
{
	return m_pSession != NULL && m_pSession->IsCreated() && m_pBlob != NULL;
}

void CAnomaly::Close()
{
	if (m_pSession != NULL)
	{
		m_pSession->ReleaseSession();
		delete m_pSession;
		m_pSession = NULL;
	}
	if (m_pMemInfo != NULL)
	{
		delete m_pMemInfo;
		m_pMemInfo = NULL;
	}
	if (m_pBlob != NULL)
	{
		delete[] m_pBlob;
		m_pBlob = NULL;
	}
}

bool CAnomaly::Open(const std::string& modelPath, int num_thread, Ort::Env* pEnv, const AnomalyParams& params)
{
	Close();
	m_params = params;
	if (m_params.maxBatch < 1)
		m_params.maxBatch = 1;
	const std::string path = modelPath;
	std::cout <<"INFO [Anomaly] open "<< path <<"  cuda=" << (int)params.useCuda << " maxBatch=" << m_params.maxBatch << " threshold=" << params.threshold << std::endl;

	// 1) session (COnnxruntime : CUDA -> CPU fallback inside)
	m_pSession = new COnnxruntime();
	if (!m_pSession->CreateSession(path, 2, pEnv, params.useCuda, params.deviceId))
	{
		std::cout <<"ERROR [Anomaly] model open failed"<<std::endl;
		Close();
		return false;
	}

	// 2) input [N,3,H,W]
	const std::vector<int64_t> in = m_pSession->GetInputShape(0);
	if (in.size() != 4)
	{
		std::cout <<"ERROR [Anomaly] unexpected input rank "<< in.size() <<" (need [N,3,H,W])" << std::endl;
		Close();
		return false;
	}
	m_inH = in[2] > 0 ? (int)in[2] : 256;
	m_inW = in[3] > 0 ? (int)in[3] : 256;
	if (in[0] > 0 && in[0] != m_params.maxBatch)
	{
		std::cout <<"WARN [Anomaly] model batch is fixed to "<< (long long)in[0] <<" -> maxBatch "<< m_params.maxBatch <<" -> "<< (long long)in[0] << std::endl;
		m_params.maxBatch = (int)in[0];
	}

	// 3) outputs (find by name, default 0 = anomaly_map, 1 = score)
	std::vector<const char*>& outNames = m_pSession->GetOutputNames();
	if (outNames.size() < 2)
	{
		std::cout <<"ERROR [Anomaly] expected 2 outputs (anomaly_map, score), got "<< outNames.size() << std::endl;
		Close();
		return false;
	}
	m_idxMap = 0;
	m_idxScore = 1;
	for (int i = 0; i < (int)outNames.size(); ++i)
	{
		if (std::string(outNames[i]) == "anomaly_map") m_idxMap = i;
		if (std::string(outNames[i]) == "score")       m_idxScore = i;
	}

	// 4) batch buffer
	const size_t n = (size_t)m_params.maxBatch * 3 * m_inH * m_inW;
	m_pMemInfo = new Ort::MemoryInfo(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));
	m_pBlob = new float[n];
	std::memset(m_pBlob, 0, n * sizeof(float));

	// 5) warmup with the SAME shape as real runs -> cuDNN tuned here, not on the first image
	for (int i = 0; i < m_params.warmup; ++i)
	{
		auto t0 = std::chrono::steady_clock::now();
		std::vector<Ort::Value> outs;
		if (!RunBlob(outs))
		{
			std::cout <<"ERROR [Anomaly] warmup run failed"<<std::endl;
			Close();
			return false;
		}
		std::cout <<"INFO [Anomaly] warmup batch "<< i + 1<<" " << m_params.warmup << " " << m_params.maxBatch << std::endl;
	}
	return true;
}

// ------------------------------------------------------------------ run
bool CAnomaly::RunBlob(std::vector<Ort::Value>& outs)
{
	const std::vector<int64_t> shape = { m_params.maxBatch, 3, m_inH, m_inW };
	Ort::Value input = Ort::Value::CreateTensor<float>(*m_pMemInfo, m_pBlob,
		(size_t)m_params.maxBatch * 3 * m_inH * m_inW,
		shape.data(), shape.size());
	if (!m_pSession->Run(input, outs) || (int)outs.size() <= (std::max)(m_idxMap, m_idxScore))
	{
		Log("ERROR", "[Anomaly] run failed (outputs %zu)", outs.size());
		return false;
	}
	return true;
}

void CAnomaly::FillSlot(int slot, const cv::Mat& bgr)
{
	const size_t plane = (size_t)m_inH * m_inW;
	float* pR = m_pBlob + (size_t)slot * 3 * plane;
	float* pG = pR + plane;
	float* pB = pG + plane;
	if (bgr.empty())
	{
		std::memset(pR, 0, 3 * plane * sizeof(float));     // padding slot
		return;
	}
	cv::Mat img;
	if (bgr.cols != m_inW || bgr.rows != m_inH)
		cv::resize(bgr, img, cv::Size(m_inW, m_inH), 0, 0, cv::INTER_LINEAR);
	else
		img = bgr;
	for (int y = 0; y < m_inH; ++y)
	{
		const uchar* p = img.ptr<uchar>(y);
		const size_t o = (size_t)y * m_inW;
		for (int x = 0; x < m_inW; ++x)
		{
			pB[o + x] = p[3 * x + 0] / 255.f;
			pG[o + x] = p[3 * x + 1] / 255.f;
			pR[o + x] = p[3 * x + 2] / 255.f;
		}
	}
}

bool CAnomaly::Run(const std::vector<cv::Mat>& crops, std::vector<float>& scores, std::vector<cv::Mat>* pMaps)
{
	scores.assign(crops.size(), -1.f);
	if (pMaps) pMaps->assign(crops.size(), cv::Mat());
	m_lastMs = 0.0;
	if (!IsOpen())
	{
		Log("ERROR", "[Anomaly] Run: model not open");
		return false;
	}
	if (crops.empty())
		return true;

	auto t0 = std::chrono::steady_clock::now();
	const int B = m_params.maxBatch;
	int nChunks = 0;

	for (size_t start = 0; start < crops.size(); start += B)
	{
		const int n = (int)(std::min)((size_t)B, crops.size() - start);
		for (int i = 0; i < B; ++i)
		{
			if (i < n)
			{
				const cv::Mat& c = crops[start + i];
				const bool valid = !c.empty() && c.type() == CV_8UC3;
				if (!valid)
					Log("WARN", "[Anomaly] crop %zu empty or not BGR 8UC3 -> score -1", start + i);
				FillSlot(i, valid ? c : cv::Mat());
			}
			else
			{
				FillSlot(i, cv::Mat());                      // pad last chunk to keep the shape fixed
			}
		}

		std::vector<Ort::Value> outs;
		if (!RunBlob(outs))
		{
			Log("ERROR", "[Anomaly] Run failed at crop %zu", start);
			return false;
		}
		const float* pScore = outs[m_idxScore].GetTensorData<float>();
		const float* pMap = outs[m_idxMap].GetTensorData<float>();
		const std::vector<int64_t> ms = GetShape(outs[m_idxMap]);
		for (int i = 0; i < n; ++i)
		{
			const cv::Mat& c = crops[start + i];
			if (c.empty() || c.type() != CV_8UC3)
				continue;
			scores[start + i] = pScore[i];
			if (pMaps && ms.size() == 4)
			{
				const int mh = (int)ms[2], mw = (int)ms[3];
				cv::Mat m(mh, mw, CV_32F, (void*)(pMap + (size_t)i * mh * mw));
				(*pMaps)[start + i] = m.clone();             // own copy (outs freed after this chunk)
			}
		}
		++nChunks;
	}
	m_lastMs = MsSince(t0);

	int nAnom = 0;
	for (float s : scores)
		if (s >= m_params.threshold) ++nAnom;
	Log("INFO", "[Anomaly] %zu crop(s) in %d chunk(s) of %d: %.1f ms (%.2f ms/crop), over threshold %d (th %.3f)",
		crops.size(), nChunks, B, m_lastMs, m_lastMs / crops.size(), nAnom, m_params.threshold);
	return true;
}