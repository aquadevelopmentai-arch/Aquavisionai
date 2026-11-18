// ShrimpCrop.cpp - per-shrimp crops (process.dll internal), port of prepare_crops.py
#include "pch.h"
#include "ShrimpCrop.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <iostream>

CShrimpCrop::CShrimpCrop()
{
	m_bDebugLog = false;
}

CShrimpCrop::~CShrimpCrop()
{
}

void CShrimpCrop::Log(const char* level, const char* fmt, ...) const
{
	char msg[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf_s(msg, sizeof(msg), _TRUNCATE, fmt, ap);
	va_end(ap);
	std::cout << "[" << level << "] " << msg << std::endl;
}

// ------------------------------------------------------------------ one crop (prepare_crops.make_crop)
cv::Mat CShrimpCrop::MakeCrop(const cv::Mat& roiImg, const cv::Mat& mask) const
{
	const CropParams& cp = m_params;
	const cv::Scalar bg(cp.bgB, cp.bgG, cp.bgR);
	std::vector<cv::Point> nz;
	cv::findNonZero(mask, nz);
	if (nz.empty())
		return cv::Mat();
	const cv::Rect r = cv::boundingRect(nz);
	const int x0 = (std::max)(0, r.x - cp.margin);
	const int y0 = (std::max)(0, r.y - cp.margin);
	const int x1 = (std::min)(roiImg.cols, r.x + r.width + cp.margin);
	const int y1 = (std::min)(roiImg.rows, r.y + r.height + cp.margin);
	const cv::Rect box(x0, y0, x1 - x0, y1 - y0);

	cv::Mat roi = roiImg(box).clone();
	cv::Mat m = mask(box);
	roi.setTo(bg, m == 0);

	// rotate the long axis horizontal
	if (cp.align)
	{
		std::vector<std::vector<cv::Point>> cnts;
		cv::Mat mc = m.clone();
		cv::findContours(mc, cnts, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
		std::vector<cv::Point> all;
		for (const auto& c : cnts) all.insert(all.end(), c.begin(), c.end());
		if (!all.empty())
		{
			const cv::RotatedRect rr = cv::minAreaRect(all);
			double ang = rr.angle;
			if (rr.size.width < rr.size.height)
				ang += 90.0;
			const int w = roi.cols, h = roi.rows;
			cv::Mat M = cv::getRotationMatrix2D(cv::Point2f(w / 2.f, h / 2.f), ang, 1.0);
			const double c = std::abs(M.at<double>(0, 0)), s = std::abs(M.at<double>(0, 1));
			const int nw = (int)(h * s + w * c), nh = (int)(h * c + w * s);
			M.at<double>(0, 2) += nw / 2.0 - w / 2.0;
			M.at<double>(1, 2) += nh / 2.0 - h / 2.0;
			cv::Mat rot, mr;
			cv::warpAffine(roi, rot, M, cv::Size(nw, nh), cv::INTER_LINEAR, cv::BORDER_CONSTANT, bg);
			cv::warpAffine(m, mr, M, cv::Size(nw, nh), cv::INTER_NEAREST, cv::BORDER_CONSTANT, cv::Scalar(0));
			std::vector<cv::Point> nz2;
			cv::findNonZero(mr, nz2);
			roi = nz2.empty() ? rot : rot(cv::boundingRect(nz2)).clone();
		}
	}

	// letterbox (keep aspect, pad with bg)
	const int h = roi.rows, w = roi.cols;
	const double sc = cp.size / (double)(std::max)(h, w);
	const int nw = (std::max)(1, (int)std::lround(w * sc)), nh = (std::max)(1, (int)std::lround(h * sc));
	cv::Mat rs;
	cv::resize(roi, rs, cv::Size(nw, nh), 0, 0, sc < 1.0 ? cv::INTER_AREA : cv::INTER_LINEAR);
	cv::Mat out(cp.size, cp.size, CV_8UC3, bg);
	rs.copyTo(out(cv::Rect((cp.size - nw) / 2, (cp.size - nh) / 2, nw, nh)));
	return out;
}

// ------------------------------------------------------------------ all shrimp of one image
bool CShrimpCrop::Make(const cv::Mat& bgr, SegResult& res, std::vector<cv::Mat>& crops, std::vector<int>& objIdx,
	std::vector<cv::Mat>* pPartial, std::vector<int>* pPartialIdx)
{
	const CropParams& cp = m_params;
	crops.clear();
	objIdx.clear();
	if (pPartial) pPartial->clear();
	if (pPartialIdx) pPartialIdx->clear();
	if (bgr.empty() || bgr.type() != CV_8UC3)
	{
		Log("ERROR", "[Crop] image empty or not BGR 8UC3");
		return false;
	}
	auto t0 = std::chrono::steady_clock::now();
	const int W = bgr.cols, H = bgr.rows;
	const int N = (int)res.objects.size();

	// whole image once (same as python: lab / laplacian of the full image)
	cv::Mat lab, gray, lap;
	cv::cvtColor(bgr, lab, cv::COLOR_BGR2Lab);
	cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
	cv::Laplacian(gray, lap, CV_32F);

	// polygon (or box rectangle) + bounding box per object
	std::vector<std::vector<cv::Point>> polys(N);
	std::vector<cv::Rect> bbs(N);
	for (int i = 0; i < N; ++i)
	{
		const SegObject& o = res.objects[i];
		if (o.polygon.size() >= 3)
			polys[i] = o.polygon;
		else
		{
			const int x1 = (int)std::lround(o.box.x), y1 = (int)std::lround(o.box.y);
			const int x2 = (int)std::lround(o.box.x + o.box.width), y2 = (int)std::lround(o.box.y + o.box.height);
			polys[i] = { {x1, y1}, {x2, y1}, {x2, y2}, {x1, y2} };
		}
		bbs[i] = cv::boundingRect(polys[i]);
	}

	int nJudge = 0, nPartial = 0, nSkip = 0;
	const int pad = cp.margin + cp.erode + 2;
	const cv::Mat kErode = cp.erode > 0
		? cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(2 * cp.erode + 1, 2 * cp.erode + 1)) : cv::Mat();

	for (int i = 0; i < N; ++i)
	{
		SegObject& o = res.objects[i];
		const cv::Rect& b = bbs[i];

		if (cp.minArea > 0.0 && o.area < cp.minArea)
		{
			o.cropClass = CropClass::Skip;
			++nSkip;
			continue;
		}

		// ROI = bbox + pad (inside the image)
		const int rx0 = (std::max)(0, b.x - pad), ry0 = (std::max)(0, b.y - pad);
		const int rx1 = (std::min)(W, b.x + b.width + pad), ry1 = (std::min)(H, b.y + b.height + pad);
		if (rx1 <= rx0 || ry1 <= ry0)
		{
			o.cropClass = CropClass::Skip;
			++nSkip;
			continue;
		}
		const cv::Rect roiR(rx0, ry0, rx1 - rx0, ry1 - ry0);
		const cv::Point off(-rx0, -ry0);

		// own mask minus every overlapping shrimp
		cv::Mat own = cv::Mat::zeros(roiR.size(), CV_8U);
		std::vector<std::vector<cv::Point>> one(1, polys[i]);
		cv::fillPoly(own, one, cv::Scalar(255), cv::LINE_8, 0, off);
		cv::Mat others = cv::Mat::zeros(roiR.size(), CV_8U);
		int nOver = 0;
		for (int j = 0; j < N; ++j)
		{
			if (j == i || (bbs[j] & b).area() <= 0)
				continue;
			std::vector<std::vector<cv::Point>> oj(1, polys[j]);
			cv::fillPoly(others, oj, cv::Scalar(255), cv::LINE_8, 0, off);
			++nOver;
		}
		cv::Mat vis;
		cv::bitwise_and(own, ~others, vis);

		const int ownPx = cv::countNonZero(own);
		const int visPx = cv::countNonZero(vis);
		o.overlaps = nOver;
		o.visibleRatio = (float)visPx / (float)(std::max)(1, ownPx);
		o.onBorder = b.x < cp.border || b.y < cp.border ||
			b.x + b.width - 1 > W - 1 - cp.border || b.y + b.height - 1 > H - 1 - cp.border;
		if (visPx < 10)
		{
			o.cropClass = CropClass::Skip;
			++nSkip;
			continue;
		}

		// color features on the eroded visible mask
		cv::Mat sel = vis;
		if (!kErode.empty())
		{
			cv::Mat e;
			cv::erode(vis, e, kErode);
			if (cv::countNonZero(e) >= 20)
				sel = e;
		}
		const cv::Scalar mLab = cv::mean(lab(roiR), sel);
		cv::Scalar mu, sd;
		cv::meanStdDev(lap(roiR), mu, sd, sel);
		o.labL = (float)mLab[0];
		o.labA = (float)mLab[1];
		o.labB = (float)mLab[2];
		o.texture = (float)(sd[0] * sd[0]);               // variance (numpy var)

		o.cropClass = (o.visibleRatio < cp.minVisible || o.onBorder) ? CropClass::Partial : CropClass::Judge;

		if (o.cropClass == CropClass::Judge || pPartial != NULL)
		{
			cv::Mat crop = MakeCrop(bgr(roiR), vis);
			if (crop.empty())
			{
				o.cropClass = CropClass::Skip;
				++nSkip;
				continue;
			}
			if (o.cropClass == CropClass::Judge)
			{
				crops.push_back(crop);
				objIdx.push_back(i);
			}
			else
			{
				pPartial->push_back(crop);
				if (pPartialIdx) pPartialIdx->push_back(i);
			}
		}
		if (o.cropClass == CropClass::Judge) ++nJudge; else ++nPartial;

		if (m_bDebugLog)
			Log("DEBUG", "[Crop] #%d %s vis=%.2f border=%d overlaps=%d L=%.1f a=%.1f b=%.1f tex=%.1f",
				o.id, o.cropClass == CropClass::Judge ? "judge" : "partial", o.visibleRatio, (int)o.onBorder,
				nOver, o.labL, o.labA, o.labB, o.texture);
	}

	res.cropMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	Log("INFO", "[Crop] %d object(s): judge %d / partial %d / skip %d  (%.1f ms)", N, nJudge, nPartial, nSkip, res.cropMs);
	return true;
}