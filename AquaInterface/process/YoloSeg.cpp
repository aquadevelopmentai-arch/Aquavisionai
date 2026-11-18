#include "pch.h"
#include "YoloSeg.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>          // NMSBoxes
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <iostream>

CYoloSeg::CYoloSeg()
{
	m_pOnnxruntime = NULL;
	m_pMemInfo = NULL;
	m_pBlob = NULL;
	m_pCanvas = NULL;
	m_inW = 0;                     // set in CreateYoloSeg (from the ONNX input or params.inputSize)
	m_inH = 0;
	m_bSeg = false;
}

CYoloSeg::~CYoloSeg()
{
	ReleaseYoloSeg();
}

void CYoloSeg::ReleaseYoloSeg()
{
	if (m_pOnnxruntime)
	{
		delete m_pOnnxruntime;
		m_pOnnxruntime = NULL;
	}
	if (m_pMemInfo)
	{
		delete m_pMemInfo;
		m_pMemInfo = NULL;
	}
	if (m_pBlob)
	{
		delete[] m_pBlob;
		m_pBlob = NULL;
	}
	if (m_pCanvas)
	{
		delete m_pCanvas;
		m_pCanvas = NULL;
	}
	m_bSeg = false;
}

bool CYoloSeg::CreateYoloSeg(std::string weight_path, int num_thread, Ort::Env* env)
{
	if (m_pOnnxruntime != NULL)
	{
		Log("WARN", "[YoloSeg] already created");
		return false;
	}
	if (env == NULL)
	{
		Log("ERROR", "[YoloSeg] Ort::Env is NULL (CProcesser::Init first)");
		return false;
	}
	m_pOnnxruntime = new COnnxruntime();
	try
	{
		if (!m_pOnnxruntime->CreateSession(weight_path, num_thread, env, m_params.useCuda, m_params.deviceId))
		{
			Log("ERROR", "[YoloSeg] CreateSession failed: %s", weight_path.c_str());
			ReleaseYoloSeg();                       // no half-created object left behind
			return false;
		}
	}
	catch (const std::exception& e)
	{
		Log("ERROR", "[YoloSeg] Error creating ONNX Runtime session: %s", e.what());
		ReleaseYoloSeg();
		return false;
	}

	// input [1,3,H,W]
	const std::vector<int64_t> inShape = m_pOnnxruntime->GetInputShape(0);
	if (inShape.size() != 4)
	{
		Log("ERROR", "[YoloSeg] unexpected input rank %zu (need [1,3,H,W])", inShape.size());
		ReleaseYoloSeg();
		return false;
	}
	if (inShape[2] > 0 && inShape[3] > 0)
	{
		// fixed-shape export (imgsz baked into the ONNX) -> always use the model size
		m_inH = (int)inShape[2];
		m_inW = (int)inShape[3];
		if (m_params.inputSize > 0 && (m_params.inputSize != m_inW || m_params.inputSize != m_inH))
			Log("WARN", "[YoloSeg] params.inputSize %d ignored: ONNX input is fixed %dx%d",
				m_params.inputSize, m_inW, m_inH);
	}
	else
	{
		// dynamic export -> size must come from params (same as training imgsz)
		if (m_params.inputSize <= 0 || m_params.inputSize % 32 != 0)
		{
			Log("ERROR", "[YoloSeg] ONNX input is dynamic -> set params.inputSize (multiple of 32, e.g. 1568), now %d",
				m_params.inputSize);
			ReleaseYoloSeg();
			return false;
		}
		m_inH = m_params.inputSize;
		m_inW = m_params.inputSize;
	}
	m_bSeg = m_pOnnxruntime->GetOutputNames().size() >= 2;      // output0 + protos

	m_pMemInfo = new Ort::MemoryInfo(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));
	m_pBlob = new float[(size_t)3 * m_inH * m_inW];
	m_pCanvas = new cv::Mat(m_inH, m_inW, CV_8UC3, cv::Scalar(114, 114, 114));

	Log("INFO", "[YoloSeg] created %s  input %dx%d (WxH)  outputs %zu -> %s",
		weight_path.c_str(), m_inW, m_inH, m_pOnnxruntime->GetOutputNames().size(),
		m_bSeg ? "SEGMENTATION" : "DETECTION (boxes only)");
	Log("INFO", "[YoloSeg] params conf=%.2f iou=%.2f maxDet=%d maskTh=%.2f useMask=%d",
		m_params.confTh, m_params.iouTh, m_params.maxDet, m_params.maskTh, (int)m_params.useMask);
	return true;
}

void CYoloSeg::Warmup(int nCount)
{
	if (!IsCreated() || nCount <= 0)
		return;
	std::fill(m_pBlob, m_pBlob + (size_t)3 * m_inH * m_inW, 0.f);
	const std::vector<int64_t> shape = { 1, 3, m_inH, m_inW };
	for (int i = 0; i < nCount; ++i)
	{
		auto t0 = std::chrono::steady_clock::now();
		Ort::Value input = Ort::Value::CreateTensor<float>(*m_pMemInfo, m_pBlob, (size_t)3 * m_inH * m_inW,
			shape.data(), shape.size());
		std::vector<Ort::Value> outs;
		const bool ok = m_pOnnxruntime->Run(input, outs);
		Log("INFO", "[YoloSeg] warmup %d/%d %s %.1f ms", i + 1, nCount, ok ? "ok" : "FAIL", MsSince(t0));
		if (!ok)
			break;
	}
}

// [YoloSeg] log -> std::cout
void CYoloSeg::Log(const char* level, const char* fmt, ...) const
{
	char msg[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf_s(msg, sizeof(msg), _TRUNCATE, fmt, ap);
	va_end(ap);
	std::cout << "[" << level << "] " << msg << std::endl;
}

double CYoloSeg::MsSince(const std::chrono::steady_clock::time_point& t0) const
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

std::vector<int64_t> CYoloSeg::GetShape(const Ort::Value& v) const
{
	return v.GetTensorTypeAndShapeInfo().GetShape();
}

// ------------------------------------------------------------------ 1) letterbox + HWC BGR -> CHW RGB /255
void CYoloSeg::Preprocess(const cv::Mat& bgr, Letterbox& lb)
{
	const int w = bgr.cols, h = bgr.rows;
	lb.r = (std::min)(m_inW / (float)w, m_inH / (float)h);
	const int nw = (int)std::lround(w * lb.r), nh = (int)std::lround(h * lb.r);
	lb.px = (m_inW - nw) / 2;
	lb.py = (m_inH - nh) / 2;

	m_pCanvas->setTo(cv::Scalar(114, 114, 114));
	cv::Mat roi = (*m_pCanvas)(cv::Rect(lb.px, lb.py, nw, nh));
	cv::resize(bgr, roi, cv::Size(nw, nh), 0, 0, lb.r > 1.f ? cv::INTER_LINEAR : cv::INTER_AREA);

	const size_t plane = (size_t)m_inH * m_inW;
	float* pR = m_pBlob;
	float* pG = m_pBlob + plane;
	float* pB = m_pBlob + plane * 2;
	for (int y = 0; y < m_inH; ++y)
	{
		const uchar* p = m_pCanvas->ptr<uchar>(y);
		const size_t o = (size_t)y * m_inW;
		for (int x = 0; x < m_inW; ++x)
		{
			pB[o + x] = p[3 * x + 0] / 255.f;
			pG[o + x] = p[3 * x + 1] / 255.f;
			pR[o + x] = p[3 * x + 2] / 255.f;
		}
	}
}

// ------------------------------------------------------------------ Run
bool CYoloSeg::Run(const cv::Mat& bgr, SegResult& out)
{
	out.Clear();
	if (!IsCreated())
	{
		out.error = "yolo-seg not created";
		Log("ERROR", "[YoloSeg] Run: %s", out.error.c_str());
		return false;
	}
	if (bgr.empty() || bgr.type() != CV_8UC3)
	{
		out.error = "image empty or not BGR 8UC3";
		Log("ERROR", "[YoloSeg] Run: %s", out.error.c_str());
		return false;
	}
	out.imgW = bgr.cols;
	out.imgH = bgr.rows;
	out.cuda = m_pOnnxruntime->IsCuda();          // COnnxruntime에 IsCuda()가 없으면 이 줄 삭제

	// 1) preprocess
	auto t0 = std::chrono::steady_clock::now();
	Letterbox lb;
	Preprocess(bgr, lb);
	out.preMs = MsSince(t0);

	// 2) inference
	t0 = std::chrono::steady_clock::now();
	const std::vector<int64_t> inShape = { 1, 3, m_inH, m_inW };
	Ort::Value input = Ort::Value::CreateTensor<float>(*m_pMemInfo, m_pBlob, (size_t)3 * m_inH * m_inW,
		inShape.data(), inShape.size());
	std::vector<Ort::Value> outs;
	try
	{
		if (!m_pOnnxruntime->Run(input, outs) || outs.empty())
		{
			out.error = "inference failed";
			Log("ERROR", "[YoloSeg] Run: inference failed");
			return false;
		}
	}
	catch (const std::exception& e)
	{
		out.error = std::string("inference exception: ") + e.what();
		Log("ERROR", "[YoloSeg] Run: %s", out.error.c_str());
		return false;
	}
	out.inferMs = MsSince(t0);

	// 3) postprocess
	t0 = std::chrono::steady_clock::now();
	const std::vector<int64_t> s0 = GetShape(outs[0]);
	std::vector<int64_t> sp;
	int nmIn0 = 0;                                 // mask coefficients inside output0 (seg model)
	if (outs.size() >= 2)
	{
		sp = GetShape(outs[1]);                     // protos [1, nm, mh, mw]
		if (sp.size() == 4)
			nmIn0 = (int)sp[1];
	}
	const bool useMask = m_params.useMask && m_bSeg && sp.size() == 4;
	Postprocess(outs[0].GetTensorData<float>(), s0, useMask ? outs[1].GetTensorData<float>() : nullptr, sp, nmIn0, lb, bgr.cols, bgr.rows, out);
	out.postMs = MsSince(t0);
	out.ok = true;

	int nPoly = 0;
	for (const SegObject& o : out.objects)
		if (o.hasMask) ++nPoly;
	Log("INFO", "[YoloSeg] %dx%d -> %d object(s) (polygon %d / box %d)  pre %.1f infer %.1f post %.1f ms %s",
		bgr.cols, bgr.rows, out.Count(), nPoly, out.Count() - nPoly, out.preMs, out.inferMs, out.postMs,
		out.cuda ? "GPU" : "CPU");
	return true;
}

// ------------------------------------------------------------------ decode -> NMS -> mask polygon
void CYoloSeg::Postprocess(const float* out0, const std::vector<int64_t>& s0,
	const float* protos, const std::vector<int64_t>& sp, int nmIn0,
	const Letterbox& lb, int W, int H, SegResult& out)
{
	if (s0.size() != 3)
	{
		Log("ERROR", "[YoloSeg] output0 rank %zu (need 3)", s0.size());
		return;
	}
	const int nm = (protos != nullptr) ? nmIn0 : 0;     // coefficients used for masks
	const int d1 = (int)s0[1], d2 = (int)s0[2];

	std::vector<cv::Rect2d>   boxes;      // letterboxed input coords
	std::vector<float>        scores;
	std::vector<const float*> coefPtr;    // end-to-end: row pointer
	std::vector<int>          coefIdx;    // raw: column index
	std::vector<int>          keep;

	// YOLO26 end-to-end : [1, N, 6+nm] (x1,y1,x2,y2,score,cls,coef...)  /  raw : [1, 4+nc+nm, N]
	const bool e2e = (d2 == 6 + nmIn0) && (d1 != 6 + nmIn0);
	if (e2e)
	{
		const int N = d1, C = d2;
		for (int i = 0; i < N; ++i)
		{
			const float* r = out0 + (size_t)i * C;
			if (r[4] < m_params.confTh) continue;
			boxes.emplace_back(r[0], r[1], r[2] - r[0], r[3] - r[1]);
			scores.push_back(r[4]);
			coefPtr.push_back(r + 6);
			if ((int)boxes.size() >= m_params.maxDet) break;
		}
		for (int i = 0; i < (int)boxes.size(); ++i) keep.push_back(i);
		Log("DEBUG", "[YoloSeg] end-to-end [%d,%d]: %zu over conf %.2f", N, C, boxes.size(), m_params.confTh);
	}
	else
	{
		const int C = d1, N = d2;
		const int nc = C - 4 - nmIn0;
		if (nc < 1)
		{
			Log("ERROR", "[YoloSeg] output0 [%d,%d] does not match nm=%d", C, N, nmIn0);
			return;
		}
		for (int i = 0; i < N; ++i)
		{
			float best = 0.f;
			for (int c = 0; c < nc; ++c)
			{
				const float v = out0[(size_t)(4 + c) * N + i];
				if (v > best) best = v;
			}
			if (best < m_params.confTh) continue;
			const float cx = out0[i], cy = out0[(size_t)N + i];
			const float bw = out0[(size_t)2 * N + i], bh = out0[(size_t)3 * N + i];
			boxes.emplace_back(cx - bw / 2, cy - bh / 2, bw, bh);
			scores.push_back(best);
			coefIdx.push_back(i);
		}
		if (!boxes.empty())
			cv::dnn::NMSBoxes(boxes, scores, m_params.confTh, m_params.iouTh, keep, 1.f, m_params.maxDet);
		if ((int)keep.size() > m_params.maxDet) keep.resize(m_params.maxDet);
		Log("DEBUG", "[YoloSeg] raw [%d,%d] nc=%d: %zu over conf -> %zu after NMS (iou %.2f)",
			C, N, nc, boxes.size(), keep.size(), m_params.iouTh);
	}

	int mh = 0, mw = 0;
	float sx = 0.f, sy = 0.f;
	if (protos != nullptr)
	{
		mh = (int)sp[2];
		mw = (int)sp[3];
		sx = mw / (float)m_inW;
		sy = mh / (float)m_inH;
	}
	const size_t mplane = (size_t)mh * mw;
	std::vector<float> coef(nm > 0 ? nm : 1);

	out.objects.reserve(keep.size());
	for (int k : keep)
	{
		const cv::Rect2d& b = boxes[k];
		const double bx1 = b.x, by1 = b.y, bx2 = b.x + b.width, by2 = b.y + b.height;
		// letterbox -> original
		const float x1 = (float)(std::max)(0.0, (std::min)((double)W - 1, (bx1 - lb.px) / lb.r));
		const float y1 = (float)(std::max)(0.0, (std::min)((double)H - 1, (by1 - lb.py) / lb.r));
		const float x2 = (float)(std::max)(0.0, (std::min)((double)W - 1, (bx2 - lb.px) / lb.r));
		const float y2 = (float)(std::max)(0.0, (std::min)((double)H - 1, (by2 - lb.py) / lb.r));
		if (x2 - x1 < 1.f || y2 - y1 < 1.f) continue;

		SegObject o;
		o.id = (int)out.objects.size() + 1;
		o.box = cv::Rect2f(x1, y1, x2 - x1, y2 - y1);
		o.score = scores[k];
		o.area = (double)o.box.width * o.box.height;

		if (protos != nullptr && nm > 0)
		{
			if (e2e) for (int j = 0; j < nm; ++j) coef[j] = coefPtr[k][j];
			else     for (int j = 0; j < nm; ++j) coef[j] = out0[(size_t)(d1 - nmIn0 + j) * d2 + coefIdx[k]];

			// mask = sigmoid(coef . protos), only inside the predicted box
			const int mx1 = (std::max)(0, (int)std::floor(bx1 * sx)), my1 = (std::max)(0, (int)std::floor(by1 * sy));
			const int mx2 = (std::min)(mw, (int)std::ceil(bx2 * sx)), my2 = (std::min)(mh, (int)std::ceil(by2 * sy));
			if (mx2 > mx1 && my2 > my1)
			{
				cv::Mat m(my2 - my1, mx2 - mx1, CV_32F);
				for (int y = my1; y < my2; ++y)
				{
					float* dst = m.ptr<float>(y - my1);
					for (int x = mx1; x < mx2; ++x)
					{
						const size_t off = (size_t)y * mw + x;
						float v = 0.f;
						for (int j = 0; j < nm; ++j) v += coef[j] * protos[(size_t)j * mplane + off];
						dst[x - mx1] = 1.f / (1.f + std::exp(-v));
					}
				}
				// proto box -> original coords
				const double ox1 = (mx1 / sx - lb.px) / lb.r, oy1 = (my1 / sy - lb.py) / lb.r;
				const double ox2 = (mx2 / sx - lb.px) / lb.r, oy2 = (my2 / sy - lb.py) / lb.r;
				const int tw = (std::max)(1, (int)std::lround(ox2 - ox1)), th = (std::max)(1, (int)std::lround(oy2 - oy1));
				cv::Mat up, mb;
				cv::resize(m, up, cv::Size(tw, th), 0, 0, cv::INTER_LINEAR);
				cv::threshold(up, mb, m_params.maskTh, 255, cv::THRESH_BINARY);
				mb.convertTo(mb, CV_8U);

				const int ix0 = (int)std::lround(ox1), iy0 = (int)std::lround(oy1);
				cv::Rect boxInMask((int)std::lround(x1 - ix0), (int)std::lround(y1 - iy0),
					(int)std::lround(x2 - x1) + 1, (int)std::lround(y2 - y1) + 1);
				boxInMask &= cv::Rect(0, 0, mb.cols, mb.rows);
				cv::Mat clip = cv::Mat::zeros(mb.size(), CV_8U);
				if (boxInMask.area() > 0) mb(boxInMask).copyTo(clip(boxInMask));

				// biggest contour = polygon
				std::vector<std::vector<cv::Point>> cnts;
				cv::findContours(clip, cnts, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
				int best = -1;
				double bestA = 0.0;
				for (int c = 0; c < (int)cnts.size(); ++c)
				{
					const double a = cv::contourArea(cnts[c]);
					if (a > bestA) { bestA = a; best = c; }
				}
				if (best >= 0 && bestA >= m_params.minPolyArea)
				{
					o.polygon = cnts[best];
					for (cv::Point& p : o.polygon)
					{
						p.x = (std::max)(0, (std::min)(W - 1, p.x + ix0));
						p.y = (std::max)(0, (std::min)(H - 1, p.y + iy0));
					}
					o.area = bestA;
					o.hasMask = true;
				}
			}
		}
		out.objects.push_back(o);
	}
}

