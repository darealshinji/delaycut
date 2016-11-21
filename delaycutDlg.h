// delaycutDlg.h : header file
//

#if !defined(AFX_DELAYCUTDLG_H__0F7245F9_D4E6_4F85_B7D7_EA9A20DDAD51__INCLUDED_)
#define AFX_DELAYCUTDLG_H__0F7245F9_D4E6_4F85_B7D7_EA9A20DDAD51__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CDelaycutDlg dialog

class CDelaycutDlg : public CToolTipDialog
{
// Construction
public:
	CDelaycutDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CDelaycutDlg)
	enum { IDD = IDD_DELAYCUT_DIALOG };
	CProgressCtrl	m_CProgressbar;
	BOOL	m_bCheckcut;
	BOOL	m_bCheckdelay;
	CString	m_csCutend;
	CString	m_csCutstart;
	CString	m_csInputfile;
	CString	m_csOutputfile;
	CString	m_csExt;
	CString	m_csInfo;
	int		m_iCrc;
	CString	m_csDelayend;
	CString	m_csDelaystart;
	CString	m_csProgresstitle;
	//}}AFX_DATA

	CString	m_csFileInfo, m_csTarget, m_csLog;

	FILE *  m_Fin;

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDelaycutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL
public:
	virtual void UpdateProgress(int nPerc);
	virtual void CalculateTarget();
	virtual void SetScroll();

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CDelaycutDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnButtoninput();
	afx_msg void OnButtonoutput();
	afx_msg void OnCheckdelay();
	afx_msg void OnCheckcut();
	afx_msg void OnChangeEditdelaye();
	afx_msg void OnChangeEditcute();
	afx_msg void OnChangeEditcuts();
	afx_msg void OnChangeEditdelays();
	virtual void OnCancel();
	virtual void OnOK();
	afx_msg void OnButtonabort();
	afx_msg void OnChangeEditinput();
	//}}AFX_MSG
	afx_msg void OnDropFiles( HDROP hDropInfo);

	virtual void GetInputInfo();


	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DELAYCUTDLG_H__0F7245F9_D4E6_4F85_B7D7_EA9A20DDAD51__INCLUDED_)
