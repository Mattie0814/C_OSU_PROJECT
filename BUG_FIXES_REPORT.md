# Bug Fixes Report - C OSU Game (GP1.c)

## Summary
Found and fixed **7 critical bugs** related to mouse handling, input management, timing, and memory safety.

---

## Bugs Found and Fixed

### 1. **CRITICAL: Console Mouse Input Mode Not Enabled**
**Severity:** CRITICAL  
**Location:** Entire mouse handling system  
**Problem:** The game calls `ReadConsoleInput()` to get mouse events, but never enables the `ENABLE_MOUSE_INPUT` mode for the console. This prevents the console from queuing mouse events.

**Fix Applied:**
- Added new function `enable_mouse_input()` that:
  - Gets current console mode
  - Enables `ENABLE_MOUSE_INPUT` flag
  - Disables `ENABLE_QUICK_EDIT_MODE` to avoid conflicts
  - Sets the new mode back to console handle
- Called `enable_mouse_input()` in `main()` right after loading the leaderboard

**Code Changes:**
```c
void enable_mouse_input(){
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    
    if(!GetConsoleMode(hInput, &mode)){
        return;
    }
    
    mode |= ENABLE_MOUSE_INPUT;
    mode &= ~ENABLE_QUICK_EDIT_MODE;
    SetConsoleMode(hInput, mode);
}
```

---

### 2. **Mouse Position Detection Logic Error**
**Severity:** HIGH  
**Location:** `get_mouse_position()` function  
**Problem:** - Function reads all input records into buffer array (128 elements) instead of checking one at a time
- Processes ANY mouse event regardless of whether it's a click or position update
- Doesn't use non-blocking I/O, can hang indefinitely waiting for input
- Clears input buffer aggressively, losing keyboard events

**Fix Applied:**
- Changed to use `PeekConsoleInput()` for non-blocking input checking
- Only read/update position for non-click events (checks button state)
- Properly distinguish between position updates and clicks
- Only call `ReadConsoleInput()` when we have a valid event to process

**Code Changes:**
```c
int get_mouse_position(int *mouse_x, int *mouse_y){
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD inRec;
    DWORD numRead;

    // Non-blocking check with timeout
    if(!PeekConsoleInput(hInput, &inRec, 1, &numRead) || numRead == 0){
        return 0;
    }

    if(inRec.EventType == MOUSE_EVENT){
        MOUSE_EVENT_RECORD m = inRec.Event.MouseEvent;
        
        // Only update position if it's not a click event
        if(!(m.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)){
            ReadConsoleInput(hInput, &inRec, 1, &numRead);
            *mouse_x = m.dwMousePosition.X;
            *mouse_y = m.dwMousePosition.Y;
            return 1;
        }
    }
    
    return 0;
}
```

---

### 3. **Mouse Click Detection Inefficiency**
**Severity:** HIGH  
**Location:** `get_mouse_click()` function  
**Problem:**
- Same issues as position detection
- Reads entire buffer array instead of checking one event
- Blocks indefinitely if no input available
- Doesn't distinguish between click and position events properly

**Fix Applied:**
- Switched to `PeekConsoleInput()` for non-blocking checking
- Only reads one event record at a time
- Properly checks for left mouse button press
- Returns early if no click event present

**Code Changes:**
```c
int get_mouse_click(int *mouse_x, int *mouse_y){
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD inRec;
    DWORD numRead;

    // Non-blocking check with PeekConsoleInput
    if(!PeekConsoleInput(hInput, &inRec, 1, &numRead) || numRead == 0){
        return 0;
    }

    if(inRec.EventType == MOUSE_EVENT){
        MOUSE_EVENT_RECORD m = inRec.Event.MouseEvent;

        if(m.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED){
            // Actually read the input to remove it from buffer
            ReadConsoleInput(hInput, &inRec, 1, &numRead);
            *mouse_x = m.dwMousePosition.X;
            *mouse_y = m.dwMousePosition.Y;
            return 1;
        }
    }
    
    return 0;
}
```

---

### 4. **Keyboard Input Conflict with Mouse Input**
**Severity:** HIGH  
**Location:** `check_escape()` and mouse functions  
**Problem:**
- Original code used `_kbhit()` and `_getch()` for keyboard input
- These operate on a different input stream than `ReadConsoleInput()`
- Caused interference and missed input events
- Non-portable API (Windows-specific in wrong way)

**Fix Applied:**
- Rewrote `check_escape()` to use the same `PeekConsoleInput()`/`ReadConsoleInput()` API as mouse functions
- Now checks for `KEY_EVENT` instead of using `_kbhit()`
- Checks for `VK_ESCAPE` virtual key code
- Non-blocking and consistent with mouse input handling

**Code Changes:**
```c
int check_escape(){
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD inRec;
    DWORD numRead;

    // Non-blocking check
    if(!PeekConsoleInput(hInput, &inRec, 1, &numRead) || numRead == 0){
        return 0;
    }

    if(inRec.EventType == KEY_EVENT){
        KEY_EVENT_RECORD k = inRec.Event.KeyEvent;
        
        if(k.bKeyDown && k.wVirtualKeyCode == VK_ESCAPE){
            // Actually read it to remove from buffer
            ReadConsoleInput(hInput, &inRec, 1, &numRead);
            return 1;
        }
    }
    
    return 0;
}
```

---

### 5. **Inaccurate Game Timing**
**Severity:** MEDIUM  
**Location:** `get_track_data()` and `handle_game()` functions  
**Problem:**
- Uses `clock()` for game timing, which:
  - Returns CPU ticks, not wall-clock time
  - Can be inaccurate on Windows
  - Affected by system load and other processes
  - Not suitable for real-time game timing

**Fix Applied:**
- Changed timing to use `GetTickCount()` which returns milliseconds since system startup
- Much more accurate for real-time games
- Consistent across Windows platforms
- Better precision for note timing detection

**Code Changes:**
- Changed variable type: `float track_start_time` → `DWORD track_start_time`
- Updated initialization: `track_start_time = clock()` → `track_start_time = GetTickCount()`
- Updated calculation: `(float)(clock() - track_start_time) / CLOCKS_PER_SEC` → `(float)(GetTickCount() - track_start_time) / 1000.0f`

---

### 6. **Mouse Cursor Trail on Screen**
**Severity:** MEDIUM  
**Location:** `handle_game()` function  
**Problem:**
- Cursor drawn every frame with "O" character
- Previous position never cleared
- Creates visual trail on screen
- Makes game UI confusing

**Fix Applied:**
- Added tracking variables for last mouse position: `last_mouse_x`, `last_mouse_y`
- Clear previous cursor position before drawing new one
- Initialize these values in `get_track_data()`

**Code Changes:**
```c
// Added global tracking variables
int last_mouse_x = -1, last_mouse_y = -1;

// In handle_game():
// Clear previous mouse position
if(last_mouse_x >= 0 && last_mouse_y >= 0){
    gotoxy(last_mouse_x, last_mouse_y);
    printf(" ");
}

// Get and display current mouse position
if(get_mouse_position(&mouse_x, &mouse_y)){
    gotoxy(mouse_x, mouse_y);
    set_color(14);
    printf("O");
    set_color(7);
    last_mouse_x = mouse_x;
    last_mouse_y = mouse_y;
}
```

---

### 7. **Missing Input Validation for Array Index**
**Severity:** HIGH  
**Location:** `handle_game()` function - score saving section  
**Problem:**
- Code saves scores directly to `leaderboard[current_player_index]` without validating the index
- If `current_player_index` is invalid or corrupted, causes buffer overflow/crash
- No bounds checking against `last_lb`

**Fix Applied:**
- Added validation check before score saving:
```c
if(current_player_index >= 0 && current_player_index <= last_lb){
    // score saving code...
}
```
- Prevents undefined behavior from invalid indices
- Prevents potential security issues

---

## Testing Recommendations

1. **Mouse Input Test:**
   - Enable console mouse mode by compiling and running the fixed version
   - Click on the screen during gameplay
   - Verify clicks register correctly on notes

2. **Mouse Position Test:**
   - Move mouse over the game area
   - Verify cursor position updates without lag
   - Check for no visual trails

3. **Timing Test:**
   - Play a track and verify note timing is accurate
   - Time the track duration manually
   - Verify scores match timing accuracy

4. **Escape Key Test:**
   - Press ESC during gameplay
   - Verify track ends immediately (no delay)

5. **Edge Cases:**
   - Test with many players (near MAX_PLAYERS limit)
   - Test rapid clicking
   - Test rapid mouse movement

---

## Compilation Notes

The code now uses:
- `GetTickCount()` - Windows API for accurate timing
- `PeekConsoleInput()` / `ReadConsoleInput()` - Windows API for console input
- `SetConsoleMode()` - Windows API for console configuration with `ENABLE_MOUSE_INPUT`
- Standard C library functions

**Compiler:** GCC with `-lm` flag for math library (fabs function)

**Compile Command:**
```bash
gcc -Wall -Wextra GP1.c -o GP1 -lm
```

---

## Summary of Changes

| Bug | Type | Severity | Fixed |
|-----|------|----------|-------|
| Mouse input mode not enabled | Configuration | CRITICAL | ✓ |
| Mouse position detection error | Logic | HIGH | ✓ |
| Mouse click detection error | Logic | HIGH | ✓ |
| Keyboard/mouse input conflict | Architecture | HIGH | ✓ |
| Inaccurate game timing | Algorithm | MEDIUM | ✓ |
| Mouse cursor trail | Visual | MEDIUM | ✓ |
| Missing array bounds check | Safety | HIGH | ✓ |

All identified bugs have been fixed and the code is ready for testing.
