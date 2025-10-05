#include "timer.h"
#include "screen_ui.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

    char log_path[PATH_MAX];
    snprintf(log_path, sizeof(log_path), "%s/%s", executable_dir, LOG_FILE);
    FILE* file = fopen(log_path, "a");
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
    char log_path[PATH_MAX];
    snprintf(log_path, sizeof(log_path), "%s/%s", executable_dir, LOG_FILE);
    FILE* file = fopen(log_path, "r");
    if (!file) {
        display_output_screen("Work Time Report", "/tme/no_log.tmp");
        return;
    }
    long timer_total = 0;
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
            timer_total += seconds;
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

    char today_buf[50], total_buf[50], week_buf[50], month_buf[50], sem_buf[50], year_buf[50];
    format_duration(today_total, today_buf, sizeof(today_buf));
    format_duration(week_total, week_buf, sizeof(week_buf));
    format_duration(month_total, month_buf, sizeof(month_buf));
    format_duration(semester_total, sem_buf, sizeof(sem_buf));
    format_duration(year_total, year_buf, sizeof(year_buf));
    format_duration(timer_total, total_buf, sizeof(total_buf));
    
    char temp_filename[] = "/tmp/time_report.XXXXXX";
    int fd = mkstemp(temp_filename);
    if (fd == -1) return;
    
    FILE *temp_file = fdopen(fd, "w");
    if (!temp_file) { 
         close(fd); return;
    }
    
    
    fprintf(temp_file, "--- Work Time Report ---\n");
    fprintf(temp_file, "Date: %s\n\n", date_str);
    fprintf(temp_file, "      Today: %s\n", today_buf);
    fprintf(temp_file, "       Week: %s\n", week_buf);
    fprintf(temp_file, "      Month: %s\n", month_buf);
    fprintf(temp_file, "   Semester: %s\n", sem_buf);
    fprintf(temp_file, "       Year: %s\n", year_buf);
    fprintf(temp_file, "Total Timer: %s\n", total_buf);
    fprintf(temp_file, "\n--------------------------\n");
    fprintf(temp_file, "\nPress Enter to return to the editor...");
    fclose(temp_file);
    display_output_screen("", temp_filename);
}
