call BCBconfigB6.bat
"%BCB%\bin\bcc32.exe" %BCC_OPTIONS% -c -I"%BCB%\Include";"%BCB%\Include\vcl" -L"%BCB%\lib" ST_RecoverPlus.cpp GUIForm1.cpp AccessDiskThread.cpp AnalyseDiskThread.cpp FloppyDisk.cpp Track.cpp vcl.bpi
"%BCB%\bin\ilink32.exe" @ST_RecoverPlus.rsp
@rem https://docwiki.embarcadero.com/RADStudio/Alexandria/en/Compiling_a_C%2B%2B_Application_from_the_Command_Line
@rem https://docwiki.embarcadero.com/RADStudio/Sydney/en/BCC32.EXE,_the_C%2B%2B_32-bit_Command-Line_Compiler
@rem https://docwiki.embarcadero.com/RADStudio/Alexandria/en/Using_ILINK32_and_ILINK64_on_the_Command_Line
