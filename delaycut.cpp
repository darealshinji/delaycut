/*
delaycut: cuts and corrects delay in ac3 and dts files
Copyright (C) 2004 by jsoto

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
// delaycut.cpp : Defines the class behaviors for the application.
//
// 2007-09-11 E-AC3 support added by www.madshi.net
//

#include "stdafx.h"
#include "io.h"
#include "fcntl.h"
#include "ToolTipDialog.h"
#include "delayac3.h"
#include "delaycut.h"
#include "delaycutDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern int getac3info ( FILE *in, FILEINFO *myinfo);
extern int geteac3info( FILE *in, FILEINFO *myinfo);
extern int getdtsinfo ( FILE *in, FILEINFO *myinfo);
extern int getmpainfo ( FILE *in, FILEINFO *myinfo);
extern int getwavinfo ( FILE *in, FILEINFO *myinfo);
extern int gettargetinfo ( FILEINFO *fileinfo);
extern void ac3_crc_init(void);

/////////////////////////////////////////////////////////////////////////////
// CDelaycutApp

BEGIN_MESSAGE_MAP(CDelaycutApp, CWinApp)
	//{{AFX_MSG_MAP(CDelaycutApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDelaycutApp construction

CDelaycutApp::CDelaycutApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CDelaycutApp object

CDelaycutApp theApp;


/////////////////////////////////////////////////////////////////////////////
// stdout & stderr
void Redirect_console()
{
	int hCrtout,hCrterr;
	FILE *hfout, *hferr;

	AllocConsole();

	hCrtout=_open_osfhandle( (long) GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT );
	hfout=_fdopen(hCrtout, "w");
	*stdout= *hfout;
	setvbuf(stdout,NULL, _IONBF,0);

	hCrterr=_open_osfhandle( (long) GetStdHandle(STD_ERROR_HANDLE), _O_TEXT );
	hferr=_fdopen(hCrterr, "w");
	*stderr= *hferr;
	setvbuf(stderr,NULL, _IONBF,0);

}

/////////////////////////////////////////////////////////////////////////////
// CDelaycutApp initialization

BOOL CDelaycutApp::InitInstance()
{
	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	m_csLog="";
	m_bAbort=false;

	ac3_crc_init();

// CLI mode
	if ( __argc > 2 )
	{
		m_bCLI=true;

		if (ParseCommandLine() != TRUE)
		{
			m_iRet=-1;
			if ( m_bConsole) Redirect_console();
			if ( m_bConsole) fprintf (stderr,"Error parsing command line\n");
			return FALSE;
		}

		if ( m_bConsole) Redirect_console();

		m_iRet=0;

		m_Fin=fopen(m_csInputfile,"rb");
		if (m_Fin ==NULL) 
		{
			m_iRet=-1;
			if ( m_bConsole) fprintf (stderr,"Error: Cannot open inputfile %s\n",m_csInputfile);
			return FALSE;
		}

		Initinfo();
		if (m_csExt=="wav")
			getwavinfo ( m_Fin, &m_myinfo);
		else if (m_csExt=="ac3")
			getac3info ( m_Fin, &m_myinfo);
		else if (m_csExt=="eac3" || m_csExt=="ddp" || m_csExt=="ec3" || m_csExt=="dd+")
			geteac3info ( m_Fin, &m_myinfo);
		else if (m_csExt=="dts")
			getdtsinfo ( m_Fin, &m_myinfo);
		else if (m_csExt=="mpa" || m_csExt=="mp2" || m_csExt=="mp3" )
			getmpainfo ( m_Fin, &m_myinfo);
		else
		{
			fclose (m_Fin);
			return FALSE;
		}
		fclose (m_Fin);

		m_csLogfile= m_csInputfile.Left(m_csInputfile.ReverseFind('.'))+"_log.txt";
		LogInputInfo();

		if (m_iInfo) 		return FALSE;

		gettargetinfo (&m_myinfo);

		LogTargetInfo();

		m_iRet= ExeDelaycut(NULL);
		
		//  return FALSE so that we exit the
		//  application, rather than start the application's message pump.
		return FALSE;
	}

	
// Dlg mode	
	CDelaycutDlg dlg;
	m_pMainWnd = &dlg;
	m_bCLI=false;

	int nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}
	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}


void CDelaycutApp::UpdateProgress( CWnd* pDlg, int nPerc)
{
	MSG msg;
	static int oldPerc=-1;
	static int calls=0;

// Just to improve the performance
	calls++;
	if ((calls%10) != 0 && nPerc !=100) return;


	if ( pDlg )
	{
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (oldPerc!=nPerc) 
		{
			((CDelaycutDlg *)pDlg)->UpdateProgress(nPerc);
			oldPerc=nPerc;
		}
	}

}


BOOL CDelaycutApp::ParseCommandLine()
{

/*
Command Line 
Output and log files will be in the same path than the input file.

delaycut [options] <filein> 
 Options:
  -info: Do nothing, only outputs info in log file
  -fixcrc {ignore, skip, fix, silence}: Action in the case of crc errors
  -start <delay_msecs>: Adds (-Cuts) the needed frames  at the beginning
  -end <delay_msecs>: Adds (-Cuts) the needed frames  at the end (default same)
  -auto: detect start delay in filename (assuming DVD2AVI style)
  -startsplit <ini_sec>: Starts in ini_sec
  -endsplit <end_sec>:  Stops in end_sec
  -console: starts a new window (console)
  -out  <output_filename>

Return values
 0: Success
-1: Something went wrong.


Examples
Get info: Log file will be myfile_log.txt
	delaycut -info myfile.ac3

Adds 100 msec of silence at the begining. File lenght will be the same
	delaycut -start 100 -same myfile.ac3

Adds 100 msec of silence at the begining. File lenght will be 100 msecs more
	delaycut -start 100 myfile.ac3

Cuts start at 10.32 sec and ends at 15.20 sec. 
	delaycut -startsplit 10.32 -endsplit 15.20 myfile.ac3

Cuts start at 10.32 sec and ends at 15.20 sec. Delay correction of 100 msec. 
	delaycut -start 100 -startsplit 10.32 -endsplit 15.20 myfile.ac3

*/


// Check arguments
	int i,iSame;
	CString csPar,csAux;
	char szOutputfile[MAX_PATH];
	bool bAuto;

	m_iInfo=iSame=0;
	m_dCutstart=m_dCutend=0;
    m_dDelaystart=m_dDelayend=0;
	m_bConsole=false;
	szOutputfile[0]=0;

	m_iCrc=CRC_SILENCE;
	m_csFixcrc="SILENCED";

	bAuto=false;

	if (__argc <3  )
		return false;

	for (i =1 ; i<(__argc)-1 ; i++)
	{
		csPar.Format(_T("%s"),(__argv[i]));
		csPar.MakeLower();
		if (csPar=="-auto") bAuto=true;
		if (csPar=="-info") m_iInfo=1;
		if (csPar=="-same") iSame=1;
		if (csPar=="-console") m_bConsole=true;
		if (csPar=="-start")
			{ i++; if (sscanf((__argv[i]), "%lf", &m_dDelaystart)!=1) return false;}
		if (csPar=="-end")
			{ i++; if (sscanf((__argv[i]), "%lf", &m_dDelayend)!=1)	return false;}
		if (csPar=="-startsplit")
			{ i++; if (sscanf((__argv[i]), "%lf", &m_dCutstart)!=1)	return false;}
		if (csPar=="-endsplit")
			{ i++; if (sscanf((__argv[i]), "%lf", &m_dCutend)!=1) return false;}
		if (csPar=="-fixcrc")
		{
			i++;
			csPar.Format(_T("%s"),(__argv[i]));
			csPar.MakeLower();
			if (csPar=="ignore") {m_iCrc=CRC_IGNORE; m_csFixcrc="IGNORED";}
			if (csPar=="fix") {m_iCrc=CRC_FIX; m_csFixcrc="FIXED";}
			if (csPar=="skip") {m_iCrc=CRC_SKIP; m_csFixcrc="SKIPPED";}
			if (csPar=="silence") {m_iCrc=CRC_SILENCE; m_csFixcrc="SILENCED";}
		}
		if (csPar=="-out")
		{
			i++;
			strcpy (szOutputfile, (__argv[i]));
			if (strlen(szOutputfile) <=0)
				return false;
		}
//			{ i++; if (sscanf((__argv[i]), "%s", szOutputfile)!=1) return false;}

	}
	

// Input and Output filenames
	m_csInputfile.Format(_T("%s"),(__argv[(__argc)-1]));
	m_csInputfile.MakeLower();
	m_csExt= m_csInputfile.Right(m_csInputfile.GetLength() - m_csInputfile.ReverseFind('.')-1);
	m_csExt.MakeLower();
	if (szOutputfile[0]!=0)
		m_csOutputfile= szOutputfile;
	else
		m_csOutputfile= m_csInputfile.Left(m_csInputfile.ReverseFind('.'))+"_fixed." + m_csExt;

	m_csLogfile= m_csInputfile.Left(m_csInputfile.ReverseFind('.'))+"_log.txt";

	if (bAuto)
	{
		csAux= m_csInputfile.Right(m_csInputfile.GetLength() - m_csInputfile.ReverseFind(' ')-1);
		csAux= csAux.Left(csAux.Find("ms."));
		sscanf(csAux, "%lf", &m_dDelaystart );
	}

	if (iSame) m_dDelayend= m_dDelaystart;

	if (m_dCutstart !=0 || m_dCutend != 0)
		if (m_dCutstart<0 || m_dCutend <0 || ( m_dCutend != 0 && m_dCutend<m_dCutstart))
			return false;

	return true;
}


// Exit Value
int CDelaycutApp::ExitInstance()
{
	int iRet;
	iRet= CWinApp::ExitInstance();
    
	if (__argc == 1)
		return iRet;
	else
		return m_iRet;
}

int CDelaycutApp::ExeDelaycut(CWnd* pWnd)
{
// Returns 0 if  OK

    int iRet;

	if (m_iCrc==CRC_IGNORE)		m_csFixcrc="IGNORED";
	if (m_iCrc==CRC_FIX)		m_csFixcrc="FIXED";
	if (m_iCrc==CRC_SKIP)		m_csFixcrc="SKIPPED";
	if (m_iCrc==CRC_SILENCE)	m_csFixcrc="SILENCED";


	m_Fout=fopen(m_csOutputfile,"wb");
	if (m_Fout ==NULL) return -1;

	m_Fin=fopen(m_csInputfile,"rb");
	if (m_Fin ==NULL) 
	{
		fclose (m_Fout);
		return -1;
	}

	m_csLogfile= m_csInputfile.Left(m_csInputfile.ReverseFind('.'))+"_log.txt";
	m_Flog=fopen(m_csLogfile,"a");
	if (m_Flog ==NULL) 
	{
		fclose (m_Fin);
		fclose (m_Fout);
		return -1;
	}

	if (m_csExt=="wav")
		iRet=delaywav (pWnd);
	if (m_csExt=="ac3")
		iRet=delayac3 (pWnd);
	if (m_csExt=="eac3" || m_csExt=="ddp" || m_csExt=="ec3" || m_csExt=="dd+")
		iRet=delayeac3 (pWnd);
	if (m_csExt=="dts")
		iRet=delaydts (pWnd);
	if (m_csExt=="mpa" || m_csExt=="mp2" || m_csExt=="mp3" )
		iRet=delaympa (pWnd);

	fclose (m_Fout);
	fclose (m_Fin);
	fclose (m_Flog);


	m_iRet=iRet;

    return iRet;

}

int CDelaycutApp::LogInputInfo()
{
	m_Flog=fopen(m_csLogfile,"w");
	if (m_Flog ==NULL) 	return -1;
	else
	{
		if (m_csExt!="wav")
		{
			fprintf(m_Flog,"[Input info]\n");
			fprintf(m_Flog, "Bitrate=%d\n",m_myinfo.iBitrate);
			fprintf(m_Flog, "Actual rate=%lf\n",m_myinfo.dactrate);
			fprintf(m_Flog, "Sampling Frec=%d\n",m_myinfo.fsample);
			fprintf(m_Flog, "TotalFrames=%I64d\n",m_myinfo.i64TotalFrames);
			fprintf(m_Flog, "Bytesperframe=%9.4lf\n",m_myinfo.dBytesperframe);
			fprintf(m_Flog, "Filesize=%I64d\n",m_myinfo.i64Filesize);	
			fprintf(m_Flog, "FrameDuration=%8.4lf\n",m_myinfo.dFrameduration);
			fprintf(m_Flog, "Framespersecond=%8.4lf\n",m_myinfo.dFramesPerSecond);
			fprintf(m_Flog, "Duration=%s\n",m_myinfo.csOriginalDuration);
			fprintf(m_Flog, "Channels mode=%s\n",m_myinfo.csMode);
			fprintf(m_Flog, "LFE=%s\n",m_myinfo.csLFE);
		}
		else
		{
			fprintf(m_Flog,"[Input info]\n");
			fprintf(m_Flog, "Bitrate=%d\n",m_myinfo.iBitrate);
			fprintf(m_Flog, "Actual rate=%lf\n",m_myinfo.dactrate);
			fprintf(m_Flog, "Byte rate=%d\n",m_myinfo.iByterate);
			fprintf(m_Flog, "Sampling Frec=%d\n",m_myinfo.fsample);
			fprintf(m_Flog, "Bits of Prec=%d\n",m_myinfo.iBits);
			fprintf(m_Flog, "TotalSamples=%I64d\n",m_myinfo.i64TotalFrames);
			fprintf(m_Flog, "Bytespersample=%9.4lf\n",m_myinfo.dBytesperframe);
			fprintf(m_Flog, "Filesize=%I64d\n",m_myinfo.i64Filesize);	
			fprintf(m_Flog, "Duration=%s\n",m_myinfo.csOriginalDuration);
			fprintf(m_Flog, "Channels mode=%s\n",m_myinfo.csMode);
		}
		fclose (m_Flog);
		return 0;
	}

  }
int CDelaycutApp::LogTargetInfo()
{
	m_Flog=fopen(m_csLogfile,"a");
	if (m_Flog ==NULL) 	return -1;
	else
	{
		if (m_csExt!="wav")
		{
			fprintf(m_Flog,"[Target info]\n");
			fprintf(m_Flog, "StartFrame=%I64d\n",m_myinfo.i64StartFrame);
			fprintf(m_Flog, "EndFrame=%I64d\n",m_myinfo.i64EndFrame);
			fprintf(m_Flog, "NotFixedDelay=%8.4lf\n",m_myinfo.dNotFixedDelay);
			fprintf(m_Flog, "Duration=%s\n",m_myinfo.csTimeLengthEst);
		}
		else
		{
			fprintf(m_Flog,"[Target info]\n");
			fprintf(m_Flog, "StartSample=%I64d\n",m_myinfo.i64StartFrame);
			fprintf(m_Flog, "EndSample=%I64d\n",m_myinfo.i64EndFrame);
			fprintf(m_Flog, "NotFixedDelay=%8.4lf\n",m_myinfo.dNotFixedDelay);
			fprintf(m_Flog, "Duration=%s\n",m_myinfo.csTimeLengthEst);
		}
		fclose (m_Flog);
		return 0;
	}
}

void CDelaycutApp::Initinfo()
{
	m_myinfo.csType="unknown";
	m_myinfo.csMode="unknown";
	m_myinfo.csLFE="unknown";
	m_myinfo.i64Filesize=0;
	m_myinfo.i64TotalFrames=0;
	m_myinfo.i64rest=0;
	m_myinfo.iBitrate=0;
	m_myinfo.iByterate=0;
	m_myinfo.iInidata=0;
	m_myinfo.dactrate=0.0;
	m_myinfo.mode=0;
	m_myinfo.layer=0;
	m_myinfo.fsample=0;
	m_myinfo.dBytesperframe=0;
	m_myinfo.dFrameduration=0.0;
	m_myinfo.dFramesPerSecond=0.0;
	m_myinfo.csOriginalDuration="";
	m_myinfo.bsmod=0;
	m_myinfo.acmod=0;
	m_myinfo.frmsizecod=0;
	m_myinfo.fscod=0;
	m_myinfo.cpf=0;

	m_myinfo.i64StartFrame=0;
	m_myinfo.i64EndFrame=0;
	m_myinfo.i64frameinfo=0;
	m_myinfo.dNotFixedDelay=0;
	m_myinfo.csTimeLengthEst="";
}





