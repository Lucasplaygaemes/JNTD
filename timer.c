#include "timer.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#define LOG_FILE "work_timer.log"

// Static global variable to hold the session start time.
static time_t session_start_time;

// Helper function to format total seconds into HH:MM:SS
void format_duration(long total_seconds, char* buffer, size_t buffer_size) {
    if (total_seconds < 0) total_seconds = 0;
    long hours = total_seconds / 3600;
    long minutes = (total_seconds % 3600) / 60;
    long seconds = total_seconds % 60;
    snprintf(buffer, buffer_size, "%ldh %02ldm %02lds", hours, minutes, seconds);
}

void start_work_timer() {
    session_start_time = time(NULL);
}

void stop_and_log_work() {
    if (session_start_time == 0) return;

    time_t session_end_time = time(NULL);
    long duration = (long)(session_end_time - session_start_time);

    // Do not log very short sessions (e.g., less than 5 seconds)
    if (duration < 5) {
        return;
    }

    FILE* file = fopen(LOG_FILE, "a");
    if (!file) {
        perror("Could not open the timer log file");
        return;
    }

    char date_str[11]; // Format YYYY-MM-DD
    // Standardized to YYYY-MM-DD for consistency and easier parsing
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", localtime(&session_end_time));

    fprintf(file, "%s,%ld\n", date_str, duration);
    fclose(file);
}

void display_work_summary() {
    FILE* file = fopen(LOG_FILE, "r");
    if (!file) {
        printf("No work time records found.\n");
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }

    long today_total = 0;
    long week_total = 0;
    long month_total = 0;
    long semester_total = 0;
    long year_total = 0;

    time_t now = time(NULL);
    struct tm* current_time = localtime(&now);

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char* date_part = strtok(line, ",");
        char* seconds_part = strtok(NULL, ",");

        if (date_part && seconds_part) {
            long seconds = atol(seconds_part);
            struct tm log_time = {0};

            // Parse the YYYY-MM-DD format
            sscanf(date_part, "%d-%d-%d", &log_time.tm_year, &log_time.tm_mon, &log_time.tm_mday);

            // Correct adjustments for the tm struct
            log_time.tm_year -= 1900; // tm_year is years since 1900
            log_time.tm_mon -= 1;     // tm_mon is 0-11

            if (log_time.tm_year == current_time->tm_year) {
                year_total += seconds;

                // Semester (0-5 is the first, 6-11 is the second)
                if ((log_time.tm_mon / 6) == (current_time->tm_mon / 6)) {
                    semester_total += seconds;
                }
                // Month
                if (log_time.tm_mon == current_time->tm_mon) {
                    month_total += seconds;
                }

                // Normalize the struct to get day of the week/year correctly
                mktime(&log_time);

                // Week (compares the day of the year)
                if (log_time.tm_yday >= (current_time->tm_yday - current_time->tm_wday) && log_time.tm_yday <= current_time->tm_yday) {
                    week_total += seconds;
                }
                // Day (check month as well to be safe)
                if (log_time.tm_mday == current_time->tm_mday && log_time.tm_mon == current_time->tm_mon) {
                    today_total += seconds;
                }
            }
        }
    }
    fclose(file);

    char date_str[100];
    // Note: The output language of strftime depends on the system's locale.
    strftime(date_str, sizeof(date_str), "%A, %B %d, %Y", current_time);

    char today_buf[50], week_buf[50], month_buf[50], sem_buf[50], year_buf[50];
    format_duration(today_total, today_buf, sizeof(today_buf));
    format_duration(week_total, week_buf, sizeof(week_buf));
    format_duration(month_total, month_buf, sizeof(month_buf));
    format_duration(semester_total, sem_buf, sizeof(sem_buf));
    format_duration(year_total, year_buf, sizeof(year_buf));

    system("clear");
    printf("--- Work Time Report ---\n");
    printf("Date: %s\n\n", date_str);
    printf("     Today: %s\n", today_buf);
    printf("      Week: %s\n", week_buf);
    printf("     Month: %s\n", month_buf);
    printf("  Semester: %s\n", sem_buf);
    printf("      Year: %s\n", year_buf);
    printf("\n--------------------------\n");
    printf("\nPress Enter to return to the editor...");

    // Clear input buffer and wait for Enter
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
    if (c != EOF) {
        getchar();
    }
}