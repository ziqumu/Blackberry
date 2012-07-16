The USBClient project must be linked against BbDevMgr.exe in order to compile.
If the BlackBerry JDE and Desktop Manager are installed in their default locations,
no configuration of the USBClient project is necessary, as it will know where to find
BbDevMgr.exe.  Otherwise, follow these instructions before attempting to compile:

1) Locate BbDevMgr.exe on your machine.  Its default location is
   C:\Program Files\Common Files\Research In Motion\USB Drivers.
2) In the Microsoft Visual C++ window, click Project, Settings...
3) In the "Settings For" drop-down list, select "All Configurations".
4) Click the C/C++ tab.
5) In the Category drop-down list, select Preprocessor.
6) Change the path in the "Additional include directories" text box to reflect the
   location of BbDevMgr.exe.

The project can now be compiled.