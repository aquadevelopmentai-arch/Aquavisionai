#pragma once
#include "afxdialogex.h"
#include "AquaInterfaceDlg.h"
#include "countingDlg.h"
#include "processer.h"

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
	virtual void OnCancel();   // X 버튼, Alt+F4, Esc
	virtual void OnOK();
	DECLARE_MESSAGE_MAP()
private:
	CTabCtrl m_aqua_tab_control;
	CAquaInterfaceDlg* m_pAquaDlg;
	CcountingDlg* m_pCountingDlg;

	Cprocesser* m_pProcess;

	bool m_bCounting;


	bool EnsureProcesser();      // 없으면 생성 + 초기화
	void ReleaseProcesser();     // 카운팅 정지 -> 포인터 끊기 -> 해제

public:

	afx_msg void OnTcnSelchangingTab(NMHDR* pNMHDR, LRESULT* pResult);   // 바뀌기 전 (막기)
	afx_msg void OnTcnSelchangeTab(NMHDR* pNMHDR, LRESULT* pResult);
	virtual BOOL DestroyWindow();
	virtual BOOL OnInitDialog();
	afx_msg void OnTcnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult);
};
