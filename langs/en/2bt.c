#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/stat.h> // For stat (Linux/macOS)
#include <unistd.h>  // For sleep()
#ifdef _WIN32
#include <windows.h> // For Sleep()
#endif
#ifdef _WIN32
#include <direct.h> // For _getcwd on Windows
#else
#include <unistd.h> // For getcwd on Linux/macOS
#endif
// Defines the maximum size for the user prompt input buffer
#define MAX_PROMPT_LEN 256
// Defines the maximum size for the Ollama input + extras
#define MAX_OLLAMA_CMD_LEN 4096 // Increased to accommodate the worst case of long prompt + prefixes
// Defines the maximum size of the Ollama output
#define OLLAMA_BUFFER_SIZE 1024
// Defines the model
#define OLLAMA_MODEL "llama2"
// Defines the maximum size of the command history
#define MAX_HISTORY 50
// Defines the maximum size of the user input + extras like calc
#define COMBINED_PROMPT_LEN 2048 // Already suitable, but kept for clarity
int max_todo = 100;
char tarefa[512] = {0};
char usuario[128] = "Unknow";
char prazo[32] = "Without deadline";
char input_copy[1024];
char dir_novo[100];
char *path[100];
char dir_ant[100];
char *command_history[MAX_HISTORY];
int history_count = 0;
static char ult_calc_resu[1024] = "";
char buf[512];
char lines[100][1024];
int linhazinhas[100];
char quiz[1024];
int countar = 0;
int line_number;
int count = 0;
const char* quiz_file = "quiz.txt";
// Commands structure
typedef struct {
    const char *key;
    const char *shell_command;
    const char *descri;
} CmdEntry;

// Commands definitions (declared before the function is used.)
static const CmdEntry cmds[] = {
    { "ls", "pwd && ls -l", "Print the current directory and its files." },
    { ":q", "exit", "Another way to exit JNTD, inspired by VIM." },
    { "lsa", "pwd && ls -la", "Same as 'ls' but shows all files, including hidden ones." },
    { "data", "date", "Displays the current date and time." },
    { "quem", "whoami", "Displays the name of the user (works only on Linux)." },
    { "esp", "df -h .", "Shows the free storage space (Linux only)." },
    { "sysatt?", "pop-upgrade release check", "Checks if there are any updates available (for Pop!_OS)." },
    { "sudo", "sudo su", "Enters sudo mode. Use with caution." },
    { "help", NULL, "Lists all available commands and their descriptions." },
    { "criador", "echo lucasplayagemes is the creator of this code.", "Displays the name of the creator of JNTD and 2B." },
    { "2b", NULL, "Starts a conversation with 2B and processes its output." },
    { "log", NULL, "The code always saves a log file for eventualities." },
    { "calc", NULL, "Uses the advanced calculator. Example: calc sum 5 3 or calc deriv_poly 3 2 2 1 -1 0." },
    { "his", NULL, "Displays the history of typed commands." },
    { "cl", "clear", "Clears the terminal." },
    { "git", NULL, "Shows the GitHub link of the repository and the last commit, but the commit may not always work as it depends on the system's git, which might be linked to another repo." },
    { "mkdir", NULL, "Creates a new unnamed directory; naming it will be added." },
    { "rscript", NULL, "Runs a predefined script; place each command on a line." },
    { "sl", "sl", "Easter Egg." },
    { "cd", NULL, "The cd command; you change directory using cd <destination>." },
    { "pwd", "pwd", "Displays the current directory." },
    { "vim", "vim", "The editor in which I made all this code." },
    { "todo", NULL, "Adds one or more TODO tasks to the todo.txt file." },
    { "checkt", NULL, "Checks if there are any overdue TODOs or those due today." },
    { "listt", NULL, "Lists all TODO tasks saved in the todo.txt file." },
    { "remt", NULL, "Removes a TODO task from the file by its number." },
    { "editt", NULL, "Edits an existing TODO task by its number." },
    { "edit_vim", NULL, "Opens the todo.txt file in vim for direct editing." },
    { "quiz", NULL, "Shows all questions from the integrated quiz." },
    { "quizt", NULL, "Sets the time interval between quizzes." },
    { "quizale", NULL, "Asks a random question from the quiz." }
};

// Forward declarations of functions
void dispatch(const char *user_in);
void handle_ollama_interaction();
void handle_calc_interaction(const char *args);
void display_help();
void log_action(const char *action, const char *details);
int is_safe_command(const char *cmd);
void add_to_history(const char *cmd);
void display_history();
void TODO(const char *input);
void check_todos();
void cd(const char *args);
int jntd_mkdir(const char *args);
void rscript(const char *args);
void func_quiz();
char* read_random_line(const char* quiz_file);

// Function to check command safety
int is_safe_command(const char *cmd) {
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i) {
        if (strcasecmp(cmd, cmds[i].key) == 0) {
            return 1; // Command is in the allowed list
        }
    }
    return 0; // Command not recognized, not safe
}

// Functions for command history
void add_to_history(const char *cmd) {
    if (history_count < MAX_HISTORY) {
        command_history[history_count] = strdup(cmd); // Allocates memory
        history_count++;
    } else {
        free(command_history[0]);
        for (int i = 1; i < MAX_HISTORY; i++) {
            command_history[i - 1] = command_history[i];
        }
        command_history[MAX_HISTORY - 1] = strdup(cmd);
    }
}

void display_history() {
    printf("Command history:\n");
    for (int i = 0; i < history_count; i++) {
        printf("  %d: %s\n", i + 1, command_history[i]);
    }
}

void list_todo() {
    count = 0; // Resets the global counter
    printf("List of TODOs:\n");
    printf("------------------------------------------------\n");
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        perror("Error opening todo.txt");
        printf("Could not list TODOs. File not found or no permission.\n");
        return;
    }
    char line[1024]; // Temporary buffer for each line
    while (fgets(line, sizeof(line), todo_file) != NULL) {
        line[strcspn(line, "\n")] = '\0'; // Removes newline from the end
        printf("%d: %s\n", ++count, line);
    }
    fclose(todo_file);
    if (count == 0) {
        printf("No TODO found in the file.\n");
    }
    printf("------------------------------------------------\n");
}

void remove_todo() {
    list_todo(); // Shows the list of TODOs for the user to choose
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        perror("Error opening todo.txt");
        printf("Could not remove TODOs. File not found or no permission.\n");
        return;
    }

    if (count >= max_todo) {
        printf("TODO limit reached!\n");
        return;
    }

    // Reads all lines into a temporary buffer
    // Limit of 100 TODOs for simplicity
    int count = 0;
    while (fgets(lines[count], sizeof(lines[count]), todo_file) != NULL && count < 100) {
        lines[count][strcspn(lines[count], "\n")] = '\0';
        count++;
    }
    fclose(todo_file);
    if (count == 0) {
        printf("No TODO to remove.\n");
        return;
    }

    // Asks for the number of the TODO to be removed
    char temp_input[256];
    printf("Enter the number of the TODO to remove (1-%d, or 0 to cancel): ", count);
    fflush(stdout);
    if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
        perror("Error reading input");
        return;
    }
    temp_input[strcspn(temp_input, "\n")] = '\0';
    int index = atoi(temp_input);
    if (index <= 0 || index > count) {
        printf("Operation canceled or invalid number.\n");
        return;
    }

    // Writes all lines except the chosen one back to the file
    FILE *temp_file = fopen("todo_temp.txt", "w");
    if (temp_file == NULL) {
        perror("Error creating temporary file");
        printf("Could not remove the TODO.\n");
        return;
    }
    for (int i = 0; i < count; i++) {
        if (i != index - 1) { // Ignores the line to be removed
            fprintf(temp_file, "%s\n", lines[i]);
        }
    }
    fclose(temp_file);

    // Replaces the original file with the temporary one
    if (remove("todo.txt") != 0 || rename("todo_temp.txt", "todo.txt") != 0) {
        perror("Error updating todo.txt");
        printf("Could not complete the removal.\n");
        return;
    }
    printf("TODO number %d removed successfully.\n", index);
}

void quiz_aleatorio() {
    printf("Do you want to play the QUIZ? (y/n)\n");
    char respostas[256];
    if (fgets(respostas, sizeof(respostas), stdin) != NULL) {
        respostas[strcspn(respostas, "\n")] = '\0'; // Removes newline character
        if (strcasecmp(respostas, "y") == 0 || strcasecmp(respostas, "yes") == 0) {
            read_random_line(respostas);
        } else {
            printf("Could not identify the answer\n");
        }
    } else {
        printf("Error reading the answer\n");
    }
}

void quiz_timer() {
    printf("What is the time interval between QUIZZES in seconds? (Press enter for default, 10 [600s] min)");
    int seconds;
    if (scanf("%d", &seconds) != 1) { // Remove \n from scanf
        seconds = 600; // Default value
    }

    // Clear input buffer
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    // Use sleep for better performance
    sleep(seconds); // For Linux/macOS
#ifdef _WIN32
    Sleep(seconds * 1000); // For Windows
#endif
}

void func_quiz() {
    printf("List of QUIZ's:\n");
    printf("------------------------------------------------\n");
    FILE *quiz_f = fopen("quiz.txt", "r");
    if (quiz_f == NULL) {
        perror("Error opening quiz.txt");
        printf("Could not list QUIZ's. File not found or no permission.\n");
        return;
    }
    while (fgets(quiz, sizeof(quiz), quiz_f) != NULL) {
        quiz[strcspn(quiz, "\n")] = '\0'; // Removes newline from the end
        printf("%d: %s\n", ++countar, quiz);
    }
    fclose(quiz_f);
    if (countar == 0) {
        printf("No QUIZ found in the file.\n");
    }
    printf("------------------------------------------------\n");
}

int contar_linhas(const char* quiz_file) {
    FILE *quizlin = fopen("quiz.txt", "r");
    if (quizlin == NULL) {
        perror("Error opening quiz.txt");
        printf("Could not list QUIZ's. File not found or no permission.\n");
        return 0;
    }
    int linhaslin = 0;
    char buffin1[1024];
    while (fgets(buffin1, sizeof(buffin1), quizlin) != NULL) {
        linhaslin++;
    }
    fclose(quizlin);
    return linhaslin;
}

char* read_speci_line(const char* quiz_file, int line_number) {
    FILE *quiz_f = fopen(quiz_file, "r");
    if (quiz_f == NULL) {
        perror("Error opening file");
        return NULL;
    }

    char buffer[1024];
    char *result = NULL;
    int linha_atual = 0;

    while (fgets(buffer, sizeof(buffer), quiz_f) != NULL) {
        linha_atual++;
        if (linha_atual == line_number) {
            buffer[strcspn(buffer, "\n")] = '\0'; // Removes '\n' if present
            result = strdup(buffer);
            if (result == NULL) {
                fprintf(stderr, "Error allocating memory for the line\n");
                fclose(quiz_f);
                return NULL;
            }
            break; // Exits the loop after finding the line
        }
    }

    fclose(quiz_f);

    if (result == NULL) {
        fprintf(stderr, "Line %d not found. Total lines in file: %d\n", line_number, linha_atual);
        return NULL;
    }

    return result;
}

// Function to read a random line
char* read_random_line(const char* quiz_file) {
    int total_lines = contar_linhas(quiz_file);
    if (total_lines == 0) {
        printf("No file found or empty\n");
        return NULL;
    }
    // Generates a random number between 1 and total_lines
    srand(time(NULL)); // Initializes the generator seed
    int random_line = (rand() % total_lines) + 1; // Generates between 1 and total_lines

    // Reads the line corresponding to the number
    return read_speci_line(quiz_file, random_line);
}

void edit_todo() {
    list_todo(); // Shows the list of TODOs for the user to choose
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        perror("Error opening todo.txt");
        printf("Could not edit TODOs. File not found or no permission.\n");
        return;
    }

    // Reads all lines into a temporary buffer
    // Limit of 100 TODOs for simplicity
    int count = 0;
    while (fgets(lines[count], sizeof(lines[count]), todo_file) != NULL && count < 100) {
        lines[count][strcspn(lines[count], "\n")] = '\0';
        count++;
    }
    fclose(todo_file);

    if (count == 0) {
        printf("No TODO to edit.\n");
        return;
    }
    if (count >= max_todo) {
        printf("TODO limit reached!\n");
        return;
    }
    // Asks for the number of the TODO to be edited
    char temp_input[256];
    printf("Enter the number of the TODO to edit (1-%d, or 0 to cancel): ", count);
    fflush(stdout);
    if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
        perror("Error reading input");
        return;
    }
    temp_input[strcspn(temp_input, "\n")] = '\0';
    int index = atoi(temp_input);
    if (index <= 0 || index > count) {
        printf("Operation canceled or invalid number.\n");
        return;
    }

    // Asks for new values for the TODO
    char nova_tarefa[512] = {0};
    char novo_usuario[128] = {0};
    char novo_prazo[32] = {0};

    printf("Enter the new task subject (leave empty to keep '%s'): ", lines[index - 1]);
    fflush(stdout);
    if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
        perror("Error reading new subject");
        return;
    }
    temp_input[strcspn(temp_input, "\n")] = '\0';
    if (strlen(temp_input) > 0) {
        strncpy(nova_tarefa, temp_input, sizeof(nova_tarefa) - 1);
        nova_tarefa[sizeof(nova_tarefa) - 1] = '\0';
        char *ptr = nova_tarefa;
        while (*ptr == ' ') ptr++;
        memmove(nova_tarefa, ptr, strlen(ptr) + 1);
        char *end = nova_tarefa + strlen(nova_tarefa) - 1;
        while (end >= nova_tarefa && *end == ' ') *end-- = '\0';
    } else {
        // Extracts the current value (simplified, can be improved)
        char *start = strstr(lines[index - 1], "TODO: ") + 6;
        char *end = strstr(start, " | User: ");
        if (end) *end = '\0';
        strncpy(nova_tarefa, start, sizeof(nova_tarefa) - 1);
        nova_tarefa[sizeof(nova_tarefa) - 1] = '\0';
    }

    // Asks for new user
    printf("Enter the new responsible (leave empty to keep current): ");
    fflush(stdout);
    if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
        perror("Error reading new responsible");
        return;
    }
    temp_input[strcspn(temp_input, "\n")] = '\0';
    if (strlen(temp_input) > 0) {
        strncpy(novo_usuario, temp_input, sizeof(novo_usuario) - 1);
        novo_usuario[sizeof(novo_usuario) - 1] = '\0';
        char *ptr = novo_usuario;
        while (*ptr == ' ') ptr++;
        memmove(novo_usuario, ptr, strlen(ptr) + 1);
        char *end = novo_usuario + strlen(novo_usuario) - 1;
        while (end >= novo_usuario && *end == ' ') *end-- = '\0';
    } else {
        // Extracts the current value (simplified)
        char *start = strstr(lines[index - 1], "User: ") + 6;
        char *end = strstr(start, " | Deadline: ");
        if (end) *end = '\0';
        strncpy(novo_usuario, start, sizeof(novo_usuario) - 1);
        novo_usuario[sizeof(novo_usuario) - 1] = '\0';
    }

    // Asks for new date
    printf("Enter the new due date (dd/mm/yyyy, leave empty to keep current): ");
    fflush(stdout);
    if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
        perror("Error reading new date");
        return;
    }
    temp_input[strcspn(temp_input, "\n")] = '\0';
    if (strlen(temp_input) > 0) {
        strncpy(novo_prazo, temp_input, sizeof(novo_prazo) - 1);
        novo_prazo[sizeof(novo_prazo) - 1] = '\0';
        char *ptr = novo_prazo;
        while (*ptr == ' ') ptr++;
        memmove(novo_prazo, ptr, strlen(ptr) + 1);
        char *end = novo_prazo + strlen(novo_prazo) - 1;
        while (end >= novo_prazo && *end == ' ') *end-- = '\0';
    } else {
        // Extracts the current value (simplified)
        char *start = strstr(lines[index - 1], "Deadline: ") + 10;
        strncpy(novo_prazo, start, sizeof(novo_prazo) - 1);
        novo_prazo[sizeof(novo_prazo) - 1] = '\0';
    }

    // Gets the original creation date
    char time_str[26] = {0};
    char *start_time = strstr(lines[index - 1], "[");
    if (start_time) {
        strncpy(time_str, start_time + 1, 24);
        time_str[24] = '\0';
    } else {
        time_t now = time(NULL);
        ctime_r(&now, time_str);
        time_str[24] = '\0';
    }

    // Writes all lines, updating the chosen one
    FILE *temp_file = fopen("todo_temp.txt", "w");
    if (temp_file == NULL) {
        perror("Error creating temporary file");
        printf("Could not edit the TODO.\n");
        return;
    }
    for (int i = 0; i < count; i++) {
        if (i == index - 1) {
            fprintf(temp_file, "[%s] TODO: %s | User: %s | Deadline: %s\n", time_str, nova_tarefa, novo_usuario, novo_prazo);
        } else {
            fprintf(temp_file, "%s\n", lines[i]);
        }
    }
    fclose(temp_file);

    // Replaces the original file with the temporary one
    if (remove("todo.txt") != 0 || rename("todo_temp.txt", "todo.txt") != 0) {
        perror("Error updating todo.txt");
        printf("Could not complete the edit.\n");
        return;
    }
    printf("TODO number %d edited successfully.\n", index);
}

void edit_with_vim() {
    printf("Opening todo.txt in vim editor for editing...\n");
    int status = system("vim todo.txt");
    if (status == -1) {
        perror("Error executing vim command");
        printf("Could not open the editor. Check if 'vim' is installed.\n");
    } else if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code == 0) {
            printf("Editing completed successfully.\n");
        } else {
            printf("Editor closed with status %d.\n", exit_code);
        }
    }
}

void TODO(const char *input) {
    char tarefa[512] = {0};
    char usuario[128] = {0};
    char prazo[32] = {0};
    char temp_input[256] = {0};
    do {
        // Clears the buffers for new input
        memset(tarefa, 0, sizeof(tarefa));
        memset(usuario, 0, sizeof(usuario));
        memset(prazo, 0, sizeof(prazo));

        // Asks for the task subject
        printf("Enter the task subject: ");
        fflush(stdout);
        if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
            perror("Error reading task subject");
            return;
        }
        temp_input[strcspn(temp_input, "\n")] = '\0'; // Removes newline
        strncpy(tarefa, temp_input, sizeof(tarefa) - 1);
        tarefa[sizeof(tarefa) - 1] = '\0';
        // Removes leading and trailing spaces
        char *ptr = tarefa;
        while (*ptr == ' ') ptr++;
        memmove(tarefa, ptr, strlen(ptr) + 1);
        char *end = tarefa + strlen(tarefa) - 1;
        while (end >= tarefa && *end == ' ') *end-- = '\0';

        if (strlen(tarefa) == 0) {
            printf("Error: No task provided. Operation canceled.\n");
            return;
        }
        // Asks for the name of the responsible
        printf("Enter the name of the responsible (or leave empty for 'Unknown'): ");
        fflush(stdout);
        if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
            perror("Error reading responsible name");
            return;
        }
        temp_input[strcspn(temp_input, "\n")] = '\0'; // Removes newline
        if (strlen(temp_input) > 0) {
            strncpy(usuario, temp_input, sizeof(usuario) - 1);
            usuario[sizeof(usuario) - 1] = '\0';
            ptr = usuario;
            while (*ptr == ' ') ptr++;
            memmove(usuario, ptr, strlen(ptr) + 1);
            end = usuario + strlen(usuario) - 1;
            while (end >= usuario && *end == ' ') *end-- = '\0';
        } else {
            strcpy(usuario, "Unknown");
        }
        // Asks for the due date
        printf("Enter the due date (dd/mm/yyyy or leave empty for 'No deadline'): ");
        fflush(stdout);
        if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
            perror("Error reading due date");
            return;
        }
        temp_input[strcspn(temp_input, "\n")] = '\0'; // Removes newline
        if (strlen(temp_input) > 0) {
            strncpy(prazo, temp_input, sizeof(prazo) - 1);
            prazo[sizeof(prazo) - 1] = '\0';
            ptr = prazo;
            while (*ptr == ' ') ptr++;
            memmove(prazo, ptr, strlen(ptr) + 1);
            end = prazo + strlen(prazo) - 1;
            while (end >= prazo && *end == ' ') *end-- = '\0';
        } else {
            strcpy(prazo, "No deadline");
        }
        // Gets the current time for the creation stamp
        char time_str[26];
        time_t now = time(NULL);
        ctime_r(&now, time_str);
        time_str[24] = '\0'; // Removes newline from the end of the time string

        // Saves to the file
        FILE *todo_file = fopen("todo.txt", "a");
        if (todo_file == NULL) {
            perror("Error opening todo.txt");
            printf("Could not save the TODO. Check permissions or directory.\n");
            return;
        }
        fprintf(todo_file, "[%s] TODO: %s | User: %s | Deadline: %s\n", time_str, tarefa, usuario, prazo);
        fclose(todo_file);
        printf("TODO saved successfully: '%s' by '%s' with deadline '%s' at [%s]\n", tarefa, usuario, prazo, time_str);

        // Asks if the user wants to add another TODO
        printf("Do you want to add another TODO? (y/n): ");
        fflush(stdout);
        if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
            perror("Error reading response");
            return;
        }
        temp_input[strcspn(temp_input, "\n")] = '\0';
    } while (temp_input[0] == 'y' || temp_input[0] == 'Y');
}

// Helper function to convert date in dd/mm/yyyy format to time_t
time_t parse_date(const char *date_str) {
    if (strcmp(date_str, "No deadline") == 0) {
        return -1; // Indicator for "no deadline"
    }
    struct tm tm = {0};
    if (sscanf(date_str, "%d/%d/%d", &tm.tm_mday, &tm.tm_mon, &tm.tm_year) != 3) {
        return -1; // Invalid format
    }
    tm.tm_mon -= 1; // Adjusts month (0-11 in tm)
    tm.tm_year -= 1900; // Adjusts year (since 1900 in tm)
    tm.tm_hour = 23; // Sets to the end of the day (23:59:59)
    tm.tm_min = 59;
    tm.tm_sec = 59;
    return mktime(&tm);
}

void check_todos() {
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        perror("Error opening todo.txt for checking");
        printf("Could not check TODOs. File not found or no permission.\n");
        return;
    }
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char current_date[11];
    snprintf(current_date, sizeof(current_date), "%02d/%02d/%04d",
             tm_now->tm_mday, tm_now->tm_mon + 1, tm_now->tm_year + 1900);
    printf("Checking overdue TODOs or those due today (%s)...\n", current_date);
    int has_overdue = 0;
    char line[1024]; // Temporary buffer for each line
    while (fgets(line, sizeof(line), todo_file) != NULL) {
        line[strcspn(line, "\n")] = '\0'; // Removes newline
        // Extracts the fields from the line
        char *prazo_start = strstr(line, "Deadline: ");
        if (prazo_start) {
            prazo_start += 10; // Skips "Deadline: "
            char prazo[32] = {0};
            char *end = strchr(prazo_start, '\n');
            if (end) *end = '\0'; // Ends the string at the end of the line
            strncpy(prazo, prazo_start, sizeof(prazo) - 1);
            time_t prazo_time = parse_date(prazo);
            if (prazo_time != -1) { // Valid date
                if (difftime(now, prazo_time) >= 0) { // Current date >= deadline
                    has_overdue = 1;
                    printf("OVERDUE TODO OR DUE TODAY: %s\n", line);
                }
            }
        }
    }
    fclose(todo_file);
    if (!has_overdue) {
        printf("No overdue TODOs or due today.\n");
    }
}

// Function for logging actions
void log_action(const char *action, const char *details) {
    FILE *log_file = fopen("jntd_log.txt", "a");
    if (log_file == NULL) {
        perror("Error opening log file");
        return;
    }
    time_t now = time(NULL);
    char time_str[26];
    ctime_r(&now, time_str);
    time_str[24] = '\0'; // Removes newline
    fprintf(log_file, "[%s] %s: %s\n", time_str, action, details);
    fclose(log_file);
}

// Function to interact with the calculator
void handle_calc_interaction(const char *args) {
    char calc_command[256];
    char calc_output[1024];
    FILE *calc_pipe;

    ult_calc_resu[0] = '\0'; // Clears the previous calculator result

    // Builds the command with arguments (if any)
    if (args && strlen(args) > 0) {
        snprintf(calc_command, sizeof(calc_command), "./calc %s", args);
    } else {
        snprintf(calc_command, sizeof(calc_command), "./calc");
    }
    printf("Running calculator: %s\n", calc_command);
    calc_pipe = popen(calc_command, "r");
    if (calc_pipe == NULL) {
        perror("Failed to run calculator with popen");
        fprintf(stderr, "Check if ./calc is in the current directory.\n");
        return;
    }

    printf("-------------- Calculator Output --------------\n");
    while (fgets(calc_output, sizeof(calc_output), calc_pipe) != NULL) {
        printf("%s", calc_output);
        fflush(stdout);
        // Accumulates the output in ult_calc_resu
        strncat(ult_calc_resu, calc_output, sizeof(ult_calc_resu) - strlen(ult_calc_resu) - 1);
        log_action("Calculator Output", calc_output);
    }
    printf("---------------- End of Output --------------\n");

    int status = pclose(calc_pipe);
    if (status == -1) {
        perror("pclose failed for calculator pipe");
    } else {
        if (WIFEXITED(status)) {
            printf("Calculator finished with status: %d\n", WEXITSTATUS(status));
        }
    }
}

// Function for interaction with Ollama/2B
void handle_ollama_interaction() {
    char user_prompt[MAX_PROMPT_LEN];
    char combined_prompt[COMBINED_PROMPT_LEN];
    char ollama_full_command[MAX_OLLAMA_CMD_LEN];
    char ollama_output_line[OLLAMA_BUFFER_SIZE];
    FILE *ollama_pipe;

    printf("Enter your prompt for 2B, the output will be processed as commands\n");
    printf("IA prompt> ");
    fflush(stdout);

    if (fgets(user_prompt, sizeof(user_prompt), stdin) == NULL) {
        perror("Failed to read user prompt for 2B");
        return;
    }

    user_prompt[strcspn(user_prompt, "\n")] = '\0';
    log_action("User Prompt to 2B", user_prompt);

    if (user_prompt[0] == '\0') {
        printf("Empty prompt, no interaction with Ollama\n");
        return;
    }

    // Adds the last calculator result to the prompt, if any
    if (strlen(ult_calc_resu) > 0) {
        snprintf(combined_prompt, sizeof(combined_prompt),
                 "%s\n[Last calculator result: %s]", user_prompt, ult_calc_resu);
        snprintf(ollama_full_command, sizeof(ollama_full_command),
                 "ollama run %s \"%s\"", OLLAMA_MODEL, combined_prompt);
    } else {
        snprintf(ollama_full_command, sizeof(ollama_full_command),
                 "ollama run %s \"%s\"", OLLAMA_MODEL, user_prompt);
    }

    printf("Running Ollama with: %s\n", ollama_full_command);
    printf("Waiting for 2B's response...\n");
    printf("-------------- 2B output --------------\n");

    ollama_pipe = popen(ollama_full_command, "r");
    if (ollama_pipe == NULL) {
        perror("Failed to run Ollama command with popen");
        fprintf(stderr, "Check if Ollama is in the path and if 2B is available\n");
        return;
    }

    while (fgets(ollama_output_line, sizeof(ollama_output_line), ollama_pipe) != NULL) {
        printf("%s", ollama_output_line);
        fflush(stdout);
        log_action("2B Output", ollama_output_line);

        ollama_output_line[strcspn(ollama_output_line, "\n")] = '\0';
        if (ollama_output_line[0] != '\0') {
            if (strncmp(ollama_output_line, "CMD:", 4) == 0) {
                const char *cmd = ollama_output_line + 4;
                if (is_safe_command(cmd)) {
                    printf(">>> Running safe command from 2B: '%s'\n", cmd);
                    dispatch(cmd);
                } else {
                    printf(">>> WARNING: Command '%s' from 2B is not safe. Ignored.\n", cmd);
                    printf("    Type 'help' to see allowed commands.\n");
                }
            } else {
                printf(">>> Text from 2B (not a command): '%s'\n", ollama_output_line);
            }
        }
    }
    printf("---------------- End of 2B's speech --------------\n");

    int status = pclose(ollama_pipe);
    if (status == -1) {
        perror("pclose failed for Ollama pipe");
    } else {
        if (WIFEXITED(status)) {
            printf("Ollama process finished with status: %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Ollama process terminated with signal %d\n", WTERMSIG(status));
        }
    }
}

void display_help() {
    printf("Available commands:\n");
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i) {
        printf("  %-15s: %s\n", cmds[i].key, cmds[i].descri);
    }
}

void cd(const char *args) {
    // Checks if the argument (directory path) was provided
    if (args && strlen(args) > 0) {
        char old_dir[256];
        char new_dir[256];
        char args_copy[256];
        // Gets the current directory before changing
        if (getcwd(old_dir, sizeof(old_dir)) == NULL) {
            perror("Error getting current directory before change");
            old_dir[0] = '\0';
        }
        // Makes a copy of the argument to avoid direct modification
        strncpy(args_copy, args, sizeof(args_copy) - 1);
        args_copy[sizeof(args_copy) - 1] = '\0';
        // Removes newline or extra spaces, if any
        args_copy[strcspn(args_copy, "\n")] = '\0';
        // Tries to change directory
        int result = chdir(args_copy);
        if (result == 0) {
            // Gets the new current directory after change
            if (getcwd(new_dir, sizeof(new_dir)) == NULL) {
                perror("Error getting new current directory");
                // Continues without showing the new path
                new_dir[0] = '\0';
            }
            printf("Directory changed successfully. Before '%s', Now '%s'\n",
                   old_dir[0] ? old_dir : "unknown",
                   new_dir[0] ? new_dir : "unknown");
        } else {
            perror("Error changing directory");
            printf("Remains in the current directory: '%s'\n",
                   old_dir[0] ? old_dir : "unknown");
        }
    } else {
        printf("Error: Directory path not provided. Usage: cd <path>\n");
    }
}

int jntd_mkdir(const char *args) {
    // Checks if the argument (directory name) was provided
    if (args && strlen(args) > 0) {
        char current_dir[256];
        char mkdir_command[256];
        char full_path[512];
        // Gets the current working directory
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            perror("Error getting current directory");
            current_dir[0] = '\0';
            snprintf(full_path, sizeof(full_path), "%s", args);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, args);
        }
        // Checks if the directory already exists
        struct stat st;
        if (stat(args, &st) == 0) {
            printf("Error: Directory '%s' already exists in '%s'.\n",
                   args, current_dir[0] ? current_dir : "unknown current directory");
        } else {
            // Builds the command to create the directory
            snprintf(mkdir_command, sizeof(mkdir_command), "mkdir \"%s\"", args);
            printf("Creating directory '%s' in '%s'...\n",
                   args, current_dir[0] ? current_dir : "unknown current directory");
            // Executes the mkdir command via system
            int status = system(mkdir_command);
            if (status == -1) {
                perror("Error executing mkdir command");
            } else {
                if (WIFEXITED(status)) {
                    int exit_code = WEXITSTATUS(status);
                    printf("mkdir command finished with status '%d'.\n", exit_code);
                    if (exit_code == 0) {
                        // Checks again if the directory was created
                        if (stat(args, &st) == 0) {
                            printf("Directory created successfully in '%s'.\n", full_path);
                        } else {
                            printf("Error: Directory was not created, even with exit code 0\n");
                        }
                    }
                }
            }
        }
    } else {
        printf("Error: Directory name not provided. Usage: mkdir <name>\n");
    }
    return 0; // Added return to avoid warnings
}

void rscript(const char *args) {
    // Checks if the argument (script file name) was provided
    if (args && strlen(args) > 0) {
        FILE *script_file = fopen(args, "r");
        if (script_file == NULL) {
            perror("Error opening script file");
            printf("Check if the file: '%s' is in the directory, and if you have permission to read it\n", args);
        } else {
            char script_line[128];
            printf("Running commands from script '%s':\n", args);
            printf("-----------------------------------------------\n");
            while (fgets(script_line, sizeof(script_line), script_file) != NULL) {
                script_line[strcspn(script_line, "\n")] = '\0';
                if (script_line[0] != '\0' && script_line[0] != '#') {
                    printf("Running %s\n", script_line);
                    dispatch(script_line);
                }
            }
            printf("------------------------------------------------\n");
            printf("End of script execution '%s'.\n", args);
            fclose(script_file);
        }
    } else {
        printf("Error: Script file name not provided. Usage: rscript <filename>\n");
    }
}

void dispatch(const char *user_in) {
    char input_copy[128];
    strncpy(input_copy, user_in, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';

    char *token = strtok(input_copy, " ");
    if (token == NULL) return;

    char *args = strtok(NULL, ""); // Gets the rest of the string as arguments
    if (strlen(buf) > 0) {
        for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
            if (strncmp(buf, cmds[i].key, strlen(buf)) == 0) {
                printf("Suggestion: %s\n", cmds[i].key); // Shows suggestion
                fflush(stdout);
            }
        }
    }
    // Adds to history and log
    add_to_history(user_in);
    log_action("User Input", user_in);

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i) {
        if (strcasecmp(token, cmds[i].key) == 0) {
            if (strcmp(cmds[i].key, "help") == 0) {
                display_help();
            } else if (strcasecmp(cmds[i].key, "mkdir") == 0) {
                jntd_mkdir(args); // Passes arguments to mkdir
            } else if (strcasecmp(cmds[i].key, "2b") == 0) {
                handle_ollama_interaction();
            } else if (strcasecmp(cmds[i].key, "calc") == 0) {
                handle_calc_interaction(args);
            } else if (strcasecmp(cmds[i].key, "cd") == 0) {
                cd(args); // Passes arguments to cd
            } else if (strcasecmp(cmds[i].key, "git") == 0) {
                printf("The repository is: https://github.com/Lucasplaygaemes/JNTD\n");
                printf("Last commit: ");
                fflush(stdout);
                system("git log -1 --pretty=%B | head -n1");
            } else if (strcasecmp(cmds[i].key, "his") == 0) {
                display_history();
            } else if (strcasecmp(cmds[i].key, "ct") == 0) {
                check_todos();
            } else if (strcasecmp(cmds[i].key, "listt") == 0) {
                list_todo();
            } else if (strcasecmp(cmds[i].key, "quiz") == 0) {
                func_quiz();
            } else if (strcasecmp(cmds[i].key, "quizale") == 0) {
                quiz_aleatorio();
            } else if (strcasecmp(cmds[i].key, "quizt") == 0) {
                quiz_timer();
            } else if (strcasecmp(cmds[i].key, "rscript") == 0) {
                rscript(args);
            } else if (strcasecmp(cmds[i].key, "todo") == 0) {
                TODO(args);
            } else if (strcasecmp(cmds[i].key, "checkt") == 0) {
                check_todos();
            } else if (strcasecmp(cmds[i].key, "editt") == 0) {
                edit_todo();
            } else if (strcasecmp(cmds[i].key, "listt") == 0) {
                list_todo();
            } else if (strcasecmp(cmds[i].key, "remt") == 0) {
                remove_todo(args);
            } else if (cmds[i].shell_command != NULL) {
                // Executes shell commands defined in the table
                system(cmds[i].shell_command);
            }
            return;
        }
    }
    printf("Command '%s' not recognized. Use 'help' to see available commands.\n", token);
}

int main(void) {
    printf("Starting JNTD...\n");
    check_todos();
    printf("Enter a command. Use 'help' to see options or 'exit' to quit.\n");
    while (printf("> "), fgets(buf, sizeof(buf), stdin) != NULL) {
        buf[strcspn(buf, "\n")] = '\0'; // Removes newline
        if (strcmp(buf, "exit") == 0) {
            break;
        } else if (strcmp(buf, ":q") == 0) {
            break;
        }
        if (buf[0] == '\0') { // User just pressed Enter
            continue;
        }
        dispatch(buf);
    }
    // Frees memory from history before exiting
    for (int i = 0; i < history_count; i++) {
        free(command_history[i]);
    }
    printf("Exiting....\n");
    return 0;
}
