// AquaInterfaceDlg.h: 헤더 파일
//

#pragma once

#include "..\BaslerCamIF\BaslerLinescanCam.h"
#include "LedCtrl.h"
#include "Piechartctrl.h"


#define WM_USER_RESULT   (WM_USER + 100)   // 처리 스레드 -> UI : 분류 결과
#define TIMER_STATUS     1                 // 상태 표시 갱신 타이머


#define RESULT_TANK_A    0
#define RESULT_TANK_B    1
#define RESULT_TANK_C    2
#define RESULT_UNKNOWN   4
#define RESULT_FLAG_NG   0x100        // 불량 표시 (수조 번호와 OR)
#define RESULT_TANK_MASK 0x0FF


enum FrameState
{
	FrameState_Idle = 0,    // 취득 전  (회색)
	FrameState_Ok,          // 정상 취득 (초록)
	FrameState_Lost         // 누락 발생 (빨강)
};

// CAquaInterfaceDlg 대화 상자
class CAquaInterfaceDlg : public CDialogEx
{
	// 생성입니다.
public:
	CAquaInterfaceDlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.
	virtual ~CAquaInterfaceDlg();                   // m_pCam 해제 (안전장치)

	static CString FormatNum(unsigned long long v);                                 
	static CString FormatCountSub(unsigned long long count, unsigned long long sub);

	// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_AQUAINTERFACE_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.


	// 구현입니다.
protected:

	// 카메라
	CBaslerLineScan* m_pcam = nullptr;                      // OnInitDialog에서 new, OnDestroy에서 delete
	static bool     OnChunk(const FrameChunk& c, void* user); // 처리 스레드에서 호출

	// 상태 LED (검측 중: 초록 / 종료: 빨강)
	CLedCtrl        m_ledRed;
	CLedCtrl        m_ledGreen;
	void            SetRunState(bool running);       // LED + 버튼 활성화

	// 결과 카운트 (UI 스레드에서만 사용)
	UINT64          m_nOk = 0;
	UINT64          m_nNg = 0;

	CPieChartCtrl* m_pPie = nullptr;   // OnInitDialog 에서 new, OnDestroy 에서 delete
	int            m_pieA   = -1;        // A수조
	int            m_pieB   = -1;        // B수조
	int            m_pieC   = -1;        // C수조
	int            m_pieNg  = -1;        // 불량
	int            m_pieUnk = -1;

	// 취득 상태 표시
	FrameState m_frameState = FrameState_Idle;

	ULONGLONG     m_lastTick = 0;
	std::uint64_t m_lastFrames = 0;

	CBrush* m_pBrIdle = nullptr;   // 회색
	CBrush* m_pBrOk = nullptr;   // 초록
	CBrush* m_pBrLost = nullptr;   // 빨강

	void UpdateStatus();
	void ResetStatus();

	void ShowCamError(LPCTSTR title);

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg LRESULT OnResult(WPARAM wp, LPARAM lp);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnDestroy();
	afx_msg void OnStnClickedStaticGrab();
	afx_msg void OnBnClickedButtonStart();
	afx_msg void OnBnClickedEnd();
	afx_msg void OnLvnItemchangedList1(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBnClickedButtonSettingfile();
};