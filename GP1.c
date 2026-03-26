#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <time.h>
#include <math.h>
#include <conio.h>
#include <mmsystem.h>

// FIX: Uncommented to ensure audio functions link correctly in MSVC

#define MAX_PLAYERS 30
#define NAME_LEN 16

typedef struct Player {
    char name[NAME_LEN];
    int score_1;
    int score_2;
    int score_3;
} Player;

typedef struct {
    int x, y;
    float spawn_time;
    int is_hit;
    int is_active;  
} Note;

Player leaderboard[MAX_PLAYERS];
Note track[500];

int last_lb = -1;
int last_tr = -1;
int logged_in = 0;
int track_playing = 0;
int current_player_index = -1;
int current_track_id = 0;
int current_score = 0;
int notes_hit = 0;
int notes_missed = 0;
DWORD track_start_time = 0;

// FIX: Consolidated input tracking variables
int last_mouse_x = -1, last_mouse_y = -1;
int prev_mouse_x = -1, prev_mouse_y = -1;
int click_x = -1, click_y = -1;
int esc_pressed = 0;

void lobby_menu();
void display_leaderboard();
void display_track_selector();
void sort_leaderboard();

void register_menu();
int login_menu();
void user_menu();

void save_leaderboard();
void retrieve_leaderboard();
void read_track_data(int track_id);

int is_valid_username(char *name);
int is_file_empty();
int find_player(char *name);
int get_int_input();
void set_color(int color);
void gotoxy(int x, int y);
void hide_cursor(); // NEW
void enable_mouse_input();
void process_inputs(); // NEW

void play_audio(int file);
void play_track(int file);

void spawn_note(Note n);
void clear_note(Note n);
void handle_game();
int calculate_score(float time_diff);
void end_track_results();

// ========================================
// ================= MENU =================
// ========================================

int main() {
    retrieve_leaderboard();
    enable_mouse_input();
    hide_cursor(); // FIX: Hide the blinking console cursor for a cleaner look
    lobby_menu();
    return 0;
}

void lobby_menu(){
    int choice;

    while(1){
        system("cls");
        set_color(13);
        printf("--= C OSU =--\n");
        set_color(7);
        printf("[1] PLAY\n");
        printf("[2] SHOW LEADERBOARD\n\n");
        set_color(4);
        printf("[0] QUIT\n\n");
        set_color(7);
        printf("Choice: ");

        choice = get_int_input();

        switch(choice){
            case 1:
                if(is_file_empty()){
                    printf("\nNo users found. Please register first.\n");
                    register_menu();
                    current_player_index = last_lb;
                }
                if(logged_in == 1)
                    display_track_selector();
                else
                    user_menu();
                break;
            case 2:
                display_leaderboard();
                break;
            case 0:
                save_leaderboard();
                printf("Goodbye!\n");
                exit(0);
            default:
                printf("Invalid choice.\n");
        }
    }
}

void user_menu(){
    system("cls");
    int choice;

    printf("--= C OSU =--\n");
    printf("[1] LOGIN\n");
    printf("[2] REGISTER\n\n");
    printf("[0] BACK\n\n");
    printf("Choice: ");

    choice = get_int_input();

    switch(choice){
        case 1: {
            int player_index = login_menu();
            if(player_index != -1){
                logged_in = 1;
                current_player_index = player_index;
                display_track_selector();
            }
            break;
        }
        case 2:
            register_menu();
            if(logged_in == 1){
                current_player_index = last_lb;
                display_track_selector();
            }
            break;
        case 0:
            return;
        default:
            printf("Invalid input.\n");
            system("pause");
            play_audio(1); 
    }
}

void register_menu(){
    system("cls");
    char name[NAME_LEN];

    printf("--= REGISTER USER =--\n");
    printf("Enter username (letters only, max 15 chars): ");
    if (!fgets(name, sizeof(name), stdin)) {
        printf("Input error.\n");
        return;
    }
    name[strcspn(name, "\n")] = 0;

    if(strlen(name) == 0 || !is_valid_username(name)){
        printf("Invalid username.\n");
        return;
    }

    if(find_player(name) != -1){
        printf("Username already exists.\n");
        return;
    }

    if(last_lb >= MAX_PLAYERS - 1){
        printf("Leaderboard full.\n");
        return;
    }

    last_lb++;
    strncpy(leaderboard[last_lb].name, name, NAME_LEN-1);
    leaderboard[last_lb].name[NAME_LEN-1] = '\0';
    leaderboard[last_lb].score_1 = 0;
    leaderboard[last_lb].score_2 = 0;
    leaderboard[last_lb].score_3 = 0;

    logged_in = 1;
    play_audio(1);
    printf("Registration successful!\n");
    Sleep(1000);
}

int login_menu(){
    system("cls");
    char name[NAME_LEN];

    printf("--= LOGIN USER =--\n");
    printf("Enter username: ");
    if (!fgets(name, sizeof(name), stdin)) {
        printf("Input error.\n");
        return -1;
    }
    name[strcspn(name, "\n")] = 0;

    int index = find_player(name);

    if(index == -1){
        printf("User not found.\n");
        system("pause");
        return -1;
    }

    printf("Welcome, %s!\n", name);
    play_audio(1);
    Sleep(1000);
    return index;
}

void display_track_selector(){
    system("cls");
    int choice;

    printf("Select Track:\n");
    printf("[1] Track 1\n");
    printf("[2] Track 2\n");
    printf("[3] Track 3\n");
    printf("[4] Tutorial\n");
    printf("[0] Back\n");
    printf("Choice: ");

    choice = get_int_input();

    if(choice < 0 || choice > 4) {
        printf("Invalid track selection.\n");
        system("pause");
        return;
    }
    if(choice == 0) return;
    
    play_audio(2);
    read_track_data(choice);

    if(last_tr == -1){
        printf("Track not found or empty!\n");
        system("pause");
        return;
    }

    current_track_id = choice;
    current_score = 0;
    notes_hit = 0;
    notes_missed = 0;
    last_mouse_x = -1; last_mouse_y = -1;
    prev_mouse_x = -1; prev_mouse_y = -1;
    
    track_playing = 1;
    track_start_time = GetTickCount();

    // FIX: Actually start the music track here!
    play_track(choice);

    system("cls");

    while(track_playing){
        handle_game();
    }
    
    // FIX: Stop the track when exiting the loop
    mciSendString("close bgm", NULL, 0, NULL);
}

void read_track_data(int track_id){
    char filepath[32];
    last_tr = -1;

    switch(track_id){
        case 1: strcpy(filepath, "track_1.txt"); break;
        case 2: strcpy(filepath, "track_2.txt"); break;
        case 3: strcpy(filepath, "track_3.txt"); break;
        case 4: strcpy(filepath, "track_tuto.txt"); break;
        default: return;
    }

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        printf("Track file '%s' not found!\n", filepath);
        return;
    }

    while(last_tr < 499 && fscanf(fp, "%d,%d,%f",
          &track[last_tr+1].x,
          &track[last_tr+1].y,
          &track[last_tr+1].spawn_time) == 3){

        last_tr++;
        track[last_tr].is_hit = 0;
        track[last_tr].is_active = 0;
    }
    fclose(fp);
}

void sort_leaderboard(){
    char current_name[NAME_LEN] = "";
    if(current_player_index >= 0 && current_player_index <= last_lb){
        strcpy(current_name, leaderboard[current_player_index].name);
    }

    for(int i = 0; i <= last_lb; i++){
        for(int j = i + 1; j <= last_lb; j++){
            int total_i = leaderboard[i].score_1 + leaderboard[i].score_2 + leaderboard[i].score_3;
            int total_j = leaderboard[j].score_1 + leaderboard[j].score_2 + leaderboard[j].score_3;

            if(total_j > total_i){
                Player temp = leaderboard[i];
                leaderboard[i] = leaderboard[j];
                leaderboard[j] = temp;
            }
        }
    }

    if(strlen(current_name) > 0){
        current_player_index = find_player(current_name);
    }
}

void display_leaderboard(){
    system("cls");
    sort_leaderboard();

    printf("%-4s %-16s %-8s %-8s %-8s %-10s\n",
           "No.", "Username", "T1", "T2", "T3", "Total");

    int count = (last_lb + 1 > 10) ? 10 : last_lb + 1;

    for(int i = 0; i < count; i++){
        int total = leaderboard[i].score_1 + leaderboard[i].score_2 + leaderboard[i].score_3;
        printf("%-4d %-16s %-8d %-8d %-8d %-10d\n",
               i+1,
               leaderboard[i].name,
               leaderboard[i].score_1,
               leaderboard[i].score_2,
               leaderboard[i].score_3,
               total);
    }
    printf("\n");
    system("pause");
}

void save_leaderboard(){
    FILE *fp = fopen("leaderboard.txt", "w");
    if (!fp) return;

    for(int i = 0; i <= last_lb; i++){
        fprintf(fp, "%s,%d,%d,%d\n",
                leaderboard[i].name,
                leaderboard[i].score_1,
                leaderboard[i].score_2,
                leaderboard[i].score_3);
    }
    fclose(fp);
}

void retrieve_leaderboard(){
    FILE *fp = fopen("leaderboard.txt", "r");
    if (!fp) {
        last_lb = -1;
        return;
    }

    last_lb = -1;
    while(last_lb < MAX_PLAYERS - 1 && fscanf(fp, "%15[^,],%d,%d,%d\n",
                 leaderboard[last_lb + 1].name,
                 &leaderboard[last_lb + 1].score_1,
                 &leaderboard[last_lb + 1].score_2,
                 &leaderboard[last_lb + 1].score_3) == 4){
        last_lb++;
    }
    fclose(fp);
}

int is_valid_username(char *name){
    for(int i = 0; name[i]; i++){
        if(!isalpha(name[i])) return 0;
    }
    return 1;
}

int find_player(char *name){
    for(int i = 0; i <= last_lb; i++){
        if(strcmp(leaderboard[i].name, name) == 0){
            return i;
        }
    }
    return -1;
}

int is_file_empty(){
    FILE *fp = fopen("leaderboard.txt", "r");
    if (!fp) return 1;
    int c = fgetc(fp);
    fclose(fp);
    return (c == EOF);
}

int get_int_input(){
    char buf[32];
    int value;
    if (!fgets(buf, sizeof(buf), stdin)) {
        return -999;
    }
    if (sscanf(buf, "%d", &value) != 1) {
        return -999;
    }
    return value;
}

void set_color(int color){
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void gotoxy(int x, int y){
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void hide_cursor() {
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    info.dwSize = 100;
    info.bVisible = FALSE;
    SetConsoleCursorInfo(consoleHandle, &info);
}

void enable_mouse_input(){
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if(!GetConsoleMode(hInput, &mode)) return;
    mode |= ENABLE_MOUSE_INPUT;
    mode &= ~ENABLE_QUICK_EDIT_MODE; // Prevents the console from pausing when clicked
    SetConsoleMode(hInput, mode);
}

// FIX: A single unified input processing loop that prevents buffer flooding lag.
void process_inputs() {
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD numEvents = 0;
    
    GetNumberOfConsoleInputEvents(hInput, &numEvents);
    
    click_x = -1;
    click_y = -1;
    esc_pressed = 0;

    // Drain all events generated since the last frame
    while (numEvents > 0) {
        INPUT_RECORD inRec;
        DWORD numRead;
        ReadConsoleInput(hInput, &inRec, 1, &numRead);

        if (inRec.EventType == MOUSE_EVENT) {
            MOUSE_EVENT_RECORD m = inRec.Event.MouseEvent;
            
            last_mouse_x = m.dwMousePosition.X;
            last_mouse_y = m.dwMousePosition.Y;

            // Register a click only on down-press (dwEventFlags == 0)
            if (m.dwEventFlags == 0 && (m.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) {
                click_x = last_mouse_x;
                click_y = last_mouse_y;
                play_audio(4);
            }
        } else if (inRec.EventType == KEY_EVENT) {
            if (inRec.Event.KeyEvent.bKeyDown && inRec.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE) {
                esc_pressed = 1;
            }
        }
        GetNumberOfConsoleInputEvents(hInput, &numEvents);
    }
}

void play_audio(int file){
    switch(file){
        case 1: PlaySound(TEXT("menu_press.wav"), NULL, SND_FILENAME | SND_ASYNC); break;
        case 2: PlaySound(TEXT("track_selected.wav"), NULL, SND_FILENAME | SND_ASYNC); break;
        case 3: PlaySound(TEXT("end_track.wav"), NULL, SND_FILENAME | SND_ASYNC); break;
        case 4: PlaySound(TEXT("hit_key.wav"), NULL, SND_FILENAME | SND_ASYNC); break;
        default: return;
    }
}

void play_track(int file){
    switch(file){
        case 1: mciSendString("open \"track_1.mp3\" type mpegvideo alias bgm", NULL, 0, NULL); break;
        case 2: mciSendString("open \"track_2.mp3\" type mpegvideo alias bgm", NULL, 0, NULL); break;
        case 3: mciSendString("open \"track_3.mp3\" type mpegvideo alias bgm", NULL, 0, NULL); break;
        case 4: mciSendString("open \"track_tuto.mp3\" type mpegvideo alias bgm", NULL, 0, NULL); break;
        default: return;
    }
    mciSendString("play bgm", NULL, 0, NULL);
}

int calculate_score(float time_diff){
    float abs_diff = fabs(time_diff);
    if(abs_diff <= 1) return 300;      
    if(abs_diff <= 2) return 200;       
    if(abs_diff <= 3) return 100;       
    return 0;
}

void spawn_note(Note n){
    set_color(4);
    gotoxy(n.x, n.y); printf(" ");
    gotoxy(n.x + 1, n.y); printf("|");
    gotoxy(n.x - 1, n.y); printf("|");
    gotoxy(n.x, n.y + 1); printf("-");
    gotoxy(n.x, n.y - 1); printf("-");
    gotoxy(n.x + 1, n.y + 1); printf("/");
    gotoxy(n.x - 1, n.y - 1); printf("/");
    gotoxy(n.x - 1, n.y + 1); printf("\\");
    gotoxy(n.x + 1, n.y - 1); printf("\\");
}

void clear_note(Note n){
    for(int dx = -1; dx <= 1; dx++){
        for(int dy = -1; dy <= 1; dy++){
            gotoxy(n.x + dx, n.y + dy);
            printf(" ");
        }
    }
}

void end_track_results(){
    system("cls");
    set_color(10);
    printf("--= TRACK COMPLETE =--\n");
    set_color(7);
    printf("Notes Hit: %d\n", notes_hit);
    printf("Notes Missed: %d\n", notes_missed);
    printf("Total Score: %d\n", current_score);

    if(current_track_id == 4){
        set_color(14);
        printf("\n(Tutorial scores are not saved to leaderboard)\n");
    }

    set_color(7);
    printf("\nPress any key to continue...\n");
    play_audio(3);
    
    // Drain remaining inputs before pause
    while(_kbhit()) _getch(); 
    system("pause");
}

void handle_game(){
    float current_time = (float)(GetTickCount() - track_start_time) / 1000.0f;

    // FIX: Removed `system("cls")` here. Overwriting using `gotoxy` avoids screen tearing.

    process_inputs();

    if(esc_pressed){
        track_playing = 0;
        system("cls");
        printf("Track cancelled.\n");
        system("pause");
        return;
    }

    // Padded string strings act as a clear line
    gotoxy(0,0);
    set_color(7);
    printf("Score: %-6d | Hit: %-3d | Missed: %-3d   ", current_score, notes_hit, notes_missed);
    gotoxy(0, 1);
    printf("Time: %.2f   [ESC to quit]       ", current_time);

    // Erase old mouse cursor
    if(prev_mouse_x >= 0 && prev_mouse_y >= 0 && (prev_mouse_x != last_mouse_x || prev_mouse_y != last_mouse_y)){
        gotoxy(prev_mouse_x, prev_mouse_y);
        printf(" ");
    }

    // Draw new mouse position
    if(last_mouse_x >= 0 && last_mouse_y >= 0){
        gotoxy(last_mouse_x, last_mouse_y);
        set_color(14);
        printf("O");
        set_color(7);
        prev_mouse_x = last_mouse_x;
        prev_mouse_y = last_mouse_y;
    }

    // Process hits
    if(click_x != -1 && click_y != -1){
        for(int i = 0; i <= last_tr; i++){
            if(!track[i].is_active || track[i].is_hit)
                continue;

            float time_diff = current_time - track[i].spawn_time;

            if(click_x >= track[i].x - 1 && click_x <= track[i].x + 1 &&
               click_y >= track[i].y - 1 && click_y <= track[i].y + 1){

                if(fabs(time_diff) <= 3){
                    int score = calculate_score(time_diff);
                    current_score += score;
                    notes_hit++;
                    track[i].is_hit = 1;
                    clear_note(track[i]);
                    break;
                }
            }
        }
    }

    for(int i = 0; i <= last_tr; i++){
        // FIX: Only draw the note exactly ONCE when it becomes active. Avoid drawing it every frame.
        if(!track[i].is_active && current_time >= track[i].spawn_time){
            track[i].is_active = 1;
            spawn_note(track[i]); 
        }

        if(track[i].is_active && !track[i].is_hit &&
           current_time > track[i].spawn_time + 3){
            notes_missed++;
            track[i].is_hit = 1;
            clear_note(track[i]); // Erase from screen when missed
        }
    }

    if(current_time > track[last_tr].spawn_time + 3.0){
        track_playing = 0;
        end_track_results();

        if(current_player_index >= 0 && current_player_index <= last_lb){
            if(current_track_id == 1 && current_score > leaderboard[current_player_index].score_1){
                leaderboard[current_player_index].score_1 = current_score;
            } else if(current_track_id == 2 && current_score > leaderboard[current_player_index].score_2){
                leaderboard[current_player_index].score_2 = current_score;
            } else if(current_track_id == 3 && current_score > leaderboard[current_player_index].score_3){
                leaderboard[current_player_index].score_3 = current_score;
            }

            if(current_track_id >= 1 && current_track_id <= 3){
                save_leaderboard();
            }
        }
    }

    Sleep(30); // Slight frame limiter for console friendliness
}