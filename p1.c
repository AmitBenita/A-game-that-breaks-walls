
#include <dos.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // Added for exit() function

#define ARRSIZE 1000
#define BALL_NUMBER 5
#define TARGET_SIZE 5
#define TARGET_NUMBER 12
#define BALL_MAX_X 78
#define BALL_MAX_Y 23
#define BALL_MIN_X 1
#define BALL_MIN_Y 1

void interrupt (*old_int9)(void);
void interrupt (*old_int8)(void);

// Function prototypes
void interrupt new_int9(void);
void interrupt new_int8(void);
void my_halt(void);
void init_interrupts(void);
void restore_interrupts(void);
void displayer(void);
void receiver(void);
void updater(void);
void update_target(int i);
void update_ball(void);
void terminate_application(void);

char entered_ascii_codes[ARRSIZE];
int tail = -1;
char display[2001];

char ch_arr[ARRSIZE];
int front = -1;
int rear = -1;
int ball_speed_x = 1;
int ball_speed_y = 1;
int cycle_time = 25; // Initial cycle time in ticks (100 ticks = 1 second)

typedef struct position {
    int x;
    int y;
} POSITION;

POSITION target_pos[TARGET_NUMBER];
POSITION ball_pos[BALL_NUMBER];
POSITION ball_angle[BALL_NUMBER];
int game_over;
int block_position;
int block_size;
int block_in_motion;
int initial_run = 1;
int tick_count = 0;
char display_draft[25][80]; // Define display_draft

int no_of_targets; // Define no_of_targets globally
int current_stage = 1; // Define current_stage and initialize

void my_halt() {
    restore_interrupts(); // Ensure interrupts are restored before exiting
    asm {CLI}
    exit(0);
}

void interrupt new_int9(void) {
    unsigned char scan; // Use unsigned char for scan code
    char ascii = 0;
    static int ctrl_pressed = 0; // To track if Ctrl is pressed

    asm {
        IN AL, 0x60       // Read scan code from port 0x60
        MOV BYTE PTR scan, AL
        IN AL, 0x61       // Read and acknowledge interrupt
        OR AL, 0x80
        OUT 0x61, AL
        AND AL, 0x7F
        OUT 0x61, AL
    }

    // Detect Ctrl key press
    if (scan == 0x1D) { // Ctrl key down
        ctrl_pressed = 1;
    } else if (scan == 0x9D || scan == 0x1D) { // Ctrl key up
        ctrl_pressed = 0;
    }

    // Detect other keys
    if (ctrl_pressed && scan == 0x2E) { // C key while Ctrl is pressed
        my_halt(); // Terminate program
    } else if (scan == 0x4B) { // Left arrow
        ascii = 'a';
    } else if (scan == 0x48) { // Up arrow
        ascii = 'w';
    } else if (scan == 0x4D) { // Right arrow
        ascii = 'd';
    } else if (scan == 0x50) { // Down arrow
        ascii = 's';
    } else if (scan == 0x01) { // Esc
        my_halt(); // Terminate program
    }

    // Ensure we do not exceed the bounds of entered_ascii_codes
    if (ascii != 0 && tail < ARRSIZE - 1) {
        entered_ascii_codes[++tail] = ascii;
    }

    (*old_int9)(); // Call the old interrupt handler
}

void interrupt new_int8(void) {
    tick_count++;
    if (tick_count >= cycle_time) { // Use cycle_time to control the tick frequency
        receiver();
        updater();
        displayer();
        tick_count = 0;
    }
    (*old_int8)(); // Call the old interrupt handler
}


void init_interrupts() {
    old_int8 = getvect(8);
    setvect(8, new_int8);
    old_int9 = getvect(9);
    setvect(9, new_int9);
}

void restore_interrupts() {
    setvect(8, old_int8);
    setvect(9, old_int9);
}

void displayer() {
    char far* screen = (char far*)0xB8000000;
    int i, j;
    char stage_message[20];
    int len;
    int start;

    // Clear screen
    for (i = 0; i < 25; i++) {
        for (j = 0; j < 80; j++) {
            screen[(i * 80 + j) * 2] = display[i * 80 + j];
            if (i == 24 && display[i * 80 + j] == '=') {
                screen[(i * 80 + j) * 2 + 1] = 0x1A; // Blue background, light green text
            } else if (display[i * 80 + j] == '*') {
                screen[(i * 80 + j) * 2 + 1] = 0x1F; // Blue background, white text
            } else if (display[i * 80 + j] == '#') {
                screen[(i * 80 + j) * 2 + 1] = 0x27; // Black background, green text
            } else if (i == 0) { // Upper walls
                screen[(i * 80 + j) * 2 + 1] = 0x4F; // Red background, white text
            } else {
                screen[(i * 80 + j) * 2 + 1] = 0x70; // Light gray background, black text
            }
        }
    }
    



    // Display current stage number
    sprintf(stage_message, "Stage: %d", current_stage);
    len = strlen(stage_message);
    start = 0; // Center the message on the screen

    for (i = 0; i < len; i++) {
        screen[(22 * 80 + start + i) * 2] = stage_message[i];
        screen[(22 * 80 + start + i) * 2 + 1] = 0x0F; // White text on black background
    }
}


void receiver() {
    char temp;
    while (tail > -1) {
        temp = entered_ascii_codes[tail];
        rear++;
        tail--;
        if (rear < ARRSIZE)
            ch_arr[rear] = temp;
        if (front == -1)
            front = 0;
    }
}

int ball_hit_right_wall() {
    return (ball_pos[0].x == BALL_MAX_X && ball_angle[0].x > 0);
}

int ball_hit_left_wall() {
    return (ball_pos[0].x == BALL_MIN_X && ball_angle[0].x < 0);
}

int ball_hit_block() {
    return (ball_pos[0].y >= BALL_MAX_Y && ball_pos[0].x >= block_position &&
            ball_pos[0].x <= block_position + block_size);
}

int fall_off() {
    return (ball_pos[0].y >= BALL_MAX_Y && ball_angle[0].y > 0 && ball_hit_block() == 0);
}

int ball_hit_roof() {
    return (ball_pos[0].y == BALL_MIN_Y && ball_angle[0].y < 0);
}

void reverse_angle_roof() {
    ball_angle[0].y = -ball_angle[0].y;
}


void reverse_angle_bottom() {
    ball_angle[0].y = -ball_angle[0].y;
}

void reverse_angle_left() {
    ball_angle[0].x = -ball_angle[0].x;
}

void reverse_angle_right() {
    ball_angle[0].x = -ball_angle[0].x;
}

void terminate_application() {
    int i, j;
    char* message = "GAME OVER";
    int len = strlen(message);
    int start = (80 - len) / 2;

    for (i = 0; i < 25; i++) {
        for (j = 0; j < 80; j++) {
            display[i * 80 + j] = ' ';
        }
    }
    for (i = 0; i < len; i++) {
        display[12 * 80 + start + i] = message[i];
    }
    displayer();
    delay(3000); // Display the game over message for 3 seconds
    printf("Game over\n");
    restore_interrupts(); // Ensure interrupts are restored before exiting
    asm INT 27;
}

void update_target(int i) {
    if ((ball_pos[0].y == BALL_MIN_Y) &&
        (ball_pos[0].x >= target_pos[i].x) &&
        (ball_pos[0].x <= target_pos[i].x + TARGET_SIZE)) {
        target_pos[i].x = -1; // Mark target as hit
    }
}

void update_ball() {
    if (fall_off()) {
        terminate_application();
    }

    if (ball_hit_right_wall() || ball_hit_left_wall()) {
        reverse_angle_left();
    }
    if (ball_hit_block()) {
        reverse_angle_bottom();
    }
    if (ball_hit_roof()) {
        reverse_angle_roof();
    }

    
    ball_pos[0].x += ball_angle[0].x * ball_speed_x;
    ball_pos[0].y += ball_angle[0].y * ball_speed_y;

    if (ball_pos[0].x < BALL_MIN_X) ball_pos[0].x = BALL_MIN_X;
    if (ball_pos[0].x > BALL_MAX_X) ball_pos[0].x = BALL_MAX_X;
    if (ball_pos[0].y < BALL_MIN_Y) ball_pos[0].y = BALL_MIN_Y;
    if (ball_pos[0].y > BALL_MAX_Y) ball_pos[0].y = BALL_MAX_Y;
}


void updater() {
    int i, j;
    int no_of_balls = 1; // Ensure no_of_balls is properly initialized
    int target_disp;
    int all_targets_hit;

    target_disp = 80 / TARGET_NUMBER;
    all_targets_hit = 1;

    if (initial_run == 1) {
        no_of_targets = 12;
        for (i = 0; i < no_of_targets; i++) {
            target_pos[i].x = (i * target_disp) + (target_disp / 2);
            target_pos[i].y = 1;
        }
        ball_pos[0].x = BALL_MAX_X / 2;
        ball_pos[0].y = BALL_MAX_Y - 1;
        ball_angle[0].x = 2;
        ball_angle[0].y = -2;
        block_size = 10;
        block_position = (BALL_MAX_X - block_size) / 2;
        initial_run = 0;
    }

    if (block_in_motion == 0) {
        char temp;
        while (front <= rear && front != -1) {
            temp = ch_arr[front++];
            if (front > rear)
                front = rear = -1;
            switch (temp) {
            case 'a':
                if (block_position > BALL_MIN_X)
                    block_position--;
                break;
            case 'd':
                if (block_position + block_size < BALL_MAX_X)
                    block_position++;
                break;
            default:
                break;
            }
        }
    }

    for (i = 0; i < no_of_balls; i++)
        update_ball();

    for (i = 0; i < no_of_targets; i++) {
        if (target_pos[i].x != -1) {
            all_targets_hit = 0;
            break;
        }
    }

    if (all_targets_hit) {
        current_stage++; // Move to the next stage
        no_of_targets = 12; 
        for (i = 0; i < no_of_targets; i++) {
            target_pos[i].x = (i * target_disp) + (target_disp / 2);
            target_pos[i].y = 1;
        }
        ball_pos[0].x = BALL_MAX_X / 2;
        ball_pos[0].y = BALL_MAX_Y - 1;
        ball_angle[0].x = 1;
        ball_angle[0].y = -1;
        block_size = 10;
        block_position = (BALL_MAX_X - block_size) / 2;

        // Decrease cycle time and increase ball speed for the next stage
        if (cycle_time > 10) { // Ensure the minimum cycle time is not too low
            cycle_time -= 3; // Decrease cycle time gradually
        }
        ball_speed_x += 1;
        ball_speed_y += 1;
    }

    // Update targets and display
    for (i = 0; i < no_of_targets; i++)
        update_target(i);

    for (i = 0; i < 25; i++) {
        for (j = 0; j < 80; j++) {
            display_draft[i][j] = ' ';
        }
    }

    for (i = 0; i < no_of_targets; i++) {
        if (target_pos[i].x != -1) {
            for (j = 0; j < TARGET_SIZE; j++) {
                display_draft[target_pos[i].y][target_pos[i].x + j] = '*';
            }
        }
    }

    for (i = 0; i < no_of_balls; i++) {
        display_draft[ball_pos[i].y][ball_pos[i].x] = '#';
    }

    for (i = 0; i < block_size; i++) {
        display_draft[24][block_position + i] = '=';
    }

    for (i = 0; i < 25; i++) {
        for (j = 0; j < 80; j++) {
            display[i * 80 + j] = display_draft[i][j];
        }
    }
}


int main() {
    asm {
        MOV AH, 3
        INT 10h
    }

    init_interrupts();

    while (!game_over) {
        // Main game loop
        // The game will continue running until game_over becomes true
        // This is a placeholder for the actual game logic
        delay(10); // Slow down the loop slightly
    }

    restore_interrupts();
    return 0;
}