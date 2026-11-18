#pragma once
// ShrimpCrop.h - per-shrimp crops for the anomaly model (process.dll internal, NOT exported)
//   same steps as prepare_crops.py / count_ort.py --crops (training and inference crops made the same way):
//     1) visible region = own polygon MINUS every overlapping shrimp polygon   (raster masks, inside the ROI only)
//     2) visible ratio = visible px / own px, on-border check
//     3) class : Skip (visible < 10 px) / Partial (ratio < minVisible or on border) / Judge
//     4) color features on the eroded visible mask : Lab mean, Laplacian variance (texture)
//     5) crop : background outside the visible mask, rotate long axis horizontal, letterbox to size x size
//   writes cropClass / visibleRatio / onBorder / overlaps / labL,A,B / texture into each SegObject
//
//   usage (Cprocesser):
//     m_pCrop = new CShrimpCrop();
//     m_pCrop->SetParams(cp);                         // CropParams defaults = prepare_crops.py defaults
//     m_pCrop->Make(bgr, seg, crops, objIdx);         // crops[i] -> seg.objects[objIdx[i]]
//     delete m_pCrop;  m_pCrop = NULL;

#include "YoloSegTypes.h"

class CShrimpCrop
{
public:
	CShrimpCrop();
	~CShrimpCrop();

	void SetParams(const CropParams& cp) { m_params = cp; }
	const CropParams& GetParams() const { return m_params; }

	/*
	* @brief crops for every judged shrimp of one image
	* @param [IN]     bgr         : original image (BGR 8UC3), same one given to YOLO-seg
	* @param [IN/OUT] res         : YOLO-seg result. cropClass / visibleRatio / color features are written
	* @param [OUT]    crops       : JUDGE crops only (size x size BGR)
	* @param [OUT]    objIdx      : crops[i] belongs to res.objects[objIdx[i]]
	* @param [OUT]    pPartial    : optional, PARTIAL crops too (review / saving)
	* @param [OUT]    pPartialIdx : optional, index for pPartial
	*/
	bool Make(const cv::Mat& bgr, SegResult& res, std::vector<cv::Mat>& crops, std::vector<int>& objIdx,
		std::vector<cv::Mat>* pPartial = NULL, std::vector<int>* pPartialIdx = NULL);
	cv::Mat MakeCrop(const cv::Mat& roiImg, const cv::Mat& mask) const;
	void Log(const char* level, const char* fmt, ...) const;     // std::cout

private:

	CropParams m_params;
	bool       m_bDebugLog;      // per-shrimp log (many lines) -> false by default
};