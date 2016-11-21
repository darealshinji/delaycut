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
=============Changelog===============
v1.3.0.0
Added: E-AC3 support (added by www.madshi.net)

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
  -startsplit <ini_sec>: Starts in ini_sec
  -endsplit <end_sec>:  Stops in end_sec
  -console: starts a new window (console)

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
