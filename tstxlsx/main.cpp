//
// prefixes used:
//	wcs - wide character string
//	str - narrow character string
//	fn	- function
//  g	- global
//  rg  - range, array
//  idx	- index
//	Mnu	- menu
//  Itm	- item
//  Mng	- managed
//	Cfg	- config

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

#include "xlsxwriter.h"

#define COL_TBL_START (/* start column --> */'O' - 'A')

extern "C" EFI_SYSTEM_TABLE * _cdegST;
extern "C" EFI_SYSTEM_TABLE * gSystemTable;
extern "C" EFI_HANDLE gImageHandle;

extern "C" int _outp(unsigned short port, int data_byte);
extern "C" unsigned short _outpw(unsigned short port, unsigned short data_word);
extern "C" unsigned long _outpd(unsigned short port, unsigned long data_word);
extern "C" int _inp(unsigned short port);
extern "C" unsigned short _inpw(unsigned short port);
extern "C" unsigned long _inpd(unsigned short port);

extern bool gfKbdDbg;
extern "C" uint16_t gPmTmrBlkAddr;
extern "C" uint64_t AcpiClkWait(uint32_t delay);
extern "C" uint64_t PITClkWait(uint32_t delay);
extern "C" unsigned long long _osifIbmAtGetTscPer62799(uint32_t delay);

extern "C" WINBASEAPI UINT WINAPI EnumSystemFirmwareTables(
	/*_In_*/ DWORD FirmwareTableProviderSignature,
	/*_Out_writes_bytes_to_opt_(BufferSize, return)*/ PVOID pFirmwareTableEnumBuffer,
	/*_In_*/ DWORD BufferSize
);

int main(int argc, char** argv)
{
	lxw_workbook* workbook = workbook_new("chart_line.xlsx");
	lxw_worksheet* worksheet = workbook_add_worksheet(workbook, nullptr);
	lxw_format* bold = workbook_add_format(workbook);
	lxw_chart* chart;
	lxw_chart_series* series;
	
	int cntSamples = 750;
    static uint64_t statbuf4maxall[1250 * 10];	// one buffer for all
	//bool Ena[10] = { true,false,false,false,false,false,false,false,false,false };
	static struct {
		char szTitle[64];
		uint32_t delay;
		uint32_t mul;
		uint64_t(*pfnDelay)(uint32_t  Delay);
		bool Ena;
		uint64_t* rgDiffTSC;	// equivalence of arrays and pointers
		//uint64_t diffTSC;
		//uint64_t sum;
		//uint64_t max;
		//uint64_t min;
	}parms[] = {
		//PIT
		{"i8254 PIT ticks : \n1193181 x    1",1 * 1193181,1, nullptr, true,	&statbuf4maxall[0 * 1250]},
		{"i8254 PIT ticks : \n62799   x   19",1 * 62799,19 , nullptr, true,&statbuf4maxall[1 * 1250]},
		{"i8254 PIT fixed : \n3287    x  363",1 * 3287,363 , nullptr, true,&statbuf4maxall[2 * 1250]},
		{"i8254 PIT ticks : \n173     x 6897",1 * 173,6897 , nullptr, true,&statbuf4maxall[3 * 1250]},
		{"i8254 PIT ticks : \n121     x 9861",1 * 121,9861 , nullptr, true,	&statbuf4maxall[4 * 1250]},
		// ACPI
		{"ACPI PMTmr ticks: \n3579543 x    1",3 * 1193181,1, nullptr, true,&statbuf4maxall[5 * 1250]},
		{"ACPI PMTmr ticks: \n188397  x   19",3 * 62799,19 , nullptr, true,&statbuf4maxall[6 * 1250]},
		{"ACPI PMTmr ticks: \n9861    x  363",3 * 3287,363 , nullptr, true,&statbuf4maxall[7 * 1250]},
		{"ACPI PMTmr ticks: \n519     x 6897",3 * 173,6897 , nullptr, true,&statbuf4maxall[8 * 1250]},
		{"ACPI PMTmr ticks: \n363     x 9861",3 * 121,9861 , nullptr, true,&statbuf4maxall[9 * 1250]},
	};

	srand((unsigned)__rdtsc());
	
	if (1)
	{
		int xten = 1;

		for (int i = 0; i < ELC(parms); i++)
		{
			if (false == parms[i].Ena)
				continue;

			for (int row = 0; row < cntSamples; row++)
			{
				parms[i].rgDiffTSC[row] = rand() / (xten);
			}
			xten *= 2;
		}
	}

	format_set_bold(bold);
	format_set_text_wrap(bold);
	worksheet_set_row(worksheet,0,80, bold);

	//
	// write column title
	//
	for (int i = 0, col = 1/* COL 0 is reserved for line numbers, scatter charts must have 'categories' and 'values'  */; i < ELC(parms); i++)
	{
		if (false == parms[i].Ena)
			continue;

		worksheet_write_string(worksheet, 0,(lxw_col_t) (COL_TBL_START + col), parms[i].szTitle, nullptr);
		col++;
	}
	
	//	
	// write line numbers, NOTE: for scatter charts x-y coordinates needed , scatter charts must have 'categories' and 'values'  
	// 
	for (int j = 0; j < cntSamples; j++)
	{
		// write numbers		
		worksheet_write_number(worksheet, j + 1, COL_TBL_START, j, nullptr);
	}

	for (int i = 0, col = 1/* COL 0 is reserved for line numbers, scatter charts must have 'categories' and 'values'  */; i < ELC(parms); i++)
	{
		if (false == parms[i].Ena)
			continue;

		for (int j = 0; j < cntSamples; j++)
		{
			/* Write some data for the chart. */
			worksheet_write_number(worksheet, j + 1, (lxw_col_t)(COL_TBL_START + col), (double)parms[i].rgDiffTSC[j], nullptr);
		}
		col++;
	}


    /* Create a chart object. */
    chart = workbook_add_chart(workbook, LXW_CHART_SCATTER);

	///* Configure the chart. */
	for (	int i = 0, col = 1/* COL 0 is reserved for line numbers, scatter charts must have 'categories' and 'values' */; 
			i < ELC(parms); 
			i++)
	{
		char strCategory[64], strValue[64];

		if (false == parms[i].Ena)
			continue;

		sprintf(strCategory	, "=Sheet1!$%c$2:$%c$%d", 'A' + COL_TBL_START      , 'A' + COL_TBL_START      , 1 + cntSamples);
		sprintf(strValue	, "=Sheet1!$%c$2:$%c$%d", 'A' + COL_TBL_START + col, 'A' + COL_TBL_START + col, 1 + cntSamples);

		series = chart_add_series(chart, strCategory, strValue);
		chart_series_set_name(series, parms[i].szTitle);

		col++;
	}

 	worksheet_insert_chart(worksheet, CELL("B2"), chart);

    return workbook_close(workbook);
}