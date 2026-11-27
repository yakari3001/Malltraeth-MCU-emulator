#include <ncurses.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define NUM_CPUS 5      // CPU 0 = Red Activity LED, CPUs 1..4 = Matrix rows
#define MATRIX_ROWS 4
#define MATRIX_COLS 16

#define uchar unsigned char

#define nop 0
#define lacc 1
#define inca 2
#define deca 3
#define ldptr 4
#define wait 5
#define ldab 6
#define movad 7
#define movda 8
#define addc 9
#define subc 10
#define addcd 11
#define subcd 12


uchar prg1[]={1,2,3,4};

typedef struct {
    uint16_t ac;
    uint16_t pc;        // Program counter
    uint16_t out_reg;   // Output latch (LATx)
    uint16_t prescaler; // Clock divider (16-bit so it can divide a 1kHz clock)
    uint16_t tick_cnt;
    bool     active;
    uchar* pmem;
    uchar dmem[16];
    uint16_t dptr;
    
} cpuinstance;

void nop_f(cpuinstance* l)
{}
void lacc_f(cpuinstance* l)
{}
void inca_f(cpuinstance* l)
{}
void deca_f(cpuinstance* l)
{}
 void ldptr_f(cpuinstance* l)
{}
void wait_f(cpuinstance* l)
{}
void ldab_f(cpuinstance* l)
{}
void movad_f(cpuinstance* l)
{}
void movda_f(cpuinstance* l)
{}
void addc_f(cpuinstance* l) 
{}
void subc_f(cpuinstance* l)
{}
void addcd_f(cpuinstance* l)
{}
void subcd_f(cpuinstance* l)
{}


long long millis() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL);
}

// Single-step execution (called at 1 kHz / every 1 ms)
void cpu_single_step(cpuinstance *cpu, int id) {
    if (!cpu->active) return;

    cpu->tick_cnt++;
    if (cpu->tick_cnt < cpu->prescaler) return;
    cpu->tick_cnt = 0;

    if (id == 0) {
        // --- CPU 0: Red Activity LED ---
        // Prescaler = 35ms per step * 10 steps = 350ms period (~2.85 Hz)
        // Step 0 is ON (35ms), Steps 1..9 are OFF (315ms) -> Exact 10% duty cycle
        uint16_t step = cpu->pc % 10;
        cpu->out_reg = (step == 0) ? 1 : 0;
        cpu->pc++;
    } else {
        // --- CPUs 1..4: Matrix Shift Pattern ---
        // Prescaler = 120ms per shift so you can clearly see the afterglow trail
        cpu->out_reg = (1 << (cpu->pc % 16));
        cpu->pc++;
    }
}

int main() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    timeout(0); // Completely non-blocking input

    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_RED, COLOR_BLACK);    // Pair 1: Red Activity LED
        init_pair(2, COLOR_GREEN, COLOR_BLACK);  // Pair 2: Matrix Green LEDs
        init_pair(3, COLOR_WHITE, COLOR_BLACK);  // Pair 3: Off grid dots
    }

    cpuinstance cpus[NUM_CPUS];

    // CPU 0: 35ms per step -> 35ms ON, 315ms OFF (10% duty cycle)
    cpus[0].pc = 0;
    cpus[0].out_reg = 0;
    cpus[0].prescaler = 35;
    cpus[0].tick_cnt = 0;
    cpus[0].active = true;

    // CPUs 1..4: 120ms per shift step
    for (int i = 1; i < NUM_CPUS; i++) {
        cpus[i].pc = i * 3;
        cpus[i].out_reg = 0;
        cpus[i].prescaler = 120;
        cpus[i].tick_cnt = 0;
        cpus[i].active = true;
    }

    // Afterglow brightness buffer (0 = off, 1 = faint, 2 = medium, 3 = full bright)
    uint8_t glow[MATRIX_ROWS][MATRIX_COLS] = {0};
    uint8_t activity_glow = 0;

    // --- TWO INDEPENDENT TIMERS ---
    const int cpu_tick_ms = 1;       // Timer 1: 1,000 Hz CPU clock
    const int display_tick_ms = 25;  // Timer 2: 40 Hz Display scan + decay step

    long long next_cpu_tick = millis() + cpu_tick_ms;
    long long next_display_tick = millis() + display_tick_ms;

    uint32_t total_steps = 0;
    bool paused = false;

    while (1) {
        long long now = millis();

        // 1. FAST TIMER (1 kHz): Execute CPU emulator steps
        while (!paused && now >= next_cpu_tick) {
            next_cpu_tick += cpu_tick_ms;
            if (next_cpu_tick < now - 50) next_cpu_tick = now + cpu_tick_ms;

            for (int i = 0; i < NUM_CPUS; i++) {
                cpu_single_step(&cpus[i], i);
            }
            total_steps++;
        }

        // 2. DISPLAY TIMER (40 Hz): Sample LEDs, apply decay, and render
        if (now >= next_display_tick) {
            next_display_tick += display_tick_ms;
            if (next_display_tick < now) next_display_tick = now + display_tick_ms;

            // Update Activity LED brightness with decay
            if (cpus[0].out_reg & 1) {
                activity_glow = 3;
            } else if (activity_glow > 0) {
                activity_glow--;
            }

            // Update Matrix brightness buffer with decay
            for (int r = 0; r < MATRIX_ROWS; r++) {
                uint16_t reg = cpus[r + 1].out_reg;
                for (int bit = 15; bit >= 0; bit--) {
                    int col = 15 - bit;
                    bool led_on = (reg >> bit) & 1;

                    if (led_on) {
                        glow[r][col] = 3; // Charge pixel to full brightness
                    } else if (glow[r][col] > 0) {
                        glow[r][col]--;   // Decay by 1 level per display scan
                    }
                }
            }

            // Draw UI Header
            mvprintw(0, 2, "Malltraeth MCU | CPU cycle: 1kHz | Scan: 40Hz");
            mvprintw(1, 2, "State: %-7s | Steps: %u   ", paused ? "PAUSED" : "RUNNING", total_steps);

mvprintw(11,2,"reprogrammed bytecode emulator (c) 2026");
mvprintw(12,2,"built using AI and descriptions of the PIC24F script");
mvprintw(13,2,"v 0.82");

            // Draw Red Activity LED with 3-step decay
            mvprintw(2, 2, "Activity LED (CPU 0): ");
            if (activity_glow == 3) {
                attron(COLOR_PAIR(1) | A_BOLD);
                mvaddch(2, 24, '*');
                attroff(COLOR_PAIR(1) | A_BOLD);
            } else if (activity_glow == 2) {
                attron(COLOR_PAIR(1));
                mvaddch(2, 24, '+');
                attroff(COLOR_PAIR(1));
            } else if (activity_glow == 1) {
                attron(COLOR_PAIR(1) | A_DIM);
                mvaddch(2, 24, '.');
                attroff(COLOR_PAIR(1) | A_DIM);
            } else {
                mvaddch(2, 24, ' ');
            }

            // Draw LED Matrix with comet-tail afterglow
            for (int r = 0; r < MATRIX_ROWS; r++) {
                uint16_t reg = cpus[r + 1].out_reg;
                mvprintw(4 + r, 2, "R%d [%04X]: ", r, reg);

                for (int col = 0; col < MATRIX_COLS; col++) {
                    int screen_x = 13 + (col * 2);
                    uint8_t level = glow[r][col];

                    if (level == 3) {
                        // Full ON: Bright Bold Green '*'
                        attron(COLOR_PAIR(2) | A_BOLD);
                        mvaddch(4 + r, screen_x, '*');
                        attroff(COLOR_PAIR(2) | A_BOLD);
                    } else if (level == 2) {
                        // Medium Afterglow: Normal Green '+'
                        attron(COLOR_PAIR(2));
                        mvaddch(4 + r, screen_x, '+');
                        attroff(COLOR_PAIR(2));
                    } else if (level == 1) {
                        // Faint Trail: Dim Green 'o' or '.'
                        attron(COLOR_PAIR(2) | A_DIM);
                        mvaddch(4 + r, screen_x, '.');
                        attroff(COLOR_PAIR(2) | A_DIM);
                    } else {
                        // Completely OFF
                        attron(COLOR_PAIR(3) | A_DIM);
                        mvaddch(4 + r, screen_x, ' ');
                        attroff(COLOR_PAIR(3) | A_DIM);
                    }
                }
            }

            mvprintw(9, 2, "[p]ause  [s]tep  [q]uit");
            refresh();
        }

        // 3. Controls
        int ch = getch();
        if (ch == 'q' || ch == 'Q') break;
        if (ch == 'p' || ch == 'P') {
            paused = !paused;
            next_cpu_tick = millis() + cpu_tick_ms;
        }
        if ((ch == 's' || ch == 'S') && paused) {
            for (int i = 0; i < NUM_CPUS; i++) {
                cpu_single_step(&cpus[i], i);
            }
            total_steps++;
        }

        // Sleep 500 microseconds (0.5ms)
        struct timespec req = {0, 500000L};
        nanosleep(&req, NULL);
    }

    endwin();
    return 0;
}
