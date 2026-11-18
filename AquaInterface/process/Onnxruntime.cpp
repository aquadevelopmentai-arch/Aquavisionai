#include "pch.h"
#include "Onnxruntime.h"
 
#include <windows.h>
#include <chrono>
 
COnnxruntime::COnnxruntime()
{
	m_pOrtSession = NULL;
	m_bCuda = false;
}
 
COnnxruntime::~COnnxruntime()
{
	ReleaseSession();
}
 
// ------------------------------------------------------------------ log (cout + logs\ort_yyyymmdd.log)
void COnnxruntime::Log(const char* level, const std::string& msg)
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	char head[64];
	sprintf_s(head, "[%04d-%02d-%02d %02d:%02d:%02d.%03d][%-5s][ORT] ", st.wYear, st.wMonth, st.wDay,
	          st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, level);
	std::cout << head << msg << std::endl;
 
	CreateDirectoryA("logs", NULL);
	char path[PATH_MAX];
	sprintf_s(path, "logs\\ort_%04d%02d%02d.log", st.wYear, st.wMonth, st.wDay);
	std::ofstream f(path, std::ios::app);
	if (f.is_open())
		f << head << msg << "\n";
}
 
std::string COnnxruntime::ShapeStr(const std::vector<int64_t>& shape) const
{
	std::string r = "[";
	for (size_t i = 0; i < shape.size(); ++i)
		r += std::to_string(shape[i]) + (i + 1 < shape.size() ? "," : "");
	return r + "]";
}
 
// CP_ACP (MFC CStringA) -> wide. 한글 경로 OK
std::wstring COnnxruntime::ConvertToWString(const std::string& strtowstr)
{
	if (strtowstr.empty())
		return std::wstring();
	const int n = MultiByteToWideChar(CP_ACP, 0, strtowstr.c_str(), -1, NULL, 0);
	std::wstring w(n > 0 ? n - 1 : 0, L'\0');
	if (n > 0)
		MultiByteToWideChar(CP_ACP, 0, strtowstr.c_str(), -1, &w[0], n);
	return w;
}
 
// ------------------------------------------------------------------ create
bool COnnxruntime::CreateSession(const std::string& stronnxfilePath, int num_thread, Ort::Env* env,
                                 bool bUseCuda, int nDeviceId)
{
	if (m_pOrtSession != NULL)
	{
		Log("WARN", "CreateSession: already created -> ReleaseSession() first");
		return false;
	}
	if (env == NULL)
	{
		Log("ERROR", "CreateSession: Env is NULL (CProcesser::Init not called?)");
		return false;
	}
 
	// full path + exists
	char resolved_path[PATH_MAX];
	if (_fullpath(resolved_path, stronnxfilePath.c_str(), PATH_MAX) == NULL)
	{
		Log("ERROR", "CreateSession: path resolution failed: " + stronnxfilePath);
		return false;
	}
	if (ACCESS(resolved_path, 0) != 0)
	{
		Log("ERROR", std::string("CreateSession: model not found: ") + resolved_path);
		return false;
	}
	m_strModelPath = resolved_path;
	const std::wstring wpath = ConvertToWString(m_strModelPath);
	auto t0 = std::chrono::steady_clock::now();
 
	// CUDA -> CPU
	bool ok = false;
	if (bUseCuda)
	{
		ok = CreateSessionWith(env, wpath, num_thread, true, nDeviceId);
		if (!ok)
			Log("WARN", "CUDA failed -> CPU. (CUDA 13 / cuDNN 9 DLL in PATH? onnxruntime GPU build?)");
	}
	if (!ok)
		ok = CreateSessionWith(env, wpath, num_thread, false, nDeviceId);
	if (!ok)
	{
		ReleaseSession();
		return false;
	}
	if (!ReadNames())
	{
		ReleaseSession();
		return false;
	}
 
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	char buf[64];
	sprintf_s(buf, " (%.0f ms)", ms);
	Log("INFO", std::string("CreateSession OK: ") + m_strModelPath + " provider=" + (m_bCuda ? "CUDA" : "CPU") + buf);
	if (bUseCuda && !m_bCuda)
		Log("WARN", "running on CPU (CUDA requested but not available)");
	return true;
}
 
bool COnnxruntime::CreateSessionWith(Ort::Env* env, const std::wstring& wpath, int num_thread, bool bCuda, int nDeviceId)
{
	OrtCUDAProviderOptionsV2* pCudaOpt = NULL;
	const OrtApi& api = Ort::GetApi();
	try
	{
		Ort::SessionOptions session_options;
		session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
		session_options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
		if (num_thread > 0)
			session_options.SetIntraOpNumThreads(num_thread);
		session_options.SetInterOpNumThreads(1);
		session_options.EnableMemPattern();
		session_options.EnableCpuMemArena();
		session_options.DisableProfiling();

		if (bCuda)
		{
			// V2 옵션 (구 구조체로는 cuDNN 작업공간을 못 바꿈)
			Ort::ThrowOnError(api.CreateCUDAProviderOptions(&pCudaOpt));
			const std::string dev = std::to_string(nDeviceId);
			const char* keys[] = {
				"device_id",
				"cudnn_conv_algo_search",          // HEURISTIC : 첫 추론 빠르게
				"cudnn_conv_use_max_workspace",    // 0 : cuDNN 작업공간 최소 -> GPU 메모리 크게 줄어듦
				"arena_extend_strategy",           // kSameAsRequested : 필요한 만큼만 (2배씩 X)
				"do_copy_in_default_stream"
			};
			const char* vals[] = { dev.c_str(), "HEURISTIC", "0", "kSameAsRequested", "1" };
			Ort::ThrowOnError(api.UpdateCUDAProviderOptions(pCudaOpt, keys, vals, 5));
			session_options.AppendExecutionProvider_CUDA_V2(*pCudaOpt);
			api.ReleaseCUDAProviderOptions(pCudaOpt);        // 세션 옵션에 복사됨 -> 바로 해제
			pCudaOpt = NULL;
		}

		Log("INFO", std::string("CreateSession try provider=") + (bCuda ? "CUDA" : "CPU")
			+ " device=" + std::to_string(nDeviceId) + " threads=" + std::to_string(num_thread)
			+ (bCuda ? " (workspace min, arena same-as-requested)" : ""));
		m_pOrtSession = new Ort::Session(*env, wpath.c_str(), session_options);
		m_bCuda = bCuda;
		return true;
	}
	catch (const Ort::Exception& e)
	{
		Log(bCuda ? "WARN" : "ERROR", std::string("CreateSession ") + (bCuda ? "CUDA" : "CPU") + " failed: " + e.what());
		if (pCudaOpt != NULL)
		{
			api.ReleaseCUDAProviderOptions(pCudaOpt);
			pCudaOpt = NULL;
		}
		if (m_pOrtSession != NULL)
		{
			delete m_pOrtSession;
			m_pOrtSession = NULL;
		}
		m_bCuda = false;
		return false;
	}
	catch (const std::exception& e)
	{
		Log("ERROR", std::string("CreateSession ") + (bCuda ? "CUDA" : "CPU") + " failed (std): " + e.what());
		if (pCudaOpt != NULL)
		{
			api.ReleaseCUDAProviderOptions(pCudaOpt);
			pCudaOpt = NULL;
		}
		m_pOrtSession = NULL;
		m_bCuda = false;
		return false;
	}
	return true;
}
 
// names copied ONCE (AllocatedStringPtr frees its memory when it goes out of scope)
bool COnnxruntime::ReadNames()
{
	try
	{
		for (size_t i = 0; i < m_pOrtSession->GetInputCount(); ++i)
		{
			m_vInputNames.push_back(m_pOrtSession->GetInputNameAllocated(i, m_allocator).get());
			Log("INFO", "  input  " + std::to_string(i) + ": " + m_vInputNames.back() + " " + ShapeStr(GetInputShape((int)i)));
		}
		for (size_t i = 0; i < m_pOrtSession->GetOutputCount(); ++i)
		{
			m_vOutputNames.push_back(m_pOrtSession->GetOutputNameAllocated(i, m_allocator).get());
			Ort::TypeInfo ti = m_pOrtSession->GetOutputTypeInfo(i);
			Log("INFO", "  output " + std::to_string(i) + ": " + m_vOutputNames.back() + " "
			            + ShapeStr(ti.GetTensorTypeAndShapeInfo().GetShape()));
		}
		// pointers AFTER every push_back (vector growth would move the strings)
		for (size_t i = 0; i < m_vInputNames.size(); ++i)  m_vInputNamePtr.push_back(m_vInputNames[i].c_str());
		for (size_t i = 0; i < m_vOutputNames.size(); ++i) m_vOutputNamePtr.push_back(m_vOutputNames[i].c_str());
	}
	catch (const Ort::Exception& e)
	{
		Log("ERROR", std::string("CreateSession: reading model info failed: ") + e.what());
		return false;
	}
	return true;
}
 
// ------------------------------------------------------------------ release
void COnnxruntime::ReleaseSession()
{
	if (m_pOrtSession != NULL)
	{
		delete m_pOrtSession;
		m_pOrtSession = NULL;
		Log("INFO", "ReleaseSession: " + m_strModelPath);
	}
	m_vInputNamePtr.clear();
	m_vOutputNamePtr.clear();
	m_vInputNames.clear();
	m_vOutputNames.clear();
	m_strModelPath.clear();
	m_bCuda = false;
}
 
// ------------------------------------------------------------------ info
std::vector<int64_t> COnnxruntime::GetInputShape(int nidx)
{
	if (m_pOrtSession == NULL)
		return std::vector<int64_t>();
	Ort::TypeInfo ti = m_pOrtSession->GetInputTypeInfo(nidx);
	return ti.GetTensorTypeAndShapeInfo().GetShape();
}
 
Ort::TypeInfo COnnxruntime::GetInputtensorTypeInfo(int nidx)
{
	if (m_pOrtSession == NULL)
		throw std::runtime_error("GetInputtensorTypeInfo: session not created");
	return m_pOrtSession->GetInputTypeInfo(nidx);
}
 
// ------------------------------------------------------------------ run
bool COnnxruntime::Run(Ort::Value& input_tensor, std::vector<Ort::Value>& vOutputs)
{
	vOutputs.clear();
	if (m_pOrtSession == NULL)
	{
		Log("ERROR", "Run: session not created");
		return false;
	}
	try
	{
		vOutputs = m_pOrtSession->Run(Ort::RunOptions{ nullptr },
		                              m_vInputNamePtr.data(), &input_tensor, 1,
		                              m_vOutputNamePtr.data(), m_vOutputNamePtr.size());
	}
	catch (const Ort::Exception& e)
	{
		Log("ERROR", std::string("Run failed: ") + e.what());
		vOutputs.clear();
		return false;
	}
	return true;
}