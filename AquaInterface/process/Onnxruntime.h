#pragma once
#include <iostream>
#include <string>
#include <stdexcept>
#include <memory>
#include <fstream>
#include <vector>
#include "onnxruntime_cxx_api.h"



#ifdef _MSC_VER
#include <io.h>
#define ACCESS _access
#else
#include <unistd.h>
#include <limits.h>
#define ACCESS access
#endif
#ifndef PATH_MAX
#define PATH_MAX 260
#endif


class COnnxruntime
{
public:
	COnnxruntime();
	~COnnxruntime();
	COnnxruntime(const COnnxruntime&) = delete;
	COnnxruntime& operator=(const COnnxruntime&) = delete;

	/*
	* 2026/11/03 kimju
	* @brief Create ONNX Runtime session
	* @param [IN] stronnxfilePath : ONNX model file path
	* @param [IN] num_thread      : number of threads for inference
	*/
	bool CreateSession(const std::string& stronnxfilePath, int num_thread, Ort::Env* env, bool bUseCuda = true, int nDeviceId = 0);
	std::wstring ConvertToWString(const std::string& strtowstr);

	void ReleaseSession();
	bool IsCreated() const { return m_pOrtSession != NULL; }
	bool IsCuda() const { return m_bCuda; }


	std::vector<const char*>& GetInputNames() { return m_vInputNamePtr; }
	std::vector<const char*>& GetOutputNames() { return m_vOutputNamePtr; }
	/*
	* 2026/11/03
	* @brief input tensor type 및 shape 등 정보 반환
	* @param [IN] nidx    : input index, default 0
	*/
	std::vector<int64_t> GetInputShape(int nidx = 0);
	Ort::TypeInfo GetInputtensorTypeInfo(int nidx = 0);
	/*
	* 2026/11/03
	* @brief run ONNX model inference
	* @param [IN] vInputNames  : input name vector
	* @param [IN] input_tensor : input tensor
	* @param [IN] vOutputNames : output name vector
	*/
	bool Run(Ort::Value& input_tensor, std::vector<Ort::Value>& vOutputs);
private:

	bool CreateSessionWith(Ort::Env* env, const std::wstring& wpath, int num_thread, bool bCuda, int nDeviceId);
	bool ReadNames();
	std::string ShapeStr(const std::vector<int64_t>& shape) const;
	void Log(const char* level, const std::string& msg);

	Ort::Session* m_pOrtSession;   // owned (new in CreateSession, delete in ReleaseSession)
	Ort::AllocatorWithDefaultOptions m_allocator;

	bool        m_bCuda;
	std::string m_strModelPath;
	std::vector<std::string> m_vInputNames;     // name storage
	std::vector<std::string> m_vOutputNames;
	std::vector<const char*> m_vInputNamePtr;   // point into the strings above
	std::vector<const char*> m_vOutputNamePtr;
};