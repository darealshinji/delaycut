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

====================================================================
=============Changelog==============================================
v1.3.0.0
Added:       E-AC3 support (added by www.madshi.net)

v1.2.1.2
BugFix:      Error in duration of Wav files

v1.2.1.1
Changed:     Maximum number of char changed to 8 in delay edit boxes
BugFix:      Spaces in output path (CLI mode)

v1.2.1.0
Added:       44.1kHz and 32 kHz  ac3 support (suggested by tebasuna51)
Improvement: In case of unsynchronized frame, rewind one frame more to look for 
             the synch word inside the previous frame. (suggested by tebasuna51)
Changed:     AC3 silence frames are now calculated from only two patterns
	     (2/0 and 5.1)


v1.2.0.4
Added:     Linear PCM (wav) support
Added:     mpa checksum calculation and fixing.
Added:     44.100 mpa minimum support. (short delays)
BugFix:	   Percentage dlg refresh 
BugFix:    Different rate frames in dts caused delaycut to hang

v1.2.0.3
BugFix:    -auto option in CLI mode was broken

v1.2.0.2
BugFix:	   mpa was broken in 1.2.0.1
BugFix:	   Info dlg refresh.

v1.2.0.1
Added:	   mpa (fsample=48k) support.
Added:     drag/drop files
Added:     -auto option in CLI mode: autodetect the delay in input filename 
Added:     -out option in CLI mode to specify the output filename.

v1.1.0.4
BugFix:     dts has been completely broken in 1.1.0.3 but nobody reported it????

v1.1.0.3
Improvement: Using fread and fwrite functions, delaycut is much 
             faster than before.
Changed:     Log window automaticaly scrolls to the end

v1.1.0.2
BugFix: Some erroneous files can hang delaycut

v1.1
Bugfix: Makelower in file extensions 
Bugfix: Only one message in the case of basic parameters change


====================================================================

Command Line Instructions
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
