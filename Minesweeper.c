#include <stdlib.h>  // Standard library for memory allocation and rand()
#include <string.h>  // Header for string manipulation (though unused here)
#include <assert.h>  // Header for diagnostic assertions
#include <stdio.h>   // Header for input/output functions like printf/scanf
#include <setjmp.h>  // Header for non-local jumps (required for CUnit)
#include <time.h>    // Header to seed the random number generator with time

#include "LibCUnit/CUnit.h" // Include CUnit framework headers
#include "LibCUnit/Basic.h" // Include CUnit basic interface

/**
 * COMP 10050 - Assignment 4 (2026)
 * Configuration constants for the game grid and difficulty.
 */
#define SIZE 8   // Define the board as an 8x8 grid
#define MINES 10 // Define the number of mines on the board

/* Structure to represent a single square on the board */
typedef struct {
    int is_mine;         // 1 if this cell contains a mine, 0 otherwise
    int revealed;        // 1 if the player has uncovered this cell
    int flagged;         // 1 if the player has marked this as a suspected mine
    int neighbor_count;  // Total count of mines in the adjacent 8 cells
} Cell;

// Declare a global 2D array of Cells to serve as our game board
Cell board[SIZE][SIZE];

// --- ASSIGNMENT REQUIREMENTS: DOUBLE VARIABLES ---

/**
 * Checks if a coordinate (passed as double) is within valid array indices.
 * Requirements met: Tests inputs < 0, == 0, and > 0.
 */
double validate_coordinate_safety(double coordinate) {
    if (coordinate < 0.0) return -1.0;           // Return -1.0 if negative (out of bounds)
    if (coordinate >= (double)SIZE) return -2.0; // Return -2.0 if too large (out of bounds)
    return 1.0;                                  // Return 1.0 if coordinate is valid (within 0-7)
}

/**
 * Calculates the current mathematical risk of hitting a mine.
 * Requirements met: Floating point division and division-by-zero protection.
 */
double calculate_risk_probability(int mines_left, int hidden_slots) {
    if (hidden_slots <= 0) return 0.0; // If no slots are hidden, risk is effectively 0
    // Perform division after casting to double to maintain decimal precision
    return (double)mines_left / (double)hidden_slots;
}

// --- CUNIT TEST SUITES & CASES ---

/* Test boundary: negative numbers */
void test_coord_lower_than_zero(void) {
    // Assert that -1.5 results in an error code of -1.0
    CU_ASSERT_DOUBLE_EQUAL(validate_coordinate_safety(-1.5), -1.0, 0.0001);
    // Assert that -5.0 results in an error code of -1.0
    CU_ASSERT_DOUBLE_EQUAL(validate_coordinate_safety(-5.0), -1.0, 0.0001);
}

/* Test boundary: the very first valid index */
void test_coord_equal_to_zero(void) {
    // Assert that exactly 0.0 is considered valid (1.0)
    CU_ASSERT_DOUBLE_EQUAL(validate_coordinate_safety(0.0), 1.0, 0.0001);
}

/* Test boundary: valid positive numbers and upper limit */
void test_coord_greater_than_zero(void) {
    // Assert that 4.0 (middle of board) is valid
    CU_ASSERT_DOUBLE_EQUAL(validate_coordinate_safety(4.0), 1.0, 0.0001);
    // Assert that 8.0 (just outside board) results in error code -2.0
    CU_ASSERT_DOUBLE_EQUAL(validate_coordinate_safety(8.0), -2.0, 0.0001);
}

/* Test standard risk math */
void test_risk_standard_values(void) {
    // 10 mines in 64 slots should be 0.15625
    CU_ASSERT_DOUBLE_EQUAL(calculate_risk_probability(10, 64), 0.15625, 0.0001);
    // 5 mines in 10 slots should be 0.5
    CU_ASSERT_DOUBLE_EQUAL(calculate_risk_probability(5, 10), 0.5, 0.0001);
}

/* Test safety: avoid crash when 0 slots are hidden */
void test_risk_zero_hidden_slots(void) {
    CU_ASSERT_DOUBLE_EQUAL(calculate_risk_probability(10, 0), 0.0, 0.0001);
}

/* Test safety: check behavior if hidden slots count is negative */
void test_risk_negative_hidden_slots(void) {
    CU_ASSERT_DOUBLE_EQUAL(calculate_risk_probability(10, -3), 0.0, 0.0001);
}

/* Register and run all unit tests */
int run_cunit_tests() {
    // Initialize the CUnit test registry
    if (CUE_SUCCESS != CU_initialize_registry()) return CU_get_error();

    // Create a suite for coordinate validation tests
    CU_pSuite coord_suite = CU_add_suite("Coordinate_Safety_Suite", NULL, NULL);
    CU_add_test(coord_suite, "Testing coordinates < 0", test_coord_lower_than_zero);
    CU_add_test(coord_suite, "Testing coordinates == 0", test_coord_equal_to_zero);
    CU_add_test(coord_suite, "Testing coordinates > 0", test_coord_greater_than_zero);

    // Create a suite for risk math tests
    CU_pSuite risk_suite = CU_add_suite("Risk_Probability_Suite", NULL, NULL);
    CU_add_test(risk_suite, "Testing standard risk logic", test_risk_standard_values);
    CU_add_test(risk_suite, "Testing zero hidden slots", test_risk_zero_hidden_slots);
    CU_add_test(risk_suite, "Testing negative hidden slots", test_risk_negative_hidden_slots);

    // Set verbose output and run all registered tests
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    // Clean up registry to free memory
    CU_cleanup_registry();
    return CU_get_error();
}

// --- GAME MENU AND INSTRUCTIONS ---

/* Simple function to print the game banner and rules */
void display_welcome_menu() {
    printf("========================================\n");
    printf("   WELCOME TO MINESWEEPER 2026          \n");
    printf("========================================\n");
    printf("INSTRUCTIONS:\n");
    printf("1. The goal is to reveal all cells that\n");
    printf("   do NOT contain a mine.\n");
    printf("2. Type '1' to REVEAL a cell.\n");
    printf("3. Type '2' to FLAG a suspected mine.\n");
    printf("4. Enter coordinates as: [Action] [Row] [Col]\n");
    printf("   Example: 1 4 4 (Reveals Row 4, Col 4)\n");
    printf("5. Numbers represent adjacent mines.\n");
    printf("========================================\n\n");
}

// --- GAME LOGIC ---

/* Loop through the grid and reset every cell to its starting state */
void initialize_board() {
    for (int i = 0; i < SIZE; i++) {       // Loop rows
        for (int j = 0; j < SIZE; j++) {   // Loop columns
            board[i][j].is_mine = 0;       // No mine
            board[i][j].revealed = 0;      // Hidden
            board[i][j].flagged = 0;       // No flag
            board[i][j].neighbor_count = 0;// No neighbor count yet
        }
    }
}

/**
 * Fisher-Yates Shuffle implementation.
 * Ensures exactly MINES mines are placed randomly across the board.
 */
void place_mines() {
    int positions[SIZE * SIZE];             // Create a flat array for 64 slots
    for (int i = 0; i < SIZE * SIZE; i++) {
        // Fill first 10 slots with 1 (mine), the rest with 0
        positions[i] = (i < MINES) ? 1 : 0;
    }

    // Shuffle the array: swap current element with a random previous one
    for (int i = (SIZE * SIZE) - 1; i > 0; i--) {
        int j = rand() % (i + 1);           // Pick random index from 0 to i
        int temp = positions[i];            // Store current value
        positions[i] = positions[j];        // Swap
        positions[j] = temp;                // Complete swap
    }

    // Map the shuffled 1D array back into our 2D game board
    for (int i = 0; i < SIZE * SIZE; i++) {
        board[i / SIZE][i % SIZE].is_mine = positions[i];
    }
}

/**
 * For every safe cell, look at the 8 neighbors and count how many are mines.
 */
void count_neighbors() {
    for (int r = 0; r < SIZE; r++) {        // Loop rows
        for (int c = 0; c < SIZE; c++) {    // Loop columns
            if (board[r][c].is_mine) continue; // Skip if this cell is a mine
            int count = 0;
            // Nested loop to check the 3x3 area around (r, c)
            for (int i = -1; i <= 1; i++) {
                for (int j = -1; j <= 1; j++) {
                    // Check if neighbor (r+i, c+j) is inside the board
                    if (validate_coordinate_safety((double)(r + i)) == 1.0 && 
                        validate_coordinate_safety((double)(c + j)) == 1.0) {
                        // If neighbor is a mine, increment count
                        if (board[r + i][c + j].is_mine) count++;
                    }
                }
            }
            board[r][c].neighbor_count = count; // Save final count to cell
        }
    }
}

/**
 * Recursive flood-fill.
 * If you reveal a cell with 0 neighbors, it reveals all surrounding cells.
 */
void reveal_cell(int r, int c) {
    // Base Case: Stop if out of bounds, already revealed, or if flagged
    if (validate_coordinate_safety((double)r) != 1.0 || 
        validate_coordinate_safety((double)c) != 1.0 || 
        board[r][c].revealed || board[r][c].flagged) {
        return;
    }

    board[r][c].revealed = 1; // Mark current cell as uncovered

    // If cell has NO mines nearby, check all 8 neighbors automatically
    if (board[r][c].neighbor_count == 0 && !board[r][c].is_mine) {
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                reveal_cell(r + i, c + j); // Recursive call
            }
        }
    }
}

/* Win if the number of revealed cells equals (Total Squares - Total Mines) */
int check_win() {
    int revealed_safe_cells = 0;
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            // Count cells that are revealed AND are not mines
            if (board[i][j].revealed && !board[i][j].is_mine) {
                revealed_safe_cells++;
            }
        }
    }
    // Return 1 if player uncovered all safe cells, 0 otherwise
    return (revealed_safe_cells == (SIZE * SIZE - MINES));
}

/* Clear stdin to prevent loops if user enters letters instead of numbers */
void clear_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF); // Consume characters until newline
}

/**
 * Draws the board in the terminal.
 * show_all is 1 during Game Over (to see all mines), 0 during gameplay.
 */
void render(int show_all) {
    printf("\n    0 1 2 3 4 5 6 7\n");     // Print column indices
    printf("   -----------------\n");
    for (int i = 0; i < SIZE; i++) {
        printf("%d | ", i);                 // Print row index
        for (int j = 0; j < SIZE; j++) {
            if (board[i][j].flagged && !show_all) printf("F ");     // Show Flag
            else if (!board[i][j].revealed && !show_all) printf(". "); // Show Hidden
            else if (board[i][j].is_mine) printf("* ");              // Show Mine
            else if (board[i][j].neighbor_count == 0) printf("  ");  // Show Empty
            else printf("%d ", board[i][j].neighbor_count);         // Show Count
        }
        printf("\n");
    }
}

// --- MAIN EXECUTION ---

int main() {
    int mode_choice;
    
    // Display the initial selection menu
    printf("========================================\n");
    printf("      MINESWEEPER 2026 BOOT MENU        \n");
    printf("========================================\n");
    printf("  1. Play the Game\n");
    printf("  2. Run CUnit Test Suites\n");
    printf("========================================\n");
    printf("Select Mode (1 or 2): ");
    
    // Validate that input is actually an integer
    if (scanf("%d", &mode_choice) != 1) {
        printf("Invalid input. Exiting.\n");
        return 1;
    }

    // Branch to Tests if chosen
    if (mode_choice == 2) {
        printf("\nStarting CUnit Tests...\n\n");
        return run_cunit_tests();
    } 
    // Exit if choice is neither 1 nor 2
    else if (mode_choice != 1) {
        printf("Invalid selection. Please run the program again.\n");
        return 1;
    }

    // --- Start Game Logic ---
    srand((unsigned int)time(NULL)); // Seed RNG once at start
    initialize_board();              // Setup board memory
    place_mines();                   // Randomly hide mines
    count_neighbors();               // Pre-calculate numbers
    
    display_welcome_menu();          // Show instructions

    int game_status = 0; // 0: playing, 1: win, -1: lose
    int r, c, act;       // Variables for Action, Row, and Col

    while (game_status == 0) {       // Game Loop
        render(0);                   // Draw board
        
        // Count currently hidden slots for the Risk HUD
        int hidden = 0;
        for(int i=0; i<SIZE; i++) {
            for(int j=0; j<SIZE; j++) {
                if(!board[i][j].revealed) hidden++;
            }
        }
        
        // Show current risk probability to the user
        printf("\nCurrent Mine Risk Probability: %.4f\n", calculate_risk_probability(MINES, hidden));
        printf("Action (1:Reveal, 2:Flag) Row Col: ");
        
        // Read 3 integers from user
        if (scanf("%d %d %d", &act, &r, &c) != 3) {
            printf("\n[!] Invalid input! Please enter three numbers (e.g., 1 3 3).\n");
            clear_buffer();          // Clear errors from stdin
            continue;
        }

        // Validate that coordinates are inside the grid
        if (validate_coordinate_safety((double)r) != 1.0 || validate_coordinate_safety((double)c) != 1.0) {
            printf("\n[!] Coordinates out of range (0-7)! Try again.\n");
            continue;
        }

        // Action 2: Toggle a Flag
        if (act == 2) {
            board[r][c].flagged = !board[r][c].flagged;
        } 
        // Action 1: Reveal a square
        else if (act == 1) {
            if (board[r][c].flagged) { // Don't allow revealing flagged squares
                printf("\n[!] Cell is flagged. Unflag it (Action 2) to reveal.\n");
                continue;
            }

            if (board[r][c].is_mine) {
                game_status = -1;    // Hit a mine = Game Over
            } else {
                reveal_cell(r, c);   // Reveal and potentially flood-fill
                if (check_win()) game_status = 1; // Check if board is cleared
            }
        } else {
            printf("\n[!] Invalid action! Use 1 to Reveal, 2 to Flag.\n");
        }
    }

    // Final board display showing mine locations
    render(1);
    if (game_status == 1) {
        printf("\n========================================\n");
        printf("   VICTORY! You cleared all safe zones!  \n");
        printf("========================================\n");
    } else {
        printf("\n========================================\n");
        printf("   GAME OVER: You triggered a mine!      \n");
        printf("========================================\n");
    }

    return 0; // Exit successfully
}