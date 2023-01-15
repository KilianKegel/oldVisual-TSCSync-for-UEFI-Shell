/*
 * An example of creating an Excel chartsheet using the libxlsxwriter library.
 *
 * Copyright 2014-2021, John McNamara, jmcnamara@cpan.org
 *
 */

#include "xlsxwriter.h"

 /*
  * Write some data to the worksheet.
  */
void write_worksheet_data(lxw_worksheet* worksheet, lxw_format* bold) {

    int row, col;
    uint8_t data[6][3] = {
        /* Three columns of data. */
        {2, 10, 30},
        {3, 40, 60},
        {4, 50, 70},
        {5, 20, 50},
        {6, 10, 40},
        {7, 50, 30}
    };

    worksheet_write_string(worksheet, CELL("A1"), "Number", bold);
    worksheet_write_string(worksheet, CELL("B1"), "Batch 1", bold);
    worksheet_write_string(worksheet, CELL("C1"), "Batch 2", bold);

    for (row = 0; row < 6; row++)
        for (col = 0; col < 3; col++)
            worksheet_write_number(worksheet, row + 1, (lxw_col_t)col, data[row][col], NULL);
}

/*
 * Create a worksheet with examples charts.
 */
int main() {

    lxw_workbook* workbook = workbook_new("chartsheet.xlsx");
    lxw_worksheet* worksheet = workbook_add_worksheet(workbook, NULL);
    //lxw_chartsheet* chartsheet = workbook_add_chartsheet(workbook, NULL);
    lxw_chart_series* series;

    /* Add a bold format to use to highlight the header cells. */
    lxw_format* bold = workbook_add_format(workbook);
    format_set_bold(bold);

    /* Write some data for the chart. */
    write_worksheet_data(worksheet, bold);

    /* Create a bar chart. */
    lxw_chart* chart = workbook_add_chart(workbook, LXW_CHART_BAR);

    /* Add the first series to the chart. */
    series = chart_add_series(chart, "=Sheet1!$A$2:$A$7", "=Sheet1!$B$2:$B$7");

    /* Set the name for the series instead of the default "Series 1". */
    chart_series_set_name(series, "=Sheet1!$B$1");

    /* Add a second series but leave the categories and values undefined. They
     * can be defined later using the alternative syntax shown below.  */
    series = chart_add_series(chart, NULL, NULL);

    /* Configure the series using a syntax that is easier to define programmatically. */
    chart_series_set_categories(series, "Sheet1", 1, 0, 6, 0); /* "=Sheet1!$A$2:$A$7" */
    chart_series_set_values(series, "Sheet1", 1, 2, 6, 2); /* "=Sheet1!$C$2:$C$7" */
    chart_series_set_name_range(series, "Sheet1", 0, 2);       /* "=Sheet1!$C$1"      */

    /* Add a chart title and some axis labels. */
    chart_title_set_name(chart, "Results of sample analysis");
    chart_axis_set_name(chart->x_axis, "Test number");
    chart_axis_set_name(chart->y_axis, "Sample length (mm)");

    /* Set an Excel chart style. */
    chart_set_style(chart, 11);

    for (int i = 0; i < 2 ;i++)
    {
        lxw_chartsheet* chartsheet[2];
        lxw_error err;
        
        chartsheet[i] = workbook_add_chartsheet(workbook, NULL);
        /* Add the chart to the chartsheet. */
        err = chartsheet_set_chart(chartsheet[i], chart);
        //printf("%2d: lxw_error %d\n", i, err);

        /* Display the chartsheet as the active sheet when the workbook is opened. */
        //chartsheet_activate(chartsheet[i]);
    }

    return workbook_close(workbook);
}
