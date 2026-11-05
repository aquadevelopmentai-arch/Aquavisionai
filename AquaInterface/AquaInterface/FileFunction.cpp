#include "pch.h"
#include "FileFunction.h"
#include <io.h>

bool PathExists(const std::string& path)
{
#ifdef _MSC_VER
	struct _stat buffer;
	return (_stat(path.c_str(), &buffer) == 0);
#else
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
#endif // !_WIN32
}

bool FileExists(const std::string& file)
{
#ifdef _MSC_VER
	return (_access(file.c_str(), 0) != -1);
#else
	return (access(file.c_str(), 0) != -1);
#endif // !_WIN32
}

std::string GetSelectFolderPath(HWND hOwner, bool bAllowNewFolder, std::string strDefPath)
{
	std::string strFolderPath;

	ITEMIDLIST* pidlBrowse;
	TCHAR       pszPathname[MAX_PATH] = { 0, };
	BROWSEINFO  BrInfo;
	memset(&BrInfo, 0, sizeof(BrInfo));

	BrInfo.pidlRoot = NULL;


	DWORD length = MAX_PATH;
	char buffer[MAX_PATH] = "D:\\ProjectFolder\\MagicReader";

	// GetCurrentDirectoryA(length, buffer);

	LPSHELLFOLDER p_desktop = NULL;
	HRESULT hr = SHGetDesktopFolder(&p_desktop);
	LPITEMIDLIST p_pidl = NULL;

	if (!FAILED(hr))
	{
		OLECHAR ole_filepath[MAX_PATH];
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, buffer, -1, ole_filepath, MAX_PATH);
		hr = p_desktop->ParseDisplayName(NULL, NULL, ole_filepath, NULL, &p_pidl, NULL);
		if (FAILED(hr))
		{
			BrInfo.pidlRoot = NULL;
		}
		else
		{
			BrInfo.pidlRoot = p_pidl;
		}
	}
	else
	{
		BrInfo.pidlRoot = NULL;
	}

	BrInfo.hwndOwner = hOwner;// GetSafeHwnd();

	BrInfo.pszDisplayName = pszPathname;
	BrInfo.lpszTitle = _T("Select Directory");

	if (bAllowNewFolder)
	{
		BrInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	}
	else
	{
		BrInfo.ulFlags = BIF_RETURNONLYFSDIRS;
	}

	if (strDefPath.size() > 0)
	{
		BrInfo.lpfn = BrowseCallbackProc;
		BrInfo.lParam = (LPARAM)strDefPath.c_str();
	}

	pidlBrowse = ::SHBrowseForFolder(&BrInfo);
	if (pidlBrowse != NULL)
	{
		SHGetPathFromIDList(pidlBrowse, pszPathname);
		//strFolderPath.Format(_T("%s"), pszPathname);
		strFolderPath = CStringA(pszPathname).GetBuffer();

		return strFolderPath;
	}
	else  //click cancel
	{
		return strDefPath;
	}

}


std::string AttachSeparator(std::string str)
{
	if (str.size() <= 0) return str;

	if (str[str.size() - 1] != '\\')
	{
		str += "\\";
	}

	return str;
}


void GetFilesInPath(std::string path, std::vector<std::string>& list, std::string filter /*= _T("*.*")*/, bool bShowErr /*= false*/)
{
	std::string strFindPath = AttachSeparator(path);
	strFindPath += filter;

	CFileFind ff;
	BOOL bFound = ff.FindFile(CString(strFindPath.c_str()));
	while (bFound)
	{
		BOOL bExistNext = ff.FindNextFile();

		if (!ff.IsDirectory() && !ff.IsDots())
		{
			//AfxMessageBox(ff.GetFilePath());
			list.push_back(CStringA(ff.GetFilePath()).GetBuffer());
		}
		if (!bExistNext) break;
	}
	if (!bFound)
	{
		if (bShowErr) AfxMessageBox(_T("원본파일 경로를 확인하십시오."));
	}
}

int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	switch (uMsg) {
		// 폴더선택 다이얼로그의 초기화가 끝난 경우
	case BFFM_INITIALIZED:
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
		break;
	}
	return 0;
}

int OpenImageFile(std::string filename, cv::Mat& image_matrix, std::vector<cv::Mat>& images_matrix)
{
	int page = 0;
	images_matrix.clear();

	bool bSuc = imreadmulti(filename, images_matrix, 1);
	page = images_matrix.size();
	
	if (bSuc == false || page < 1)
	{
		image_matrix = cv::imread(filename, 1);
	}
	else
	{
		image_matrix = images_matrix[0].clone();

	}

	return page;
}

std::string SelectFolder(HWND handleWnd, wchar_t* p_initializePath)
{
	CString selected_folder = L"";
	ITEMIDLIST* p_item_idlist;
	wchar_t  selected_path[MAX_PATH];
	BROWSEINFO browse_info;

	memset(&browse_info, 0x00, sizeof(BROWSEINFO));

	browse_info.hwndOwner = handleWnd;
	browse_info.pidlRoot = NULL;
	browse_info.pszDisplayName = p_initializePath;
	browse_info.lpszTitle = _T("");
	browse_info.ulFlags = BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS | BIF_DONTGOBELOWDOMAIN;
	browse_info.lpfn = BrowseCallbackProcEx;
	browse_info.lParam = (LPARAM)p_initializePath;

	p_item_idlist = ::SHBrowseForFolder(&browse_info);

	if (p_item_idlist != NULL)
	{
		SHGetPathFromIDList(p_item_idlist, selected_path);

		selected_folder = selected_path;
	}

	return CT2CA(selected_folder);
}

int CALLBACK BrowseCallbackProcEx(HWND handleWnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	switch (uMsg) {
	case BFFM_INITIALIZED:
	{
		// 해당 Control ID는 shell32.dll version 5.0 이상에서 사용된다.
		// 하위 버전에서는 현재 tree control이 id가 다르며
		// 새폴더 버튼을 생성할 수 없다.
		HWND handleShell = GetDlgItem(handleWnd, 0);  // 0x00000000(Shell Class)
		HWND handleTree = GetDlgItem(handleShell, 100); // 0x00000064(Tree Control)
		HWND handleNew = GetDlgItem(handleWnd, 14150); // 0x00003746(New Folder Button)
		HWND handleOK = GetDlgItem(handleWnd, 1);  // 0x00000001(OK Button)
		HWND handleCancel = GetDlgItem(handleWnd, 2);  // 0x00000002(Cancel Button)
		HWND handleStatic = GetDlgItem(handleWnd, 14146); // 0x00003742(Static Control)

												// 하나라도 못가져오면 기본 구성으로 처리한다.
		if (!handleShell || !handleTree || !handleNew || !handleOK || !handleCancel)
			return 0;

		// 상단 표시줄 삭제
		if (handleStatic)
		{
			EnableWindow(handleStatic, FALSE);
			ShowWindow(handleStatic, SW_HIDE);
		}

		CRect rectWnd;
		CRect rectNew;
		CRect rectOK;
		CRect rectCancel;
		GetClientRect(handleWnd, &rectWnd);
		GetClientRect(handleNew, &rectNew);
		GetClientRect(handleOK, &rectOK);
		GetClientRect(handleCancel, &rectCancel);

		// Tree 크기조정
		// Tree를 가지는 shell의 크기조정
		MoveWindow(handleShell,
			rectWnd.left + 10,
			rectWnd.top + 10,
			rectWnd.Width() - 22,
			rectWnd.Height() - rectNew.Height() - 24,
			TRUE);
		// 새 폴더 Button 변경
		MoveWindow(handleNew,
			rectWnd.left + 10,
			rectWnd.bottom - rectNew.Height() - 10,
			rectNew.Width(),
			rectNew.Height(),
			TRUE);
		// 확인 Button
		MoveWindow(handleOK,
			rectWnd.right - 10 - rectCancel.Width() - 5 - rectOK.Width(),
			rectWnd.bottom - rectOK.Height() - 10,
			rectOK.Width(),
			rectOK.Height(),
			TRUE);
		// 취소 Button
		MoveWindow(handleCancel,
			rectWnd.right - 10 - rectCancel.Width(),
			rectWnd.bottom - rectCancel.Height() - 10,
			rectCancel.Width(),
			rectCancel.Height(),
			TRUE);

		ShowWindow(handleNew, SW_HIDE);

		::SendMessage(handleWnd, BFFM_SETSELECTION, TRUE, lpData);
	}
	break;
	case BFFM_SELCHANGED:
	case BFFM_VALIDATEFAILED:
		break;
	}
	return 0;
}

cv::Mat ReadImageW(const std::wstring& path, int flags)
{
	FILE* fp = nullptr;
	if (_wfopen_s(&fp, path.c_str(), L"rb") != 0 || fp == nullptr)
	{
		std::wcout << L"[Image] open failed: " << path << std::endl;
		return cv::Mat();
	}
	_fseeki64(fp, 0, SEEK_END);
	const long long len = _ftelli64(fp);
	_fseeki64(fp, 0, SEEK_SET);
	if (len <= 0)
	{
		fclose(fp);
		return cv::Mat();
	}
	std::vector<uchar> buf((size_t)len);
	const size_t n = fread(buf.data(), 1, buf.size(), fp);
	fclose(fp);
	if (n != buf.size())
		return cv::Mat();

	cv::Mat img = cv::imdecode(buf, flags);
	if (img.empty())
		std::wcout << L"[Image] decode failed: " << path << std::endl;
	return img;
}

bool WriteImageW(const std::wstring& path, const cv::Mat& img, int jpgQuality)
{
	if (img.empty())
		return false;
	std::wstring ext = path.substr(path.find_last_of(L'.'));
	for (wchar_t& c : ext) c = towlower(c);
	std::vector<int> params;
	if (ext == L".jpg" || ext == L".jpeg")
		params = { cv::IMWRITE_JPEG_QUALITY, jpgQuality };

	std::vector<uchar> buf;
	if (!cv::imencode(std::string(ext.begin(), ext.end()), img, buf, params))
		return false;

	FILE* fp = nullptr;
	if (_wfopen_s(&fp, path.c_str(), L"wb") != 0 || fp == nullptr)
	{
		std::wcout << L"[Image] write failed: " << path << std::endl;
		return false;
	}
	const size_t n = fwrite(buf.data(), 1, buf.size(), fp);
	fclose(fp);
	return n == buf.size();
}

// ANSI(std::string) 경로 -> 와이드로 바꿔서 호출
static std::wstring ToW(const std::string& s)
{
	int n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
	std::wstring w(n > 0 ? n - 1 : 0, L'\0');
	if (n > 0) MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &w[0], n);
	return w;
}

cv::Mat ReadImageA(const std::string& path, int flags) 
{ 
	return ReadImageW(ToW(path), flags); 
}
bool    WriteImageA(const std::string& path, const cv::Mat& img, int jpgQuality) 
{ 
	return WriteImageW(ToW(path), img, jpgQuality); 
}