#pragma once
#include "afxdialogex.h"
#include <iostream>
#include <algorithm>
#include <vector>
#include "processer.h"

// Ccounting 대화 상자

#define WM_COUNT_PROGRESS (WM_APP + 101)
#define WM_COUNT_DONE     (WM_APP + 102)
struct AreaRect
{
	int x1, y1, x2, y2;
	bool selected;

	AreaRect() : x1(0), y1(0), x2(0), y2(0), selected(false) {}
	AreaRect(int _x1, int _y1, int _x2, int _y2)
		: x1(_x1), y1(_y1), x2(_x2), y2(_y2), selected(false)
	{
	}
	CRect GetRect() const
	{
		return CRect(x1, y1, x2, y2);
	}
	bool Contains(CPoint pt) const
	{
		return pt.x >= x1 && pt.x <= x2 && pt.y >= y1 && pt.y <= y2;
	}
	bool IsNearEdge(CPoint pt, int threshold = 5) const
	{
		return (abs(pt.x - x1) < threshold || abs(pt.x - x2) < threshold ||
			abs(pt.y - y1) < threshold || abs(pt.y - y2) < threshold);
	}
};

struct FileRowData                 // AddFileItem에서 new, LVN_DELETEITEM에서 delete
{
	CString    path;
	SegResult* pSeg = NULL;        // 카운팅 전 NULL, 결과 저장 시 new
};

struct CountRowResult        // 작업 스레드에서 new, OnCountProgress 에서 delete
{
	int       row = -1;
	bool      ok = false;
	SegResult seg;
	cv::Mat   origin;
	cv::Mat   result;
};


class CcountingDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CcountingDlg)

public:
	CcountingDlg(CWnd* pParent = nullptr);   // 표준 생성자입니다.
	virtual ~CcountingDlg();
	void ClearAllAreas();
	void DisplayImage(const cv::Mat& image_matrix, CDC* p_dc, RECT rect);
	void InitListControl(CListCtrl& list_control);
	
	void SetProcesser(Cprocesser* pProc) { m_pProc = pProc; }
	void DrawViews();
	int  AddFileItem(const CString& path);
	void SelectRow(int index);
	bool IsImageFile(const CString& path) const;

	virtual BOOL OnInitDialog();

	void OnLvnDeleteitemListFile(NMHDR* pNMHDR, LRESULT* pResult);
// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_COUNTING_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.
	afx_msg void OnPaint();
	afx_msg void OnClickedButtonOpenFile();
	afx_msg void OnLvnItemchangedListFile(NMHDR* pNMHDR, LRESULT* pResult);
	

	void ShowFile(int index);
	DECLARE_MESSAGE_MAP()
private:
	CEdit _location_control;
	POINT _point_left_top;
	POINT _point_right_bottom;
	CListCtrl _list_file_control;
	std::vector<AreaRect> _area_list;
	CEdit m_point_control;

	CStatic _image_control;
	CStatic _result_control;
	CListCtrl _filelist_control;
	CEdit m_location_control;


	int _selected_area_index;
	int m_nCurRow;
	cv::Mat _image_matrix;
	cv::Mat _result_matrix;
	std::vector<cv::Mat> _multi_image_matrix;

	Cprocesser* m_pProc;
	SegResult m_SegResult;

	CWinThread* m_pCountThread;
	std::atomic<bool>    m_bStopCount;
	bool                 m_bCounting;
	std::vector<CString> m_vCountPaths;
	CString              m_strCountSaveDir;
	int                  m_nAllImages, m_nAllCount, m_nAllDead, m_nAllFail;
	double               m_dAllMs;

	static UINT CountAllThread(LPVOID pParam);     // AfxBeginThread 진입점 (static 필수)
	UINT RunCountAll();
	void SetBusy(bool busy);
	FileRowData* GetRow(int row) const;
	void SetRowResult(int row, const SegResult& seg);
	void SetRowText(int row, const SegResult& seg);
	double TotalMs(const SegResult& seg) const;
	bool SaveImageW(const CString& path, const cv::Mat& img) const;
	bool AppendResultCsv(const CString& csvPath, const CString& fileName, const SegResult& res) const;
	CString m_strSaveDir;
	void OnDestroy();

	void EnableCloseButton(bool enable);

public:
	afx_msg void OnBnClickedButtonStart();
	afx_msg void OnBnClickedButtonFolderpath();
	afx_msg void OnBnClickedButtonSave();
	afx_msg void OnBnClickedButtonAllcounting();
	afx_msg LRESULT OnCountProgress(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnCountDone(WPARAM wParam, LPARAM lParam);
	void StopCountAll();
	bool IsCounting() const { return m_bCounting; }
};
