// counting.cpp: 구현 파일
//

#include "pch.h"
#include "AquaInterface.h"
#include "afxdialogex.h"
#include "countingDlg.h"


// Ccounting 대화 상자

IMPLEMENT_DYNAMIC(CcountingDlg, CDialogEx)

CcountingDlg::CcountingDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_COUNTING_DIALOG, pParent)
{

}

CcountingDlg::~CcountingDlg()
{
}

void CcountingDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}



BEGIN_MESSAGE_MAP(CcountingDlg, CDialogEx)
END_MESSAGE_MAP()


// Ccounting 메시지 처리기
