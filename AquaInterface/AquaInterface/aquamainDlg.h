#pragma once
#include "afxdialogex.h"
#include "AquaInterfaceDlg.h"
#include "countingDlg.h"

// Caquamain 대화 상자

class Caquamain : public CDialogEx
{
	DECLARE_DYNAMIC(Caquamain)

public:
	Caquamain(CWnd* pParent = nullptr);   // 표준 생성자입니다.
	virtual ~Caquamain();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_AQUAMAIN };
#endif

protected:
	HICON m_hIcon;
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

	DECLARE_MESSAGE_MAP()
private:
	CTabCtrl m_aqua_tab_control;
	CAquaInterfaceDlg* m_pAquaDlg;
	CcountingDlg* m_pCountingDlg;
public:
	virtual BOOL DestroyWindow();
	virtual BOOL OnInitDialog();
	afx_msg void OnTcnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult);
};
