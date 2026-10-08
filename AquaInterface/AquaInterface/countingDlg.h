#pragma once
#include "afxdialogex.h"


// Ccounting 대화 상자

class CcountingDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CcountingDlg)

public:
	CcountingDlg(CWnd* pParent = nullptr);   // 표준 생성자입니다.
	virtual ~CcountingDlg();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_COUNTING_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

	DECLARE_MESSAGE_MAP()
};
