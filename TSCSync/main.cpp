#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <intrin.h>
#undef NULL
#include <uefi.h>
#include "UefiBase.hpp"
#include "TextWindow.hpp"
#include "DPRINTF.H"
#include "base_t.h"
#include "LibWin324UEFI.h"

#include <Protocol\AcpiTable.h>
#include <Guid\Acpi.h>
#include <IndustryStandard/Acpi62.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>


#include <Protocol\AcpiSystemDescriptionTable.h>

extern "C" EFI_SYSTEM_TABLE * gSystemTable;
extern "C" EFI_HANDLE gImageHandle;

extern "C" int _outp(unsigned short port, int data_byte);
extern "C" unsigned short _outpw(unsigned short port, unsigned short data_word);
extern "C" unsigned long _outpd(unsigned short port, unsigned long data_word);
extern "C" int _inp(unsigned short port);
extern "C" unsigned short _inpw(unsigned short port);
extern "C" unsigned long _inpd(unsigned short port);

extern bool gfKbdDbg;

extern "C" WINBASEAPI UINT WINAPI EnumSystemFirmwareTables(
	/*_In_*/ DWORD FirmwareTableProviderSignature,
	/*_Out_writes_bytes_to_opt_(BufferSize, return)*/ PVOID pFirmwareTableEnumBuffer,
	/*_In_*/ DWORD BufferSize
);

extern "C" WINBASEAPI UINT WINAPI GetSystemFirmwareTable(
	/*_In_*/ DWORD FirmwareTableProviderSignature,
	/*_In_*/ DWORD FirmwareTableID,
	/*_Out_writes_bytes_to_opt_(BufferSize, return)*/ PVOID pFirmwareTableBuffer,
	/*_In_*/ DWORD BufferSize
);

extern "C" WINBASEAPI uint64_t WINAPI GetTickCount64(VOID);

extern void (*rgcbAtUpdate[32])(void* pThis, void* pBox, void* pContext);
extern void* rgcbAtUpdateParms[32][3];	// array of parameters: void* pThis, void* pBox, void* pContext

#define TYPE char
#define TYPESIZE sizeof(TYPE)
#define TYPEMASK ((1ULL << TYPESIZE * 8)-1)

#define FORMATW_ADDR L"%02X: %02X%s"
#define FORMATWOADDR L"%s%02X%s"

#define RTCRD(idx) (_outp(0xED,0x55),_outp(0xED,0x55),_outp(0x70,idx),_outp(0xED,0x55),_outp(0xED,0x55),_inp(0x71))

#define IODELAY _outp(0xED, 0x55)

char gStatusStringColor = EFI_GREEN;
#define STATUSSTRING "STATUS: 1234567890abcdefABCDEF"
wchar_t wcsStatusBar[sizeof(STATUSSTRING)] = { L"" };
bool fStatusBarRightAdjusted = false;
uint32_t blink;														// counter is DECERMENTED until it reaches 0, and then stops decrementing, and the STATUS gets cleared

typedef struct {
	RELPOS RelPos;
	const wchar_t* wcsMenuName;
	CTextWindow* pTextWindow;
	ABSDIM MnuDim;
	const wchar_t* rgwcsMenuItem[32];								// max. number of menu items is currently 32
	void (*rgfnMnuItm[32])(CTextWindow*, void* pContext);	// max. number of menu items is currently 32
}menu_t;

//void invalid_parameter_handler(
//	const wchar_t* expression,
//	const wchar_t* function,
//	const wchar_t* file,
//	unsigned int line,
//	uintptr_t pReserved
//) {
//
//	if (NULL != file)
//		fprintf(stderr, "invalid parameter: %S(%d) %S() %S \n", file, line, function, expression);
//	else
//	{
//		//CDETRACE((TRCERR(1) "abnormal program termination due to invalid parameter\n"));
//		//exit(3);
//		fprintf(stderr, "error");
//		__debugbreak();
//	}
//	return;
//}

int rtcrd(int idx)
{
	int nRet = 0;
	int UIP = 0;

	do {

		//
		// read UIP- update in progress first
		//
		_outp(0x70, 0xA);
		IODELAY;

		UIP = 0x80 == (0x80 & _inp(0x71));
		IODELAY;

		_outp(0x70, idx);
		IODELAY;

		nRet = _inp(0x71); IODELAY;

	} while (1 == UIP);

	return nRet;
}
//
// globally shared data
//
bool gExit = false;
bool gHexView = true;

//
// gfCfgMngXyz - global flag configuration managed XYZ
//
bool gfCfgMngMnuItm_View_Clock = true;
bool gfCfgMngMnuItm_View_Calendar = true;

/////////////////////////////////////////////////////////////////////////////
// FILE menu functions and strings
/////////////////////////////////////////////////////////////////////////////
void fnMnuItm_File_Exit(CTextWindow* pThis, void* pContext)
{
	pThis->TextPrint({ 20,20 }, "fnMnuItm_File_Exit...");
	gExit = true;
}

void fnMnuItm_File_SaveAs(CTextWindow* pThis, void* pContext)
{
	CTextWindow* pSaveAsBox;
	CTextWindow* pRoot = pThis->TextWindowGetRoot();	// get FullScreen window
	char strFileName[256] = { "" };

	pThis->TextClearWindow(pRoot->WinAtt);				// clear the pull down menu 
	pRoot->TextBlockRfrsh();							// refresh the main window

	pSaveAsBox = new CTextWindow(pThis, { pRoot->WinDim.X / 2 - 60 / 2,pRoot->WinDim.Y / 2 - 5 / 2 }, { 60,5 }, EFI_BACKGROUND_CYAN | EFI_YELLOW);

	pSaveAsBox->TextBorder(
		{ 0,0 },
		{ 60,5 },
		BOXDRAW_DOWN_RIGHT,
		BOXDRAW_DOWN_LEFT,
		BOXDRAW_UP_RIGHT,
		BOXDRAW_UP_LEFT,
		BOXDRAW_HORIZONTAL,
		BOXDRAW_VERTICAL,
		nullptr
	);

	pSaveAsBox->TextPrint({ 2,0 }, " File Save As ... ");
	pSaveAsBox->TextPrint({ 2,2 }, EFI_BACKGROUND_BLUE | EFI_WHITE, "                                                        ");

	//
	// simulate the "SAVE-AS dialog box"
	//
	if (1)
	{
		int i = 0;
		TEXT_KEY key;
		for (key = pThis->TextGetKey();
			KEY_ESC != key && KEY_ENTER != key;
			key = pThis->TextGetKey(), pThis->TextWindowUpdateProgress())
		{

			pSaveAsBox->TextPrint({ 2    ,2 }, EFI_BACKGROUND_BLUE | EFI_WHITE, strFileName);
			pSaveAsBox->TextPrint({ 2 + i,2 }, 4 & blink++ ? EFI_BACKGROUND_RED | EFI_WHITE : EFI_BACKGROUND_BLUE | EFI_WHITE, " ");//blink the cursor

			//debug pSaveAsBox->TextPrint({ 0,0 }, "%3d %04X", i, pThis->KeyData.Key.UnicodeChar);

			if (0xFFFF != pThis->KeyData.Key.UnicodeChar
				&& 0x0000 != pThis->KeyData.Key.UnicodeChar)
			{
				if ('\b'/* back space */ == (char)pThis->KeyData.Key.UnicodeChar)
					pSaveAsBox->TextPrint({ 2 + i,2 }, EFI_BACKGROUND_BLUE | EFI_WHITE, " "),	// clear old cursor
					i--;																	// adjust new curor position
				else
					strFileName[i++] = (char)pThis->KeyData.Key.UnicodeChar;

				strFileName[i] = '\0';
			}
		}

		if (KEY_ESC == key)
			wcscpy(wcsStatusBar, L"STATUS: cancled"),
			gStatusStringColor = EFI_GREEN,
			blink = 0x3C;

		if (KEY_ENTER == key) do
		{
			errno = 0;
			FILE* fp = fopen(strFileName, "r");
			bool fFileExists = (NULL != fp);	// flag file exists
			bool fCrtOvrd = !fFileExists;		// flag CreateOverride

			//
			// FILE OVERRIDE CHECK BOX - start
			//
			if (true == fFileExists)
			{
				bool fYSel = false;												// YES/NO selected := NO

				pSaveAsBox->BgAtt = EFI_BACKGROUND_LIGHTGRAY | EFI_BLACK;		// delete SAVE AS BOX
				delete pSaveAsBox;												// delete SAVE AS BOX
				pSaveAsBox = nullptr;											// prevent from second deletion below
#define CBHIGHT 5												// CheckBox height
				//#define CBWIDTH 30
				char strtmp[256];

				snprintf(strtmp, sizeof(strtmp), "\"%s\" overwrite?", strFileName);
				int CBWIDTH/* CheckBox width */ = 16 < strlen(strtmp) ? 4 + (int)strlen(strtmp) : 20;

				CTextWindow* pCheckBox = new CTextWindow(						// create Y/N CHECK BOX
					pThis,
					{ pRoot->WinDim.X / 2 - CBWIDTH / 2,pRoot->WinDim.Y / 2 - CBHIGHT / 2 },
					{ CBWIDTH,CBHIGHT },
					EFI_BACKGROUND_CYAN | EFI_YELLOW
				);
				// 00000000001111111111
				// 01234567890123456789
				// +------------------+
				// |    YES     NO    |
				// +------------------+

				pCheckBox->TextBorder(
					{ 0,0 },
					{ CBWIDTH,CBHIGHT },
					BOXDRAW_DOWN_RIGHT,
					BOXDRAW_DOWN_LEFT,
					BOXDRAW_UP_RIGHT,
					BOXDRAW_UP_LEFT,
					BOXDRAW_HORIZONTAL,
					BOXDRAW_VERTICAL,
					nullptr
				);

				pCheckBox->TextPrint({ CBWIDTH / 2 - (int)strlen(strtmp) / 2,1 }, strtmp);

				for (key = pThis->TextGetKey();
					KEY_ENTER != key && KEY_ESC != key;
					key = pThis->TextGetKey(), pThis->TextWindowUpdateProgress())
				{
					if (KEY_LEFT == key || KEY_RIGHT == key)
						fYSel ^= true;											// YES/NO selection toggle

					pCheckBox->TextPrint({ CBWIDTH / 2 - 7,CBHIGHT - 2 }, (fYSel ? EFI_BACKGROUND_GREEN : EFI_BACKGROUND_CYAN) | EFI_WHITE, "  YES  ");
					pCheckBox->TextPrint({ CBWIDTH / 2 + 1,CBHIGHT - 2 }, (fYSel ? EFI_BACKGROUND_CYAN : EFI_BACKGROUND_GREEN) | EFI_WHITE, "  NO  ");
				}
				pCheckBox->BgAtt = EFI_BACKGROUND_LIGHTGRAY | EFI_BLACK;		// delete Y/N CHECK BOX
				delete pCheckBox;												// delete Y/N CHECK BOX

				//
				// FILE OVERRIDE CHECK BOX - end
				//
				if (fYSel)
					fCrtOvrd = true;
				else {
					wcscpy(wcsStatusBar, L"STATUS: ");
					swprintf(
						&wcsStatusBar[wcslen(wcsStatusBar)],
						sizeof(STATUSSTRING) - 1,
						L"%hs",
						strerror(EACCES)
					);
					gStatusStringColor = EFI_RED;
					blink = UINT_MAX & ~3;
				}
			}

			if (fCrtOvrd)
			{
				errno = 0;
				fp = fopen(strFileName, "w+");
				fFileExists = (NULL != fp);
				swprintf(wcsStatusBar, sizeof(STATUSSTRING) - 1, L"%hs",
					fFileExists ? "STATUS: Success" : strerror(errno));
				gStatusStringColor = fFileExists ? EFI_GREEN : EFI_RED;
				blink = fFileExists ? 0x3C : UINT_MAX & ~3;
			}

		} while (0);
	}

	//RealTimeClock Analyser


	if (nullptr != pSaveAsBox)
		pSaveAsBox->BgAtt = EFI_BACKGROUND_LIGHTGRAY | EFI_BLACK,
		delete pSaveAsBox;
}

void fnMnuItm_View_HexSym(CTextWindow* pThis, void* pContext)
{
	CTextWindow* pRoot = pThis->TextWindowGetRoot();

	pRoot->TextBlockClear();
	gHexView ^= true;// toggle debug state
	pThis->TextClearWindow(pRoot->WinAtt);
}

void fnMnuItm_View_Clock(CTextWindow* pThis, void* pContext)
{
	CTextWindow* pRoot = pThis->TextWindowGetRoot();

	pRoot->TextBlockClear();
	gfCfgMngMnuItm_View_Clock ^= true;// toggle debug state
	pThis->TextClearWindow(pRoot->WinAtt);
}

void fnMnuItm_View_Calendar(CTextWindow* pThis, void* pContext)
{
	CTextWindow* pRoot = pThis->TextWindowGetRoot();

	pRoot->TextBlockClear();
	gfCfgMngMnuItm_View_Calendar ^= true;// toggle debug state
	pThis->TextClearWindow(pRoot->WinAtt);
}

/////////////////////////////////////////////////////////////////////////////
// CONFIG menu functions and strings
/////////////////////////////////////////////////////////////////////////////
//
// gfCfgMngXyz - global flag configuration managed XYZ
//
bool gfCfgMngMnuItm_Config_PicApicSelect = true;	// "TIMER: PIC" vs. "TIMER: ACPI" APIC selected by default
bool gfCfgMngMnuItm_Config_DelaySelect1 = true;	// "  PIC ddddd" / "  APIC ddddd" vs. " * PIC ddddd" / " * APIC ddddd"	selected by default
bool gfCfgMngMnuItm_Config_DelaySelect2 = false;	// "  PIC ddddd" / "  APIC ddddd" vs. " * PIC ddddd" / " * APIC ddddd"
bool gfCfgMngMnuItm_Config_DelaySelect3 = false;	// "  PIC ddddd" / "  APIC ddddd" vs. " * PIC ddddd" / " * APIC ddddd"
bool gfCfgMngMnuItm_Config_DelaySelect4 = false;	// "  PIC ddddd" / "  APIC ddddd" vs. " * PIC ddddd" / " * APIC ddddd"
bool gfCfgMngMnuItm_Config_DelaySelect5 = false;	// "  PIC ddddd" / "  APIC ddddd" vs. " * PIC ddddd" / " * APIC ddddd"

//wchar_t wcsTimerPicSelected[] = {GEOMETRICSHAPE_RIGHT_TRIANGLE, L" TIMER: PIC    "};
//wchar_t wcsTimerPicUnSlcted[] = { L"  TIMER: PIC    " };
//wchar_t wcsTimerAcpiSelected[] = { GEOMETRICSHAPE_RIGHT_TRIANGLE, L" TIMER: ACPI   " };
//wchar_t wcsTimerAcpiUnSlcted[] = { L"  TIMER: ACPI   " };

const wchar_t* wcsTimerSelectionPicStrings[2][1] =
{
	{ L"  TIMER: PIC    " },
	{ L"\x25ba TIMER: PIC    " }
};

const wchar_t* wcsTimerSelectionAcpiStrings[2][1] =
{
	{ L"  TIMER: ACPI   "},
	{ L"\x25ba TIMER: ACPI   "}
};
const wchar_t* wcsTimerDelayPicStrings[2][5] =
{
	{L"  PIC delay1    ",L"  PIC delay2    ",L"  PIC delay3    ",L"  PIC delay4    ",L"  PIC delay5    "},
	{L"* PIC delay1    ",L"* PIC delay2    ",L"* PIC delay3    ",L"* PIC delay4    ",L"* PIC delay5    "},
};
const wchar_t* wcsTimerDelayApicStrings[2][5] =
{
	{L"  ACPI delay1   ",L"  ACPI delay2   ",L"  ACPI delay3   ",L"  ACPI delay4   ",L"  ACPI delay5   "},
	{L"* ACPI delay1   ",L"* ACPI delay2   ",L"* ACPI delay3   ",L"* ACPI delay4   ",L"* ACPI delay5   "},
};

void fnMnuItm_Timer_1(CTextWindow* pThis, void* pContext)
{

	CTextWindow* pRoot = pThis->TextWindowGetRoot();
	menu_t* pMenu = (menu_t*)pContext;

	gfCfgMngMnuItm_Config_PicApicSelect = 0;

	pMenu->rgwcsMenuItem[0] = (wchar_t*)wcsTimerSelectionAcpiStrings[gfCfgMngMnuItm_Config_PicApicSelect][0];//wcsTimerAcpiUnSlcted;
	pMenu->rgwcsMenuItem[1] = (wchar_t*)wcsTimerSelectionPicStrings[!gfCfgMngMnuItm_Config_PicApicSelect][0];//wcsTimerPicSelected;

	pMenu->rgwcsMenuItem[3/* index 3 */] = (wchar_t*)wcsTimerDelayPicStrings[gfCfgMngMnuItm_Config_DelaySelect1][0];
	pMenu->rgwcsMenuItem[4/* index 4 */] = (wchar_t*)wcsTimerDelayPicStrings[gfCfgMngMnuItm_Config_DelaySelect2][1];
	pMenu->rgwcsMenuItem[5/* index 5 */] = (wchar_t*)wcsTimerDelayPicStrings[gfCfgMngMnuItm_Config_DelaySelect3][2];
	pMenu->rgwcsMenuItem[6/* index 6 */] = (wchar_t*)wcsTimerDelayPicStrings[gfCfgMngMnuItm_Config_DelaySelect4][3];
	pMenu->rgwcsMenuItem[7/* index 7 */] = (wchar_t*)wcsTimerDelayPicStrings[gfCfgMngMnuItm_Config_DelaySelect5][4];

	pThis->TextClearWindow(pRoot->WinAtt);	// delete menu window on exit
}

void fnMnuItm_Timer_0(CTextWindow* pThis, void* pContext)
{
	CTextWindow* pRoot = pThis->TextWindowGetRoot();
	menu_t* pMenu = (menu_t*)pContext;

	//pMenu->rgwcsMenuItem[0] = wcsTimerAcpiSelected;
	//pMenu->rgwcsMenuItem[1] = wcsTimerPicUnSlcted;
	//gfCfgMngMnuItm_Config_PicApicSelect = 1;

	gfCfgMngMnuItm_Config_PicApicSelect = 1;

	pMenu->rgwcsMenuItem[0] = (wchar_t*)wcsTimerSelectionAcpiStrings[gfCfgMngMnuItm_Config_PicApicSelect][0];
	pMenu->rgwcsMenuItem[1] = (wchar_t*)wcsTimerSelectionPicStrings[!gfCfgMngMnuItm_Config_PicApicSelect][0];

	pMenu->rgwcsMenuItem[3/* index 3 */] = (wchar_t*)wcsTimerDelayApicStrings[gfCfgMngMnuItm_Config_DelaySelect1][0];
	pMenu->rgwcsMenuItem[4/* index 3 */] = (wchar_t*)wcsTimerDelayApicStrings[gfCfgMngMnuItm_Config_DelaySelect2][1];
	pMenu->rgwcsMenuItem[5/* index 3 */] = (wchar_t*)wcsTimerDelayApicStrings[gfCfgMngMnuItm_Config_DelaySelect3][2];
	pMenu->rgwcsMenuItem[6/* index 3 */] = (wchar_t*)wcsTimerDelayApicStrings[gfCfgMngMnuItm_Config_DelaySelect4][3];
	pMenu->rgwcsMenuItem[7/* index 3 */] = (wchar_t*)wcsTimerDelayApicStrings[gfCfgMngMnuItm_Config_DelaySelect5][4];

	pThis->TextClearWindow(pRoot->WinAtt);	// delete menu window on exit
}

void fnMnuItm_Timer_2(CTextWindow* pThis, void* pContext) { CTextWindow* pRoot = pThis->TextWindowGetRoot(); menu_t* pMenu = (menu_t*)pContext;	gfCfgMngMnuItm_Config_DelaySelect1 ^= 1;	pMenu->rgwcsMenuItem[3/* index 3 */] = (wchar_t*)(0 == gfCfgMngMnuItm_Config_PicApicSelect ? wcsTimerDelayPicStrings[gfCfgMngMnuItm_Config_DelaySelect1][0] : wcsTimerDelayApicStrings[gfCfgMngMnuItm_Config_DelaySelect1][0]);	pThis->TextClearWindow(pRoot->WinAtt); }
void fnMnuItm_Timer_3(CTextWindow* pThis, void* pContext) { CTextWindow* pRoot = pThis->TextWindowGetRoot(); menu_t* pMenu = (menu_t*)pContext;	gfCfgMngMnuItm_Config_DelaySelect2 ^= 1;	pMenu->rgwcsMenuItem[4/* index 4 */] = (wchar_t*)(0 == gfCfgMngMnuItm_Config_PicApicSelect ? wcsTimerDelayPicStrings[gfCfgMngMnuItm_Config_DelaySelect2][0] : wcsTimerDelayApicStrings[gfCfgMngMnuItm_Config_DelaySelect2][0]);	pThis->TextClearWindow(pRoot->WinAtt); }
void fnMnuItm_Timer_4(CTextWindow* pThis, void* pContext) { CTextWindow* pRoot = pThis->TextWindowGetRoot(); menu_t* pMenu = (menu_t*)pContext;	gfCfgMngMnuItm_Config_DelaySelect3 ^= 1;	pMenu->rgwcsMenuItem[5/* index 5 */] = (wchar_t*)(0 == gfCfgMngMnuItm_Config_PicApicSelect ? wcsTimerDelayPicStrings[gfCfgMngMnuItm_Config_DelaySelect3][0] : wcsTimerDelayApicStrings[gfCfgMngMnuItm_Config_DelaySelect3][0]);	pThis->TextClearWindow(pRoot->WinAtt); }
void fnMnuItm_Timer_5(CTextWindow* pThis, void* pContext) { CTextWindow* pRoot = pThis->TextWindowGetRoot(); menu_t* pMenu = (menu_t*)pContext;	gfCfgMngMnuItm_Config_DelaySelect4 ^= 1;	pMenu->rgwcsMenuItem[6/* index 6 */] = (wchar_t*)(0 == gfCfgMngMnuItm_Config_PicApicSelect ? wcsTimerDelayPicStrings[gfCfgMngMnuItm_Config_DelaySelect4][0] : wcsTimerDelayApicStrings[gfCfgMngMnuItm_Config_DelaySelect4][0]);	pThis->TextClearWindow(pRoot->WinAtt); }
void fnMnuItm_Timer_6(CTextWindow* pThis, void* pContext) { CTextWindow* pRoot = pThis->TextWindowGetRoot(); menu_t* pMenu = (menu_t*)pContext;	gfCfgMngMnuItm_Config_DelaySelect5 ^= 1;	pMenu->rgwcsMenuItem[7/* index 7 */] = (wchar_t*)(0 == gfCfgMngMnuItm_Config_PicApicSelect ? wcsTimerDelayPicStrings[gfCfgMngMnuItm_Config_DelaySelect5][0] : wcsTimerDelayApicStrings[gfCfgMngMnuItm_Config_DelaySelect5][0]);	pThis->TextClearWindow(pRoot->WinAtt); }


/////////////////////////////////////////////////////////////////////////////
// About - BOX
/////////////////////////////////////////////////////////////////////////////
void fnMnuItm_About_0(CTextWindow* pThis, void* pContext)
{
	CTextWindow* pAboutBox;
	CTextWindow* pRoot = pThis->TextWindowGetRoot();

	pThis->TextClearWindow(pRoot->WinAtt);	// clear the pull down menu 
	pRoot->TextBlockRfrsh();	// refresh the main window

	if (1)// center coordinate calculation
	{

	}
	pAboutBox = new CTextWindow(pThis, { pRoot->WinDim.X / 2 - 60 / 2,pRoot->WinDim.Y / 2 - 20 / 2 }, { 60,20 }, EFI_BACKGROUND_CYAN | EFI_YELLOW);

	pAboutBox->TextBorder(
		{ 0,0 },
		{ 60,20 },
		BOXDRAW_DOWN_RIGHT,
		BOXDRAW_DOWN_LEFT,
		BOXDRAW_UP_RIGHT,
		BOXDRAW_UP_LEFT,
		BOXDRAW_HORIZONTAL,
		BOXDRAW_VERTICAL,
		nullptr
	);

	pAboutBox->TextPrint({ pAboutBox->WinDim.X / 2 - (int32_t)strlen("TSCSyncDemo+") / 2,2 }, "TSCSyncDemo+");
	pAboutBox->TextPrint({ pAboutBox->WinDim.X / 2 - (int32_t)strlen("RealTimeClock Analyser") / 2,5 }, "RealTimeClock Analyser");

	//RealTimeClock Analyser

	for (TEXT_KEY key = NO_KEY; KEY_ESC != key && KEY_ENTER != key; key = pThis->TextGetKey(), pThis->TextWindowUpdateProgress())
		;

	pAboutBox->BgAtt = EFI_BACKGROUND_LIGHTGRAY | EFI_BLACK;
	delete pAboutBox;
}

void fnMnuItm_About_1(CTextWindow* pThis, void* pContext)
{
	CTextWindow* pRoot = pThis->TextWindowGetRoot();
	gfKbdDbg ^= true;// toggle debug state

	if (false == gfKbdDbg)
	{
		char* pLineKiller = new char[pThis->ScrDim.X - 4];

		memset(pLineKiller, '\x20', pThis->ScrDim.X - 4);
		pLineKiller[pThis->ScrDim.X - 4] = '\0';

		pThis->GotoXY({ 2,pThis->ScrDim.Y - 3 });
		printf("%s", pLineKiller);
	}

	pThis->TextClearWindow(pRoot->WinAtt);
}

void resetconsole(void)
{
	gSystemTable->ConOut->EnableCursor(gSystemTable->ConOut, 1);
	gSystemTable->ConOut->SetAttribute(gSystemTable->ConOut, EFI_BACKGROUND_BLACK + EFI_WHITE);
}

int main(int argc, char** argv)
{
	int nRet = 1;
	gSystemTable = (EFI_SYSTEM_TABLE*)(argv[-1]);		//SystemTable is passed in argv[-1]
	gImageHandle = (void*)(argv[-2]);					//ImageHandle is passed in argv[-2]
	TEXT_KEY key = NO_KEY;

	DPRINTF(("hello\n"));
	atexit(resetconsole);
	EFI_ACPI_6_2_FIXED_ACPI_DESCRIPTION_TABLE* pFACP = (EFI_ACPI_6_2_FIXED_ACPI_DESCRIPTION_TABLE*)malloc(32768 * 8);
	uint32_t DataSize2;
#pragma pack(1)
	typedef struct {
		EFI_ACPI_DESCRIPTION_HEADER                       Header;
		UINT64                                            Reserved0;
		UINT64  BaseAddress;
		UINT16  PciSegmentGroupNumber;
		UINT8   StartBusNumber;
		UINT8   EndBusNumber;
		UINT32  Reserved;
	}CDE_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE;
	CDE_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE* pMCFG = (CDE_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE*)malloc(32768 * 8);
#pragma pack()

	//	_set_invalid_parameter_handler(invalid_parameter_handler);

	DataSize2 = GetSystemFirmwareTable('ACPI', 'PCAF', pFACP, 32768 * 8);
	//pFACP = (void*)pFACP;

	printf("Signature: %.4s, OemID: %.6s, OemTblID %.8s\n",
		(char*)&pFACP->Header.Signature,
		&pFACP->Header.OemId,
		(char*)&pFACP->Header.OemTableId
	);

	printf("PmTmrBlk: %04X, %sBit\n",
		pFACP->PmTmrBlk,
		(0 != (pFACP->Flags & (1 << 8))) ? "32" : "24"
	);

	DataSize2 = GetSystemFirmwareTable('ACPI', 'GFCM', pMCFG, 32768 * 8);
	printf("DataSize2 %d\nPCIEBase %p\nsizeof(CDE_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE) %zd\n", DataSize2, (void*)pMCFG->BaseAddress, sizeof(CDE_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE));

	//
	//	determine processor speed within 250ms second
	//
	if (1)
	{
		char buffer[128] = "Determining processor speed ...";
		//printf("%s", buffer);

		clock_t endCLK = CLOCKS_PER_SEC / 4 + clock();
		uint64_t endTSC, startTSC = __rdtsc();

		while (endCLK > clock())
			;
		endTSC = __rdtsc();

		sprintf(buffer, "%lld", 4 * (endTSC - startTSC));

		printf("--> %c.%c%cGHz\n", buffer[0], buffer[1], buffer[2]);
	}
	//
	// getting config data from 
	//
	if (1)
	{
		FILE* fp = fopen("welcome14.cfg", "r");

		if (nullptr != fp)
		{
			printf("reading cfg file\n");
			int tok = fscanf(fp, "gfCfgMngMnuItm_View_Clock = %hhu\ngfCfgMngMnuItm_View_Calendar = %hhu\ngfCfgMngMnuItm_Config_PicApicSelect = %hhu\ngfCfgMngMnuItm_Config_DelaySelect1 = %hhu\ngfCfgMngMnuItm_Config_DelaySelect2 = %hhu\ngfCfgMngMnuItm_Config_DelaySelect3 = %hhu\ngfCfgMngMnuItm_Config_DelaySelect4 = %hhu\ngfCfgMngMnuItm_Config_DelaySelect5 = %hhu",
				(char*)&gfCfgMngMnuItm_View_Clock,
				(char*)&gfCfgMngMnuItm_View_Calendar,
				(char*)&gfCfgMngMnuItm_Config_PicApicSelect,
				(char*)&gfCfgMngMnuItm_Config_DelaySelect1,
				(char*)&gfCfgMngMnuItm_Config_DelaySelect2,
				(char*)&gfCfgMngMnuItm_Config_DelaySelect3,
				(char*)&gfCfgMngMnuItm_Config_DelaySelect4,
				(char*)&gfCfgMngMnuItm_Config_DelaySelect5
			);

			printf("tok %d\ngfCfgMngMnuItm_View_Clock %d\ngfCfgMngMnuItm_View_Calendar %d\n", tok, gfCfgMngMnuItm_View_Clock, gfCfgMngMnuItm_View_Calendar);
		}
	}

	printf("sizeof(bool) %zd\n", sizeof(bool));

	do
	{
		char* pc = new char[256];
		int* pi = new int(12345678);
		static wchar_t wcsSeparator17[] = { BOXDRAW_HORIZONTAL,BOXDRAW_HORIZONTAL,BOXDRAW_HORIZONTAL,BOXDRAW_HORIZONTAL,BOXDRAW_HORIZONTAL,BOXDRAW_HORIZONTAL,BOXDRAW_HORIZONTAL,BOXDRAW_HORIZONTAL, BOXDRAW_HORIZONTAL, BOXDRAW_HORIZONTAL, BOXDRAW_HORIZONTAL, BOXDRAW_HORIZONTAL, BOXDRAW_HORIZONTAL, BOXDRAW_HORIZONTAL, BOXDRAW_HORIZONTAL, BOXDRAW_HORIZONTAL, '\0' };

		menu_t menu[] =
		{
			{{04,0},	L" File ",		nullptr,{20,4/* # menuitems + 2 */},	/*{false},*/ {L"Save As         ",L"Exit            "},{&fnMnuItm_File_SaveAs, &fnMnuItm_File_Exit}},
			{{12,0},	L" View ",		nullptr,{20,5/* # menuitems + 2 */},	/*{false},*/ {L"Hex/Sym view    ",L"Clock           ",L"Calendar        " },{&fnMnuItm_View_HexSym,&fnMnuItm_View_Clock,&fnMnuItm_View_Calendar}},
			{{20,0},	L" Help ",		nullptr,{20,4/* # menuitems + 2 */},	/*{false, false},*/ {L"About           ",L"KEYBOARD DEBUG  "},{&fnMnuItm_About_0, &fnMnuItm_About_1 }},
			{{28,0},	L" CONFIG ",	nullptr,{20,10/* # menuitems + 2 */},	/*{false, false, true, false},*/
				{
					/*index 0 */ wcsTimerSelectionAcpiStrings[gfCfgMngMnuItm_Config_PicApicSelect][0]		/*L"  TIMER: ACPI   "*//* selected by default */,
					/*index 1 */ wcsTimerSelectionPicStrings[!gfCfgMngMnuItm_Config_PicApicSelect][0]		/*L"  TIMER: PIC    "*/,
					/*index 2 */ wcsSeparator17,
					/*index 3 */ wcsTimerDelayApicStrings[gfCfgMngMnuItm_Config_DelaySelect1][0],	/* selected by default menu strings */
					/*index 4 */ wcsTimerDelayApicStrings[gfCfgMngMnuItm_Config_DelaySelect2][1],
					/*index 5 */ wcsTimerDelayApicStrings[gfCfgMngMnuItm_Config_DelaySelect3][2],
					/*index 6 */ wcsTimerDelayApicStrings[gfCfgMngMnuItm_Config_DelaySelect4][3],
					/*index 7 */ wcsTimerDelayApicStrings[gfCfgMngMnuItm_Config_DelaySelect5][4],
				},
				{	/*index 0 */ &fnMnuItm_Timer_0,
				/*index 1 */ &fnMnuItm_Timer_1,
				/*index 2 */ nullptr/* nullptr identifies SEPARATOR */,
				/*index 3 */ &fnMnuItm_Timer_2,
				/*index 4 */ &fnMnuItm_Timer_3,
				/*index 5 */ &fnMnuItm_Timer_4,
				/*index 6 */ &fnMnuItm_Timer_5,
				/*index 7 */ &fnMnuItm_Timer_6,
			}
		}
		};

		CTextWindow FullScreen(nullptr/* no parent pointer - this is the main window !!! */, DFLT_SCREEN_ATTRIBS);

		//
		// draw the border
		//
		if (1)
		{
			wchar_t wcsTitle[] = { BOXDRAW_VERTICAL_LEFT_DOUBLE,BLOCKELEMENT_LIGHT_SHADE ,BLOCKELEMENT_LIGHT_SHADE ,BLOCKELEMENT_LIGHT_SHADE ,BLOCKELEMENT_LIGHT_SHADE ,BLOCKELEMENT_LIGHT_SHADE ,BLOCKELEMENT_LIGHT_SHADE ,
				0x20,L'T',L'S',L'C',L'S',L'y',L'n',L'c',L'D',L'e',L'm',L'o',L'+',0x20,
				BLOCKELEMENT_LIGHT_SHADE ,BLOCKELEMENT_LIGHT_SHADE ,BLOCKELEMENT_LIGHT_SHADE ,BLOCKELEMENT_LIGHT_SHADE ,BLOCKELEMENT_LIGHT_SHADE ,BLOCKELEMENT_LIGHT_SHADE,BOXDRAW_VERTICAL_RIGHT_DOUBLE,'\0' };


			FullScreen.TextBorder(
				{ 0,1 },
				{ FullScreen.WinDim.X, FullScreen.WinDim.Y - 2/*FullScreen.WinDim.Y*/ },
				//{ FullScreen.WinPos.X-10,FullScreen.WinPos.Y - 11 },
				BOXDRAW_DOUBLE_DOWN_RIGHT,
				BOXDRAW_DOUBLE_DOWN_LEFT,
				BOXDRAW_DOUBLE_UP_RIGHT,
				BOXDRAW_DOUBLE_UP_LEFT,
				BOXDRAW_DOUBLE_HORIZONTAL,
				BOXDRAW_DOUBLE_VERTICAL,
				&wcsTitle[0]
			);
		}
		//
		// draw the menu strings
		//
		for (int i = 0; i < ELC(menu); i++)		// initialize the menu strings
			FullScreen.TextPrint(menu[i].RelPos, menu[i].wcsMenuName);

		//
		// draw status/help line
		//
		if (1)
		{
			wchar_t wcsARROW_LEFT[2] = { ARROW_LEFT ,'\0' },
				wcsARROW_UP[2] = { ARROW_UP ,'\0' },
				wcsARROW_RIGHT[2] = { ARROW_RIGHT ,'\0' },
				wcsARROW_DOWN[2] = { ARROW_DOWN ,'\0' };
			FullScreen.TextPrint({ 1, FullScreen.ScrDim.Y - 1 }, EFI_BACKGROUND_BLUE | EFI_WHITE, L"F10:Menu ENTER:Select ESC:Return %s%s%s%s:Navigate", wcsARROW_LEFT, wcsARROW_RIGHT, wcsARROW_UP, wcsARROW_DOWN);
		}

		//
		// the master loop
		//
		if (1)
		{
			enum STATE {
				MENU_DFLT,
				MENU_ENTER_ACTIVATION,
				MENU_IS_ACTIVE,
				MENU_IS_OPEN,
				/*MENU_123_IS_ACTIVE,*/
			}state = MENU_DFLT;
			int idxMenu = 0, idxMnuItm = 0;
			int pgress = 0;

			for (; false == gExit;)
			{

				//clock_t endclk = CLOCKS_PER_SEC / 8 + clock();
				//EFI_KEY_DATA KeyData = FullScreen.ReadKeyStrokeEx();

				FullScreen.TextWindowUpdateProgress();

				//
				// write STATUS BAR, if a status message exist in wcsStatusBar
				//
				if (0 != blink) {

					if (false == fStatusBarRightAdjusted)	// right adjust whatever is in the wcsStatusBar
					{
						wchar_t wcstmp[sizeof(wcsStatusBar)];
						wchar_t wcsFmt[32];

						wcscpy(wcstmp, wcsStatusBar);
						swprintf(wcsFmt, L"%%%ds", (int)ELC(wcsStatusBar) - 1);
						swprintf(wcsStatusBar, wcsFmt, wcstmp);						// do the right-justification!
						fStatusBarRightAdjusted = true;
					}

					if (0 == (blink-- & 3))
					{
						if (0 != wcscmp(wcsStatusBar, L""))
						{
							FullScreen.TextPrint({ FullScreen.ScrDim.X - (int32_t)ELC(wcsStatusBar), FullScreen.ScrDim.Y - 1 },
								EFI_BACKGROUND_BLUE | (blink & 4 ? gStatusStringColor : EFI_BLUE), wcsStatusBar);
						}
					}
				}
				else if (0 != wcscmp(wcsStatusBar, L""))
				{
					fStatusBarRightAdjusted = false;
					wmemset(wcsStatusBar, L'\x20', ELC(wcsStatusBar));																		// clear status bar message
					wcsStatusBar[-1 + ELC(wcsStatusBar)] = L'\0';																			// terminate string
					FullScreen.TextPrint({ FullScreen.ScrDim.X - 1 - (int32_t)sizeof(STATUSSTRING), FullScreen.ScrDim.Y - 1 },
						EFI_BACKGROUND_BLUE | EFI_WHITE, wcsStatusBar);																		// write cleared status bar message
					wcscpy(wcsStatusBar, L"");																								// cut status bar message to 0
				}

				key = FullScreen.TextGetKey();

				switch (state) {
				case MENU_ENTER_ACTIVATION:
					FullScreen.TextPrint(menu[idxMenu].RelPos, EFI_BACKGROUND_CYAN | EFI_YELLOW, menu[idxMenu].wcsMenuName);
					//
					// clear STATUS BAR by setting blink:=0
					//
					blink = 0;

					state = MENU_IS_ACTIVE;
					break;
				case MENU_IS_ACTIVE:
					if (KEY_ESC == key) {
						key = NO_KEY;
						state = MENU_DFLT;
						for (int i = 0; i < ELC(menu); i++)		// normalize the menu strings
							FullScreen.TextPrint(menu[i].RelPos, EFI_BACKGROUND_LIGHTGRAY | EFI_BLACK, menu[i].wcsMenuName);
					}
					else if (KEY_LEFT == key) {
						FullScreen.TextPrint(menu[idxMenu].RelPos, EFI_BACKGROUND_LIGHTGRAY | EFI_BLACK, menu[idxMenu].wcsMenuName);
						idxMenu = ((idxMenu - 1) < 0) ? ELC(menu) - 1 : idxMenu - 1;
						FullScreen.TextPrint(menu[idxMenu].RelPos, EFI_BACKGROUND_CYAN | EFI_YELLOW, menu[idxMenu].wcsMenuName);
						key = NO_KEY;
					}
					else if (KEY_RIGHT == key) {
						FullScreen.TextPrint(menu[idxMenu].RelPos, EFI_BACKGROUND_LIGHTGRAY | EFI_BLACK, menu[idxMenu].wcsMenuName);
						idxMenu = ((idxMenu + 1) < ELC(menu)) ? idxMenu + 1 : 0;
						FullScreen.TextPrint(menu[idxMenu].RelPos, EFI_BACKGROUND_CYAN | EFI_YELLOW, menu[idxMenu].wcsMenuName);
						key = NO_KEY;
					}
					else if (KEY_ENTER == key) {
						menu[idxMenu].pTextWindow = new CTextWindow(&FullScreen, { menu[idxMenu].RelPos.X, 2 }, menu[idxMenu].MnuDim, EFI_BACKGROUND_CYAN | EFI_YELLOW);
						menu[idxMenu].pTextWindow->TextBorder({ 0, 0 }, menu[idxMenu].MnuDim,
							BOXDRAW_DOWN_RIGHT,
							BOXDRAW_DOWN_LEFT,
							BOXDRAW_UP_RIGHT,
							BOXDRAW_UP_LEFT,
							BOXDRAW_HORIZONTAL,
							BOXDRAW_VERTICAL,
							nullptr);

						//
						// fill menu with menuitem strings
						//
						menu[idxMenu].pTextWindow->pwcsBlockDrawBuf[0] = '\0';	// initially terminate the string list

						for (int i = 0; /* NOTE: check for NULL string to terminate the list */menu[idxMenu].rgwcsMenuItem[i]; i++)
						{
							wchar_t* wcsList = menu[idxMenu].pTextWindow->pwcsBlockDrawBuf;
							size_t x = wcslen(wcsList);								// always get end of strings

							swprintf(&wcsList[x], UINT_MAX, L"%s\n", menu[idxMenu].rgwcsMenuItem[i]);
						}
						menu[idxMenu].pTextWindow->TextBlockDraw({ 2, 1 });
						idxMnuItm = 0;
						menu[idxMenu].pTextWindow->TextPrint({ 2,1 }, EFI_BACKGROUND_MAGENTA | EFI_YELLOW, menu[idxMenu].rgwcsMenuItem[0]);
						//__debugbreak();

						state = MENU_IS_OPEN;
						key = NO_KEY;
					}
					break;
				case MENU_IS_OPEN:
					if (KEY_ESC == key) {
						menu[idxMenu].pTextWindow->BgAtt = EFI_BACKGROUND_LIGHTGRAY | EFI_BLACK; // set background attribute
						delete menu[idxMenu].pTextWindow;
						FullScreen.TextBlockRfrsh();
						state = MENU_IS_ACTIVE;
					}
					else if (KEY_ENTER == key)
					{
						menu[idxMenu].pTextWindow->BgAtt = EFI_BACKGROUND_LIGHTGRAY | EFI_BLACK; // set background attribute
						(*menu[idxMenu].rgfnMnuItm[idxMnuItm])(menu[idxMenu].pTextWindow, &menu[idxMenu]/*nullptr*/);
						for (int i = 0; i < ELC(menu); i++)		// normalize the menu strings
							FullScreen.TextPrint(menu[i].RelPos, EFI_BACKGROUND_LIGHTGRAY | EFI_BLACK, menu[i].wcsMenuName);
						state = MENU_DFLT;
					}
					else if (KEY_DOWN == key) {
						int idxMnuItmNUM = menu[idxMenu].MnuDim.Y - 2/* number of lines within the pulldown menu */;

						menu[idxMenu].pTextWindow->TextPrint({ 2,idxMnuItm + 1 }, EFI_BACKGROUND_CYAN | EFI_YELLOW, menu[idxMenu].rgwcsMenuItem[idxMnuItm]);	// de-highlight previous menu item
						do {
							idxMnuItm = (++idxMnuItm == idxMnuItmNUM ? 0 : idxMnuItm);
						} while (nullptr == menu[idxMenu].rgfnMnuItm[idxMnuItm]);	// skip separators
						menu[idxMenu].pTextWindow->TextPrint({ 2,idxMnuItm + 1 }, EFI_BACKGROUND_MAGENTA | EFI_YELLOW, menu[idxMenu].rgwcsMenuItem[idxMnuItm]);	//    highlight current  menu item
					}
					else if (KEY_UP == key) {
						int idxMnuItmNUM = menu[idxMenu].MnuDim.Y - 2/* number of lines within the pulldown menu */;

						menu[idxMenu].pTextWindow->TextPrint({ 2,idxMnuItm + 1 }, EFI_BACKGROUND_CYAN | EFI_YELLOW, menu[idxMenu].rgwcsMenuItem[idxMnuItm]);	// de-highlight previous menu item
						do {
							idxMnuItm = (--idxMnuItm < 0 ? idxMnuItmNUM - 1 : idxMnuItm);
						} while (nullptr == menu[idxMenu].rgfnMnuItm[idxMnuItm]);	// skip separators
						menu[idxMenu].pTextWindow->TextPrint({ 2,idxMnuItm + 1 }, EFI_BACKGROUND_MAGENTA | EFI_YELLOW, menu[idxMenu].rgwcsMenuItem[idxMnuItm]);	//    highlight current  menu item
					}

					key = NO_KEY;
					break;
				case MENU_DFLT:
					if (KEY_F10 == key)
						state = MENU_ENTER_ACTIVATION,
						key = NO_KEY;
					if (true == gHexView)
					{
						//	//
						//	//	refresh the FullScreen Window
						//	//
						//	FullScreen.pwcsBlockDrawBuf[0] = '\0';
						//	for (int i = 0; i < 128 / TYPESIZE; i++)
						//		swprintf(&FullScreen.pwcsBlockDrawBuf[wcslen(FullScreen.pwcsBlockDrawBuf)],
						//			(const size_t)8192,
						//			(i % (16 / TYPESIZE) == 0 ? FORMATW_ADDR : FORMATWOADDR),
						//			(i % (16 / TYPESIZE) == 0 ? (void*)(i * TYPESIZE) : L""),
						//			TYPEMASK & RTCRD(i * TYPESIZE),
						//			((i + 1) % (16 / TYPESIZE)) ? (((i + 1) % (8 / TYPESIZE)) ? L" " : L" - ") : L"\n");

						//	FullScreen.TextBlockDraw({ FullScreen.WinPos.X + 3, FullScreen.WinPos.Y + 3 });
					}
					else {
						////
						//// NOTE:The RTC handling below is TOO SIMPLE, because it IGNORES the UIP Update In Progress phase. 
						////      During that phase registers (calendar registers) have indetermined values.
						////      For that reason "%" division and string-length-equality is done for indexed strings.
						////
						//static char* wday_name_long[7] = {
						//	"Sunday",
						//	"Monday",
						//	"Tuesday",
						//	"Wednesday",
						//	"Thursday",
						//	"Friday",
						//	"Saturday" };
						//static char* mon_name_long[12] = {
						//	"January",
						//	"February",
						//	"March",
						//	"April",
						//	"May",
						//	"June",
						//	"July",
						//	"August",
						//	"September",
						//	"October",
						//	"November",
						//	"December" };

						//FullScreen.pwcsBlockDrawBuf[0] = '\0';
						//#define ENDOFDRAWBUF &FullScreen.pwcsBlockDrawBuf[wcslen(FullScreen.pwcsBlockDrawBuf)]

						//swprintf(ENDOFDRAWBUF, (const size_t)INT_MAX,
						//	L"Date/Time: %hs, %X. %hs %02X%02X, %02X:%02X,%02X \n",
						//	wday_name_long[(rtcrd(6)) % 7],
						//	rtcrd(7),
						//	mon_name_long[-1 + (rtcrd(8)) % 12],
						//	rtcrd(0x32), rtcrd(9),
						//	0xFF & rtcrd(4),
						//	0xFF & rtcrd(2),
						//	0xFF & rtcrd(0)
						//);
						//swprintf(ENDOFDRAWBUF, (const size_t)INT_MAX,
						//	L" \nAlarm    : %02X:%02X,%02X \n", 0xFF & rtcrd(5), 0xFF & rtcrd(3), 0xFF & rtcrd(1));

						////
						//// register A
						////
						//typedef union _regA {
						//	uint8_t reg;
						//	struct {
						//		uint8_t RegA_RS0 : 1;
						//		uint8_t RegA_RS1 : 1;
						//		uint8_t RegA_RS2 : 1;
						//		uint8_t RegA_RS3 : 1;
						//		uint8_t RegA_DV0 : 1;
						//		uint8_t RegA_DV1 : 1;
						//		uint8_t RegA_DV2 : 1;
						//		uint8_t RegA_UIP : 1;
						//	}field;
						//}REGA;
						//REGA RegA;
						//RegA.reg = (uint8_t)RTCRD(0x0A);
						//swprintf(ENDOFDRAWBUF, INT_MAX, L" \nRegister A:\n    UIP %d, DV2 %d, DV1 %d, DV0 %d, RS3 %d, RS2 %d, RS1 %d, RS0 %d\n",
						//	RegA.field.RegA_RS0,
						//	RegA.field.RegA_RS1,
						//	RegA.field.RegA_RS2,
						//	RegA.field.RegA_RS3,
						//	RegA.field.RegA_DV0,
						//	RegA.field.RegA_DV1,
						//	RegA.field.RegA_DV2,
						//	RegA.field.RegA_UIP);

						////
						//// register B
						////
						//typedef union _regB {
						//	uint8_t reg;
						//	struct {
						//		uint8_t RegB_DSE : 1;
						//		uint8_t RegB_C2412 : 1;
						//		uint8_t RegB_DM : 1;
						//		uint8_t RegB_SQWE : 1;
						//		uint8_t RegB_UIE : 1;
						//		uint8_t RegB_AIE : 1;
						//		uint8_t RegB_PIE : 1;
						//		uint8_t RegB_SET : 1;
						//	}field;
						//}REGB;
						//REGB RegB;
						//RegB.reg = (uint8_t)RTCRD(0x0B);
						//swprintf(ENDOFDRAWBUF, INT_MAX, L" \nRegister B:\n    SET %d, PIE %d, AIE %d, UIE %d, SQWE %d, DM %d, 24/12 %d, DSE %d\n",
						//	RegB.field.RegB_SET,
						//	RegB.field.RegB_PIE,
						//	RegB.field.RegB_AIE,
						//	RegB.field.RegB_UIE,
						//	RegB.field.RegB_SQWE,
						//	RegB.field.RegB_DM,
						//	RegB.field.RegB_C2412,
						//	RegB.field.RegB_DSE);
						////
						//// register C
						////
						//typedef union _regC {
						//	uint8_t reg;
						//	struct {
						//		uint8_t RegC_B0 : 1;
						//		uint8_t RegC_B1 : 1;
						//		uint8_t RegC_B2 : 1;
						//		uint8_t RegC_B3 : 1;
						//		uint8_t RegC_UF : 1;
						//		uint8_t RegC_AF : 1;
						//		uint8_t RegC_PF : 1;
						//		uint8_t RegC_IRQF : 1;
						//	}field;
						//}REGC;
						//REGC RegC;
						//RegC.reg = (uint8_t)RTCRD(0x0C);
						//swprintf(ENDOFDRAWBUF, INT_MAX, L" \nRegister C:\n    IRQF %d, PF %d, AF %d, UF %d, B3 %d, B2 %d, B1 %d, B0 %d\n",
						//	RegC.field.RegC_IRQF,
						//	RegC.field.RegC_PF,
						//	RegC.field.RegC_AF,
						//	RegC.field.RegC_UF,
						//	RegC.field.RegC_B3,
						//	RegC.field.RegC_B2,
						//	RegC.field.RegC_B1,
						//	RegC.field.RegC_B0);
						////
						//// register D
						////
						//typedef union _regD {
						//	uint8_t reg;
						//	struct {
						//		uint8_t RegD_B0 : 1;
						//		uint8_t RegD_B1 : 1;
						//		uint8_t RegD_B2 : 1;
						//		uint8_t RegD_B3 : 1;
						//		uint8_t RegD_B4 : 1;
						//		uint8_t RegD_B5 : 1;
						//		uint8_t RegD_B6 : 1;
						//		uint8_t RegD_VRT : 1;
						//	}field;
						//}REGD;
						//REGD RegD;
						//RegD.reg = (uint8_t)RTCRD(0x0D);
						//swprintf(ENDOFDRAWBUF, INT_MAX, L" \nRegister D:\n    VRT %d, B6 %d, B5 %d, B4 %d, B3 %d, B2 %d, B1 %d, B0 %d\n",
						//	RegD.field.RegD_VRT,
						//	RegD.field.RegD_B6,
						//	RegD.field.RegD_B5,
						//	RegD.field.RegD_B4,
						//	RegD.field.RegD_B3,
						//	RegD.field.RegD_B2,
						//	RegD.field.RegD_B1,
						//	RegD.field.RegD_B0);
						//FullScreen.TextBlockDraw({ FullScreen.WinPos.X + 3, FullScreen.WinPos.Y + 3 });
					}
					break;
				default:break;
				}

				key = NO_KEY;
			}
		}

		delete pc;
		delete pi;
		DPRINTF(("...exit0"));

	} while (false == gExit);

	//
	// save modified config, always modified or not
	//
	if (1)
	{
		FILE* fp = fopen("welcome14.cfg", "w");

		if (nullptr != fp)
		{
			//printf("save cfg file\n");

			fprintf(fp, "gfCfgMngMnuItm_View_Clock = %hhd\ngfCfgMngMnuItm_View_Calendar = %hhd\ngfCfgMngMnuItm_Config_PicApicSelect = %hhd\ngfCfgMngMnuItm_Config_DelaySelect1 = %hhd\ngfCfgMngMnuItm_Config_DelaySelect2 = %hhd\ngfCfgMngMnuItm_Config_DelaySelect3 = %hhd\ngfCfgMngMnuItm_Config_DelaySelect4 = %hhd\ngfCfgMngMnuItm_Config_DelaySelect5 = %hhd",
				gfCfgMngMnuItm_View_Clock,
				gfCfgMngMnuItm_View_Calendar,
				gfCfgMngMnuItm_Config_PicApicSelect,
				gfCfgMngMnuItm_Config_DelaySelect1,
				gfCfgMngMnuItm_Config_DelaySelect2,
				gfCfgMngMnuItm_Config_DelaySelect3,
				gfCfgMngMnuItm_Config_DelaySelect4,
				gfCfgMngMnuItm_Config_DelaySelect5
			);
		}
	}

	DPRINTF(("...exit\n"));
	return nRet;
}