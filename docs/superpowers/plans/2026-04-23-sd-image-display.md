# General File Browser and BMP/JPG Preview Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a general-purpose SD card file browser in the current STM32F407 project that can enter arbitrary directories, show a `[..]` parent item, open regular files, and preview both `.bmp` and `.jpg/.jpeg` images on the LCD. Large images should be scaled down proportionally and centered in the LCD preview area.

**Architecture:** Keep file-system browsing state inside the existing `Library/ui` common layer so the list page, key dispatch layer, and view page all share the same current path. Use a single current-directory model plus virtual `[..]` entry generation instead of a special `Picture` mode, so directory navigation stays generic while image preview is handled by a dedicated picture module that supports `.bmp` and `.jpg/.jpeg`. Add a shared preview-layout layer that computes aspect-ratio-preserving fit and center placement for both BMP and JPG/JPEG.

**Tech Stack:** STM32F407, SDIO, FATFS, existing `Library/ui`, existing LCD driver in `Library/tftlcd`, existing picture display module in `Library/Picture`, newly integrated lightweight software JPEG decoder, CMake-based embedded build.

---

## File Structure

Files to inspect before implementation:

- Inspect: `Library/ui/ui_common.h`
- Inspect: `Library/ui/ui_common.c`
- Inspect: `Library/ui/ui_list.c`
- Inspect: `Library/ui/ui_view.c`
- Inspect: `Library/ui/ui_view.h`
- Inspect: `Library/ui/ui_app.c`
- Inspect: `Library/ui/ui_app.h`
- Inspect: `Library/Picture/picture_display.h`
- Inspect: `Library/Picture/picture_display.c`
- Inspect: `Library/Picture/README.md`

Files to modify for browser and dual-format image support:

- Modify: `Library/ui/ui_common.h`
- Modify: `Library/ui/ui_common.c`
- Modify: `Library/ui/ui_list.c`
- Modify: `Library/ui/ui_view.c`
- Modify: `Library/ui/ui_app.c`
- Modify: `Library/Picture/picture_display.h`
- Modify: `Library/Picture/picture_display.c`
- Modify: `Library/Picture/README.md`
- Modify: `CMakeLists.txt`
- Modify: `docs/superpowers/plans/2026-04-23-sd-image-display.md`

Planned new source files for JPEG support:

- Create: `Library/Picture/jpeg_decoder.h`
- Create: `Library/Picture/jpeg_decoder.c`

---

## Interaction Contract

Target interaction after implementation:

### Main Page

Main-page key behavior must remain unchanged except that `KEY0` still enters the browser:

1. `KEY0` enters the file browser rooted at `0:/`.
2. `KEY1` keeps the existing main-page color-switch behavior.
3. `KEY2` keeps the existing main-page test-file creation behavior.
4. `KEY_UP` keeps the existing main-page font reload/import behavior.

### File Browser List Page

1. The browser lists both directories and regular files.
2. In any non-root directory, the first visible item is a virtual parent item named `[..]`.
3. `KEY1` and `KEY_UP` move selection down and up.
4. `KEY2` acts on the selected item:
   - `[..]` -> go to parent directory
   - directory -> enter directory
   - regular file -> open file
5. `KEY0` always returns to the main page.

### View Page

1. `.bmp` files are previewed as images.
2. `.jpg` and `.jpeg` files are also previewed as images.
3. Image preview should preserve the original aspect ratio, scale large images down to fit the preview area, and center the result with blank margins if needed.
4. Small images should not be enlarged in the first version; they should be displayed at original size and centered in the preview area.
5. Other files use the current text/hex preview logic.
6. `KEY0` always closes the view page and returns to the current directory list.
7. Existing `KEY2` close behavior may be retained only if it does not conflict with the new browser flow.

---

### Task 1: Refactor Browser State in `ui_common`

**Files:**
- Modify: `Library/ui/ui_common.h`
- Modify: `Library/ui/ui_common.c`

- [ ] **Step 1: Keep the general current-directory model as the single browser state**

Required state:
- current directory path string, for example `0:/`, `0:/Picture`, `0:/Picture/Sub`
- root directory constant `0:/`
- list entry type, distinguishing:
  - parent item
  - directory
  - regular file

Expected result:
- no code path assumes `0:/picture` is a special or preferred browsing directory
- the browser always starts from `0:/`

- [ ] **Step 2: Keep `FileEntry_t` capable of rendering and acting on directories and the virtual parent item**

Required structure direction:

```c
typedef enum
{
    UI_ENTRY_PARENT = 0,
    UI_ENTRY_DIR,
    UI_ENTRY_FILE
} UiEntryType_t;

typedef struct
{
    char name[64];
    char ext[16];
    uint32_t size;
    uint8_t is_dir;
    UiEntryType_t type;
} FileEntry_t;
```

Rules:
- `[..]` is not read from FATFS
- `[..]` must be generated in software when the current path is not the root directory
- regular directories from FATFS must be marked as `UI_ENTRY_DIR`
- regular files from FATFS must be marked as `UI_ENTRY_FILE`

- [ ] **Step 3: Keep path helper functions for directory enter, parent navigation, and file path construction**

Required helper responsibilities:
- detect whether the current directory is the root directory
- build full file path from `current_dir + filename`
- build child directory path from `current_dir + dirname`
- compute parent directory path for `[..]`

Planned helper surface in `ui_common.h`:

```c
const char *UI_GetCurrentDir(void);
uint8_t UI_IsRootDir(void);
uint8_t UI_LoadDirectory(const char *path);
UiOpenResult_t UI_OpenSelectedEntry(void);
uint8_t UI_BuildSelectedFilePath(uint16_t index, char *out, uint16_t out_cap);
```

- [ ] **Step 4: Keep directory loading behavior generic and path-driven**

Rules:
- `UI_LoadRootFileList()` remains only as a compatibility entry that redirects to `UI_LoadDirectory("0:/")`
- non-root directories insert `[..]` at entry `0`
- root directory does not insert `[..]`
- after loading any directory, `g_file_entries[]`, `g_file_count`, `g_selected_index`, and `g_list_top_index` must remain valid

### Task 2: Keep the List Page as a General File Browser

**Files:**
- Modify: `Library/ui/ui_list.c`

- [ ] **Step 1: Keep row rendering visually distinct for parent item, directory, and regular file**

Rendering rules:
- parent item displays as `[..]`
- directory item displays with a clear directory marker, for example `[DIR] Picture`
- regular file keeps filename, extension, and size display

- [ ] **Step 2: Keep the current directory path visible in the list header area**

Display requirement:
- the user must always know which directory is currently being browsed
- the header should show the current path, for example `Dir: 0:/Picture`

- [ ] **Step 3: Keep the top layout free of overlap**

Rules:
- title area, current-path area, and first visible file row must not overlap
- list clearing region and first-row origin must stay consistent with the chosen header spacing

### Task 3: Route `KEY2` as Enter Directory or Open File

**Files:**
- Modify: `Library/ui/ui_app.c`
- Modify: `Library/ui/ui_common.c`
- Modify: `Library/ui/ui_common.h`

- [ ] **Step 1: Keep `KEY1` and `KEY_UP` as the only list movement keys**

Behavior to preserve:
- `KEY1` moves selection down in the list page
- `KEY_UP` moves selection up in the list page

- [ ] **Step 2: Keep list-page `KEY2` as the unified “open selected entry” action**

Required dispatch behavior:
- if selected entry is `UI_ENTRY_PARENT`, go to parent directory and redraw the list page
- if selected entry is `UI_ENTRY_DIR`, enter that directory and redraw the list page
- if selected entry is `UI_ENTRY_FILE`, open the view page and show the file

- [ ] **Step 3: Keep list-page `KEY0` as “return to main page” only**

Rules:
- do not overload `KEY0` for parent navigation
- parent navigation is handled only by selecting `[..]` and pressing `KEY2`

### Task 4: Keep View-Page Return Behavior Consistent

**Files:**
- Modify: `Library/ui/ui_view.c`
- Modify: `Library/ui/ui_app.c`

- [ ] **Step 1: Keep the view page opening files from the current directory instead of hardcoded paths**

Required path rule:
- `ui_view` must always use the browser’s current directory plus the selected file name
- opening a file inside `0:/Picture/Sub` must not jump back to `0:/`

- [ ] **Step 2: Keep `.bmp` as a preview branch inside the view page**

Rules:
- `.bmp` -> call the BMP display path
- other non-image files -> keep current text/UTF-8/hex preview flow

- [ ] **Step 3: Make `KEY0` in the view page always return to the current directory list**

Expected result:
- open a file from any directory
- press `KEY0`
- return to the same directory list, not the main page and not the root directory

### Task 5: Add JPEG Runtime Decode and Display

**Files:**
- Create: `Library/Picture/jpeg_decoder.h`
- Create: `Library/Picture/jpeg_decoder.c`
- Modify: `Library/Picture/picture_display.h`
- Modify: `Library/Picture/picture_display.c`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Integrate a lightweight software JPEG decoder suitable for STM32F407**

Requirements:
- software decoder, no dependency on STM32 hardware JPEG block
- source code can be built directly in the current embedded project
- decoder can read JPEG bitstream from SD-card-backed file input

First-version scope:
- baseline JPEG only
- no progressive JPEG guarantee in first version
- no PNG/GIF support in this task

- [x] **Step 2: Define a JPEG decode bridge that outputs pixels in a form the LCD path can consume**

Responsibilities:
- accept FATFS-backed file input
- decode image dimensions
- provide pixel blocks, rows, or rectangles to the picture display layer
- convert decoded color output to `RGB565` before LCD write when needed

- [x] **Step 3: Extend the picture display API to support both BMP and JPG**

Planned direction:

```c
typedef enum
{
    PICTURE_OK = 0,
    PICTURE_ERR_OPEN,
    PICTURE_ERR_FORMAT,
    PICTURE_ERR_READ,
    PICTURE_ERR_UNSUPPORTED,
    PICTURE_ERR_RANGE
} PictureResult;

PictureResult Picture_Show(const char *path, uint16_t x, uint16_t y);
PictureResult Picture_ShowBMP(const char *path, uint16_t x, uint16_t y);
PictureResult Picture_ShowJPG(const char *path, uint16_t x, uint16_t y);
```

Rules:
- keep `Picture_ShowBMP()` for existing compatibility
- add `Picture_ShowJPG()` for direct JPEG support
- add a unified `Picture_Show()` entry if integration becomes cleaner

- [x] **Step 4: Implement first-version JPEG display behavior**

Rules:
- support `.jpg` and `.jpeg`
- decode from SD card and display to LCD
- first version may crop like the current BMP path if scaling is not yet added
- keep explicit error returns for unsupported JPEG variants or decode failures

- [x] **Step 5: Add the new JPEG sources to the build system**

Update:
- `CMakeLists.txt`

Verification target:
- the project compiles with the new JPEG decoder sources linked into the same target as the current picture display module

### Task 6: Verify Generic Directory Navigation and BMP/JPG Preview

**Files:**
- Use: SD card content under `0:/`
- Use: at least one child directory named `Picture`
- Use: at least one BMP file inside `Picture`
- Use: at least one JPG or JPEG file inside `Picture`
- Use: optionally one text file inside root and one text file inside a subdirectory

- [ ] **Step 1: Verify root-directory list behavior**

Manual verification:
1. Press `KEY0` on main page
2. Confirm the list starts from `0:/`
3. Confirm root directories and root files are both visible
4. Confirm root list does not show `[..]`

Expected result:
- path label shows `Dir: 0:/`
- selection moves with `KEY1` and `KEY_UP`

- [ ] **Step 2: Verify entering and leaving directories**

Manual verification:
1. Select `Picture`
2. Press `KEY2`
3. Confirm path changes to `0:/Picture` or the actual FATFS directory name
4. Confirm first item is `[..]`
5. Select `[..]`
6. Press `KEY2`

Expected result:
- browser enters `Picture`
- browser returns to root through `[..]`

- [ ] **Step 3: Verify BMP preview from inside a directory**

Manual verification:
1. Enter `Picture`
2. Select a `.bmp`
3. Press `KEY2`
4. Confirm image displays on LCD
5. Press `KEY0`

Expected result:
- BMP preview opens correctly
- `KEY0` closes preview and returns to `Picture` directory list

- [ ] **Step 4: Verify JPG preview from inside a directory**

Manual verification:
1. Enter `Picture`
2. Select a `.jpg` or `.jpeg`
3. Press `KEY2`
4. Confirm image displays on LCD
5. Press `KEY0`

Expected result:
- JPEG preview opens correctly
- colors are reasonable
- `KEY0` closes preview and returns to `Picture` directory list

### Task 7: Add Proportional Image Scaling and Centered Preview

**Files:**
- Modify: `Library/Picture/picture_display.h`
- Modify: `Library/Picture/picture_display.c`
- Modify: `Library/Picture/jpeg_decoder.h`
- Modify: `Library/Picture/jpeg_decoder.c`
- Modify: `Library/ui/ui_view.c`
- Modify: `Library/Picture/README.md`
- Modify: `docs/superpowers/plans/2026-04-23-sd-image-display.md`

- [x] **Step 1: Define a shared preview-area layout model**

Requirements:
- use the current image preview region in the view page as the destination area
- compute destination width and height that fit within the preview area
- preserve the original image aspect ratio
- do not enlarge images that are already smaller than the preview area
- compute centered destination coordinates so blank margins are balanced

- [x] **Step 2: Apply proportional scaling to BMP preview**

Requirements:
- keep BMP file parsing and FATFS streaming behavior
- add nearest-neighbor downscaling in the BMP rendering path
- avoid full-frame image buffering
- allow large BMP images to be reduced to fit the preview area
- keep small BMP images at original size and center them

- [x] **Step 3: Apply proportional scaling to JPG/JPEG preview**

Requirements:
- keep TJpgDec as the JPEG runtime decoder
- use TJpgDec scale levels (`1/1`, `1/2`, `1/4`, `1/8`) to reduce decode cost when possible
- choose the largest TJpgDec output that still fits or comes closest to the preview area
- center the resulting image in the preview area
- first version may accept coarse scaling steps for JPEG rather than arbitrary resampling

- [x] **Step 4: Clear the preview background before drawing the scaled image**

Requirements:
- image preview should not leave stale pixels from a previously displayed image
- blank margins around the centered image should appear clean and intentional

- [ ] **Step 5: Verify scaling behavior manually**

Manual verification:
1. test a BMP larger than the preview area
2. test a JPG larger than the preview area
3. confirm both images are scaled down without distortion
4. confirm both images are centered
5. test a small image and confirm it is not enlarged

Expected result:
- large images fit fully within the preview area
- aspect ratio remains correct
- small images remain sharp at original size
- margins are white and stable

Status note:
- BMP proportional scaling has been implemented and verified on device
- JPG/JPEG centered proportional preview has been implemented
- runtime JPEG failure cause is now surfaced on the preview line, including `JPG UNSUP`, `JPG NOMEM`, and `JPG FORMAT`

- [ ] **Step 5: Verify regular file preview from multiple directories**

Manual verification:
1. Open a text file in root
2. Press `KEY0`
3. Enter a subdirectory
4. Open another text file there
5. Press `KEY0`

Expected result:
- each file returns to the directory it came from

### Task 8: Update Documentation to Match the Browser Model, Dual-Format Support, and Scaling Behavior

**Files:**
- Modify: `Library/Picture/README.md`
- Modify: `docs/superpowers/plans/2026-04-23-sd-image-display.md`

- [x] **Step 1: Remove outdated single-format wording**

Replace it with:
- browser starts from `0:/`
- image preview is triggered by file type, not by a special browsing mode
- current runtime image formats are `.bmp` and `.jpg/.jpeg`
- large images are scaled down proportionally and centered in the preview area

- [x] **Step 2: Document the new key behavior**

Document:
- `KEY0` on main page -> browser
- `KEY1 / KEY_UP` -> selection movement
- `KEY2` -> enter directory / parent / open file
- `KEY0` on view page -> back to list

- [x] **Step 3: Document current scope limits**

Keep explicit limitations:
- `.bmp` and `.jpg/.jpeg` are the only runtime image preview formats in the current version
- no image upscaling in the first version
- no PNG runtime decode
- no GIF runtime decode
- no thumbnail cache

## Self-Review

Spec coverage:
- covers generic SD-card directory browsing from root
- covers virtual `[..]` parent entry behavior
- covers unified `KEY2` action for parent, directory, and file
- covers `KEY0` behavior separation between list page and view page
- covers BMP/JPG preview as file-type branches rather than a special picture mode
- covers aspect-ratio-preserving downscaling and centered preview placement

Placeholder scan:
- no `TODO`
- no `TBD`
- no deferred “implement later” instructions inside executable tasks

Type consistency:
- `FileEntry_t` remains the common list-entry structure
- current-directory helpers remain in `ui_common`
- `ui_view` consumes full file paths built from the current directory state
- picture display APIs distinguish BMP and JPG explicitly

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-04-23-sd-image-display.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
