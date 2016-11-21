// delaycut.h : main header file for the DELAYCUT application
//

#if !defined(AFX_DELAYCUT_H__7EABFF7E_447E_4195_8FB9_4EDD5EC9EA48__INCLUDED_)
#define AFX_DELAYCUT_H__7EABFF7E_447E_4195_8FB9_4EDD5EC9EA48__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CDelaycutApp:
// See delaycut.cpp for the implementation of this class
//
// 2007-09-11 E-AC3 support added by www.madshi.net
//

class CDelaycutApp : public CWinApp
{
public:
	CDelaycutApp();

// Same values 
//	BOOL	m_bCheckcut;
//	BOOL	m_bCheckdelay;
	double	m_dCutend;  // in secs
	double	m_dCutstart; // in secs
	CString	m_csLogfile;
	CString	m_csInputfile;
	CString	m_csOutputfile;
	int		m_iCrc; 
	double	m_dDelayend;
	double	m_dDelaystart;
	int		m_iRet;
	bool	m_bCLI;
	bool	m_bAbort;
	bool 	m_bConsole;
	int		m_iInfo;
	CString m_csFixcrc;
	CString m_csExt;
	FILEINFO m_myinfo;
	FILE   *m_Fin,*m_Fout,*m_Flog;
	CString m_csLog;


	
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDelaycutApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL
	virtual void UpdateProgress( CWnd* pDlg, int nPerc);
	virtual int ExitInstance();
	virtual BOOL ParseCommandLine();
	virtual int ExeDelaycut(CWnd* pWnd);
	virtual int delaydts(CWnd* pWnd);
	virtual int delayac3(CWnd* pWnd);
	virtual int delayeac3(CWnd* pWnd);
	virtual int delaympa(CWnd* pWnd);
	virtual int delaywav(CWnd* pWnd);
	virtual void printlog(CString csLinea);
	virtual void printline(CString csLinea);
	virtual int LogInputInfo();
	virtual int LogTargetInfo();
	virtual void Initinfo();


// Implementation

	//{{AFX_MSG(CDelaycutApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DELAYCUT_H__7EABFF7E_447E_4195_8FB9_4EDD5EC9EA48__INCLUDED_)
