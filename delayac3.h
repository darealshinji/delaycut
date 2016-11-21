// actions in crc errors
#define CRC_IGNORE 0
#define CRC_FIX 1
#define CRC_SILENCE 2
#define CRC_SKIP 3

//actions to write frame
#define WF_SKIP 0
#define WF_WRITE 1
#define WF_SILENCE 2

#define MAXFRAMESIZE 16384

typedef struct FILEINFO
{
	CString csType;
	int iBitrate;
	int iByterate;
	int iInidata; // ini of data chunk in wave files
	double dactrate;
	int mode;
	int iBits;
	CString csMode,csLFE;
	__int64 i64Filesize, i64TotalFrames, i64rest;
    double dBytesperframe;
    int layer;
	int fsample;
	double dFrameduration, dFramesPerSecond;
	CString csOriginalDuration;
	UINT bsmod, acmod, frmsizecod, fscod,cpf;
//Estimated results
	CString csTimeLengthEst;
	__int64 i64StartFrame;
	__int64 i64EndFrame;
	__int64 i64frameinfo;
	double dNotFixedDelay;

} FILEINFO;