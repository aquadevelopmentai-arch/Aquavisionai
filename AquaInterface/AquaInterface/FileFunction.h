#pragma once
#ifndef _FILEFUNCTION_HEADER_
#define _FILEFUNCTION_HEADER_

cv::Mat BGR2Gray(cv::Mat input);
cv::Mat Gray2BGR(cv::Mat input);
bool CheckImage(std::string strPath);
void CallBack(std::string str, void* p);
std::string GetSelectFolderPath(HWND hOwner, bool bAllowNewFolder, std::string strDefPath);
std::string AttachSeparator(std::string str);
void GetFilesInPath(std::string path, std::vector<std::string>& list, std::string filter /*= _T("*.*")*/, bool bShowErr /*= false*/);
int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData);
int OpenImageFile(std::string filename, cv::Mat& image_matrix, std::vector<cv::Mat>& images_matrix);
std::string SelectFolder(HWND handleWnd, wchar_t* p_initializePath);
int CALLBACK BrowseCallbackProcEx(HWND handleWnd, UINT uMsg, LPARAM lParam, LPARAM lpData);
bool PathExists(const std::string& path);
bool FileExists(const std::string& file);

cv::Mat ReadImageW(const std::wstring& path, int flags = 1 /*cv::IMREAD_COLOR*/);
cv::Mat ReadImageA(const std::string& path, int flags = 1);          // ANSI 경로용 래퍼
bool    WriteImageW(const std::wstring& path, const cv::Mat& img, int jpgQuality = 92);
bool    WriteImageA(const std::string& path, const cv::Mat& img, int jpgQuality = 92);

#endif // _FILEFUNCTION_HEADER_