#include "layout_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "layout_mgr";

/* Each entry maps an ASCII char to (scancode, modifier).
 * We only map printable ASCII 0x20-0x7E. */
typedef struct {
    uint8_t scancode;
    uint8_t modifier;
} keymap_entry_t;

/* 95 printable ASCII chars (0x20 ' ' through 0x7E '~') */
#define KEYMAP_SIZE 95
#define KEYMAP_OFFSET 0x20

/* Common HID scancodes */
#define KEY_A          0x04
#define KEY_B          0x05
#define KEY_C          0x06
#define KEY_D          0x07
#define KEY_E          0x08
#define KEY_F          0x09
#define KEY_G          0x0A
#define KEY_H          0x0B
#define KEY_I          0x0C
#define KEY_J          0x0D
#define KEY_K          0x0E
#define KEY_L          0x0F
#define KEY_M          0x10
#define KEY_N          0x11
#define KEY_O          0x12
#define KEY_P          0x13
#define KEY_Q          0x14
#define KEY_R          0x15
#define KEY_S          0x16
#define KEY_T          0x17
#define KEY_U          0x18
#define KEY_V          0x19
#define KEY_W          0x1A
#define KEY_X          0x1B
#define KEY_Y          0x1C
#define KEY_Z          0x1D
#define KEY_1          0x1E
#define KEY_2          0x1F
#define KEY_3          0x20
#define KEY_4          0x21
#define KEY_5          0x22
#define KEY_6          0x23
#define KEY_7          0x24
#define KEY_8          0x25
#define KEY_9          0x26
#define KEY_0          0x27
#define KEY_ENTER      0x28
#define KEY_ESC        0x29
#define KEY_BACKSPACE  0x2A
#define KEY_TAB        0x2B
#define KEY_SPACE      0x2C
#define KEY_MINUS      0x2D
#define KEY_EQUAL      0x2E
#define KEY_LBRACKET   0x2F
#define KEY_RBRACKET   0x30
#define KEY_BACKSLASH  0x31
#define KEY_HASH       0x32   /* Non-US # and ~ */
#define KEY_SEMICOLON  0x33
#define KEY_APOSTROPHE 0x34
#define KEY_GRAVE      0x35
#define KEY_COMMA      0x36
#define KEY_DOT        0x37
#define KEY_SLASH      0x38
#define KEY_CAPSLOCK   0x39
#define KEY_102ND      0x64   /* Non-US \ and | (between left shift and Z) */

#define S MOD_LSHIFT
#define A MOD_RALT      /* AltGr */
#define SA (MOD_LSHIFT | MOD_RALT)

/* ── US QWERTY layout ──────────────────────────────────────────── */
static const keymap_entry_t keymap_us[KEYMAP_SIZE] = {
    /* 0x20 ' '  */ {KEY_SPACE, 0},
    /* 0x21 '!'  */ {KEY_1, S},
    /* 0x22 '"'  */ {KEY_APOSTROPHE, S},
    /* 0x23 '#'  */ {KEY_3, S},
    /* 0x24 '$'  */ {KEY_4, S},
    /* 0x25 '%'  */ {KEY_5, S},
    /* 0x26 '&'  */ {KEY_7, S},
    /* 0x27 '\'' */ {KEY_APOSTROPHE, 0},
    /* 0x28 '('  */ {KEY_9, S},
    /* 0x29 ')'  */ {KEY_0, S},
    /* 0x2A '*'  */ {KEY_8, S},
    /* 0x2B '+'  */ {KEY_EQUAL, S},
    /* 0x2C ','  */ {KEY_COMMA, 0},
    /* 0x2D '-'  */ {KEY_MINUS, 0},
    /* 0x2E '.'  */ {KEY_DOT, 0},
    /* 0x2F '/'  */ {KEY_SLASH, 0},
    /* 0x30 '0'  */ {KEY_0, 0},
    /* 0x31 '1'  */ {KEY_1, 0},
    /* 0x32 '2'  */ {KEY_2, 0},
    /* 0x33 '3'  */ {KEY_3, 0},
    /* 0x34 '4'  */ {KEY_4, 0},
    /* 0x35 '5'  */ {KEY_5, 0},
    /* 0x36 '6'  */ {KEY_6, 0},
    /* 0x37 '7'  */ {KEY_7, 0},
    /* 0x38 '8'  */ {KEY_8, 0},
    /* 0x39 '9'  */ {KEY_9, 0},
    /* 0x3A ':'  */ {KEY_SEMICOLON, S},
    /* 0x3B ';'  */ {KEY_SEMICOLON, 0},
    /* 0x3C '<'  */ {KEY_COMMA, S},
    /* 0x3D '='  */ {KEY_EQUAL, 0},
    /* 0x3E '>'  */ {KEY_DOT, S},
    /* 0x3F '?'  */ {KEY_SLASH, S},
    /* 0x40 '@'  */ {KEY_2, S},
    /* 0x41-0x5A: A-Z */
    {KEY_A, S}, {KEY_B, S}, {KEY_C, S}, {KEY_D, S}, {KEY_E, S},
    {KEY_F, S}, {KEY_G, S}, {KEY_H, S}, {KEY_I, S}, {KEY_J, S},
    {KEY_K, S}, {KEY_L, S}, {KEY_M, S}, {KEY_N, S}, {KEY_O, S},
    {KEY_P, S}, {KEY_Q, S}, {KEY_R, S}, {KEY_S, S}, {KEY_T, S},
    {KEY_U, S}, {KEY_V, S}, {KEY_W, S}, {KEY_X, S}, {KEY_Y, S},
    {KEY_Z, S},
    /* 0x5B '['  */ {KEY_LBRACKET, 0},
    /* 0x5C '\\' */ {KEY_BACKSLASH, 0},
    /* 0x5D ']'  */ {KEY_RBRACKET, 0},
    /* 0x5E '^'  */ {KEY_6, S},
    /* 0x5F '_'  */ {KEY_MINUS, S},
    /* 0x60 '`'  */ {KEY_GRAVE, 0},
    /* 0x61-0x7A: a-z */
    {KEY_A, 0}, {KEY_B, 0}, {KEY_C, 0}, {KEY_D, 0}, {KEY_E, 0},
    {KEY_F, 0}, {KEY_G, 0}, {KEY_H, 0}, {KEY_I, 0}, {KEY_J, 0},
    {KEY_K, 0}, {KEY_L, 0}, {KEY_M, 0}, {KEY_N, 0}, {KEY_O, 0},
    {KEY_P, 0}, {KEY_Q, 0}, {KEY_R, 0}, {KEY_S, 0}, {KEY_T, 0},
    {KEY_U, 0}, {KEY_V, 0}, {KEY_W, 0}, {KEY_X, 0}, {KEY_Y, 0},
    {KEY_Z, 0},
    /* 0x7B '{'  */ {KEY_LBRACKET, S},
    /* 0x7C '|'  */ {KEY_BACKSLASH, S},
    /* 0x7D '}'  */ {KEY_RBRACKET, S},
    /* 0x7E '~'  */ {KEY_GRAVE, S},
};

/* ── German QWERTZ layout ─────────────────────────────────────── */
static const keymap_entry_t keymap_de[KEYMAP_SIZE] = {
    /* 0x20 ' '  */ {KEY_SPACE, 0},
    /* 0x21 '!'  */ {KEY_1, S},
    /* 0x22 '"'  */ {KEY_2, S},
    /* 0x23 '#'  */ {KEY_HASH, 0},
    /* 0x24 '$'  */ {KEY_4, S},
    /* 0x25 '%'  */ {KEY_5, S},
    /* 0x26 '&'  */ {KEY_6, S},
    /* 0x27 '\'' */ {KEY_HASH, S},
    /* 0x28 '('  */ {KEY_8, S},
    /* 0x29 ')'  */ {KEY_9, S},
    /* 0x2A '*'  */ {KEY_RBRACKET, S},
    /* 0x2B '+'  */ {KEY_RBRACKET, 0},
    /* 0x2C ','  */ {KEY_COMMA, 0},
    /* 0x2D '-'  */ {KEY_SLASH, 0},
    /* 0x2E '.'  */ {KEY_DOT, 0},
    /* 0x2F '/'  */ {KEY_7, S},
    /* 0x30 '0'  */ {KEY_0, 0},
    /* 0x31 '1'  */ {KEY_1, 0},
    /* 0x32 '2'  */ {KEY_2, 0},
    /* 0x33 '3'  */ {KEY_3, 0},
    /* 0x34 '4'  */ {KEY_4, 0},
    /* 0x35 '5'  */ {KEY_5, 0},
    /* 0x36 '6'  */ {KEY_6, 0},
    /* 0x37 '7'  */ {KEY_7, 0},
    /* 0x38 '8'  */ {KEY_8, 0},
    /* 0x39 '9'  */ {KEY_9, 0},
    /* 0x3A ':'  */ {KEY_DOT, S},
    /* 0x3B ';'  */ {KEY_COMMA, S},
    /* 0x3C '<'  */ {KEY_102ND, 0},
    /* 0x3D '='  */ {KEY_0, S},
    /* 0x3E '>'  */ {KEY_102ND, S},
    /* 0x3F '?'  */ {KEY_MINUS, S},
    /* 0x40 '@'  */ {KEY_Q, A},
    /* A-Z: note Y and Z are swapped in QWERTZ */
    {KEY_A, S}, {KEY_B, S}, {KEY_C, S}, {KEY_D, S}, {KEY_E, S},
    {KEY_F, S}, {KEY_G, S}, {KEY_H, S}, {KEY_I, S}, {KEY_J, S},
    {KEY_K, S}, {KEY_L, S}, {KEY_M, S}, {KEY_N, S}, {KEY_O, S},
    {KEY_P, S}, {KEY_Q, S}, {KEY_R, S}, {KEY_S, S}, {KEY_T, S},
    {KEY_U, S}, {KEY_V, S}, {KEY_W, S}, {KEY_X, S}, {KEY_Z, S},  /* Y→Z key */
    {KEY_Y, S},  /* Z→Y key */
    /* 0x5B '['  */ {KEY_8, A},
    /* 0x5C '\\' */ {KEY_MINUS, A},
    /* 0x5D ']'  */ {KEY_9, A},
    /* 0x5E '^'  */ {KEY_GRAVE, 0},     /* dead key on real DE, but we send it directly */
    /* 0x5F '_'  */ {KEY_SLASH, S},
    /* 0x60 '`'  */ {KEY_EQUAL, S},     /* dead key ` (Shift+acute) */
    /* a-z */
    {KEY_A, 0}, {KEY_B, 0}, {KEY_C, 0}, {KEY_D, 0}, {KEY_E, 0},
    {KEY_F, 0}, {KEY_G, 0}, {KEY_H, 0}, {KEY_I, 0}, {KEY_J, 0},
    {KEY_K, 0}, {KEY_L, 0}, {KEY_M, 0}, {KEY_N, 0}, {KEY_O, 0},
    {KEY_P, 0}, {KEY_Q, 0}, {KEY_R, 0}, {KEY_S, 0}, {KEY_T, 0},
    {KEY_U, 0}, {KEY_V, 0}, {KEY_W, 0}, {KEY_X, 0}, {KEY_Z, 0},  /* y→Z key */
    {KEY_Y, 0},  /* z→Y key */
    /* 0x7B '{'  */ {KEY_7, A},
    /* 0x7C '|'  */ {KEY_102ND, A},
    /* 0x7D '}'  */ {KEY_0, A},
    /* 0x7E '~'  */ {KEY_RBRACKET, A},
};

/* ── Swiss German layout (CH-DE) ──────────────────────────────── */
static const keymap_entry_t keymap_ch_de[KEYMAP_SIZE] = {
    /* 0x20 ' '  */ {KEY_SPACE, 0},
    /* 0x21 '!'  */ {KEY_RBRACKET, S},
    /* 0x22 '"'  */ {KEY_2, S},
    /* 0x23 '#'  */ {KEY_3, A},
    /* 0x24 '$'  */ {KEY_4, S},
    /* 0x25 '%'  */ {KEY_5, S},
    /* 0x26 '&'  */ {KEY_6, S},
    /* 0x27 '\'' */ {KEY_MINUS, 0},
    /* 0x28 '('  */ {KEY_8, S},
    /* 0x29 ')'  */ {KEY_9, S},
    /* 0x2A '*'  */ {KEY_3, S},
    /* 0x2B '+'  */ {KEY_1, S},
    /* 0x2C ','  */ {KEY_COMMA, 0},
    /* 0x2D '-'  */ {KEY_SLASH, 0},
    /* 0x2E '.'  */ {KEY_DOT, 0},
    /* 0x2F '/'  */ {KEY_7, S},
    /* 0x30-0x39: digits */
    {KEY_0, 0}, {KEY_1, 0}, {KEY_2, 0}, {KEY_3, 0}, {KEY_4, 0},
    {KEY_5, 0}, {KEY_6, 0}, {KEY_7, 0}, {KEY_8, 0}, {KEY_9, 0},
    /* 0x3A ':'  */ {KEY_DOT, S},
    /* 0x3B ';'  */ {KEY_COMMA, S},
    /* 0x3C '<'  */ {KEY_102ND, 0},
    /* 0x3D '='  */ {KEY_0, S},
    /* 0x3E '>'  */ {KEY_102ND, S},
    /* 0x3F '?'  */ {KEY_MINUS, S},
    /* 0x40 '@'  */ {KEY_2, A},
    /* A-Z: QWERTZ (Y/Z swapped) */
    {KEY_A, S}, {KEY_B, S}, {KEY_C, S}, {KEY_D, S}, {KEY_E, S},
    {KEY_F, S}, {KEY_G, S}, {KEY_H, S}, {KEY_I, S}, {KEY_J, S},
    {KEY_K, S}, {KEY_L, S}, {KEY_M, S}, {KEY_N, S}, {KEY_O, S},
    {KEY_P, S}, {KEY_Q, S}, {KEY_R, S}, {KEY_S, S}, {KEY_T, S},
    {KEY_U, S}, {KEY_V, S}, {KEY_W, S}, {KEY_X, S}, {KEY_Z, S},
    {KEY_Y, S},
    /* 0x5B '['  */ {KEY_LBRACKET, A},
    /* 0x5C '\\' */ {KEY_102ND, A},
    /* 0x5D ']'  */ {KEY_RBRACKET, A},
    /* 0x5E '^'  */ {KEY_GRAVE, 0},     /* dead key */
    /* 0x5F '_'  */ {KEY_SLASH, S},
    /* 0x60 '`'  */ {KEY_EQUAL, S},     /* dead key */
    /* a-z */
    {KEY_A, 0}, {KEY_B, 0}, {KEY_C, 0}, {KEY_D, 0}, {KEY_E, 0},
    {KEY_F, 0}, {KEY_G, 0}, {KEY_H, 0}, {KEY_I, 0}, {KEY_J, 0},
    {KEY_K, 0}, {KEY_L, 0}, {KEY_M, 0}, {KEY_N, 0}, {KEY_O, 0},
    {KEY_P, 0}, {KEY_Q, 0}, {KEY_R, 0}, {KEY_S, 0}, {KEY_T, 0},
    {KEY_U, 0}, {KEY_V, 0}, {KEY_W, 0}, {KEY_X, 0}, {KEY_Z, 0},
    {KEY_Y, 0},
    /* 0x7B '{'  */ {KEY_APOSTROPHE, A},
    /* 0x7C '|'  */ {KEY_7, A},
    /* 0x7D '}'  */ {KEY_BACKSLASH, A},
    /* 0x7E '~'  */ {KEY_EQUAL, A},
};

/* ── French AZERTY layout ─────────────────────────────────────── */
static const keymap_entry_t keymap_fr[KEYMAP_SIZE] = {
    /* 0x20 ' '  */ {KEY_SPACE, 0},
    /* 0x21 '!'  */ {KEY_SLASH, 0},
    /* 0x22 '"'  */ {KEY_3, 0},
    /* 0x23 '#'  */ {KEY_3, A},
    /* 0x24 '$'  */ {KEY_RBRACKET, 0},
    /* 0x25 '%'  */ {KEY_APOSTROPHE, S},
    /* 0x26 '&'  */ {KEY_1, 0},
    /* 0x27 '\'' */ {KEY_4, 0},
    /* 0x28 '('  */ {KEY_5, 0},
    /* 0x29 ')'  */ {KEY_MINUS, 0},
    /* 0x2A '*'  */ {KEY_BACKSLASH, 0},
    /* 0x2B '+'  */ {KEY_EQUAL, S},
    /* 0x2C ','  */ {KEY_M, 0},
    /* 0x2D '-'  */ {KEY_6, 0},
    /* 0x2E '.'  */ {KEY_COMMA, S},
    /* 0x2F '/'  */ {KEY_DOT, S},
    /* 0x30 '0'  */ {KEY_0, S},
    /* 0x31 '1'  */ {KEY_1, S},
    /* 0x32 '2'  */ {KEY_2, S},
    /* 0x33 '3'  */ {KEY_3, S},
    /* 0x34 '4'  */ {KEY_4, S},
    /* 0x35 '5'  */ {KEY_5, S},
    /* 0x36 '6'  */ {KEY_6, S},
    /* 0x37 '7'  */ {KEY_7, S},
    /* 0x38 '8'  */ {KEY_8, S},
    /* 0x39 '9'  */ {KEY_9, S},
    /* 0x3A ':'  */ {KEY_DOT, 0},
    /* 0x3B ';'  */ {KEY_COMMA, 0},
    /* 0x3C '<'  */ {KEY_102ND, 0},
    /* 0x3D '='  */ {KEY_EQUAL, 0},
    /* 0x3E '>'  */ {KEY_102ND, S},
    /* 0x3F '?'  */ {KEY_M, S},
    /* 0x40 '@'  */ {KEY_0, A},
    /* A-Z: AZERTY remapping */
    {KEY_Q, S},  /* A */ {KEY_B, S}, {KEY_C, S}, {KEY_D, S}, {KEY_E, S},
    {KEY_F, S}, {KEY_G, S}, {KEY_H, S}, {KEY_I, S}, {KEY_J, S},
    {KEY_K, S}, {KEY_L, S}, {KEY_SEMICOLON, S}, /* M */  {KEY_N, S}, {KEY_O, S},
    {KEY_P, S}, {KEY_A, S},  /* Q */  {KEY_R, S}, {KEY_S, S}, {KEY_T, S},
    {KEY_U, S}, {KEY_V, S}, {KEY_Z, S},  /* W */  {KEY_X, S}, {KEY_Y, S},
    {KEY_W, S},  /* Z */
    /* 0x5B '['  */ {KEY_5, A},
    /* 0x5C '\\' */ {KEY_8, A},
    /* 0x5D ']'  */ {KEY_MINUS, A},
    /* 0x5E '^'  */ {KEY_LBRACKET, 0},   /* dead key */
    /* 0x5F '_'  */ {KEY_8, 0},
    /* 0x60 '`'  */ {KEY_7, A},
    /* a-z: AZERTY */
    {KEY_Q, 0},  /* a */ {KEY_B, 0}, {KEY_C, 0}, {KEY_D, 0}, {KEY_E, 0},
    {KEY_F, 0}, {KEY_G, 0}, {KEY_H, 0}, {KEY_I, 0}, {KEY_J, 0},
    {KEY_K, 0}, {KEY_L, 0}, {KEY_SEMICOLON, 0}, /* m */ {KEY_N, 0}, {KEY_O, 0},
    {KEY_P, 0}, {KEY_A, 0},  /* q */ {KEY_R, 0}, {KEY_S, 0}, {KEY_T, 0},
    {KEY_U, 0}, {KEY_V, 0}, {KEY_Z, 0},  /* w */ {KEY_X, 0}, {KEY_Y, 0},
    {KEY_W, 0},  /* z */
    /* 0x7B '{'  */ {KEY_4, A},
    /* 0x7C '|'  */ {KEY_6, A},
    /* 0x7D '}'  */ {KEY_EQUAL, A},
    /* 0x7E '~'  */ {KEY_2, A},
};

/* ── UK QWERTY layout ─────────────────────────────────────────── */
static const keymap_entry_t keymap_uk[KEYMAP_SIZE] = {
    /* 0x20 ' '  */ {KEY_SPACE, 0},
    /* 0x21 '!'  */ {KEY_1, S},
    /* 0x22 '"'  */ {KEY_2, S},
    /* 0x23 '#'  */ {KEY_HASH, 0},
    /* 0x24 '$'  */ {KEY_4, S},
    /* 0x25 '%'  */ {KEY_5, S},
    /* 0x26 '&'  */ {KEY_7, S},
    /* 0x27 '\'' */ {KEY_APOSTROPHE, 0},
    /* 0x28 '('  */ {KEY_9, S},
    /* 0x29 ')'  */ {KEY_0, S},
    /* 0x2A '*'  */ {KEY_8, S},
    /* 0x2B '+'  */ {KEY_EQUAL, S},
    /* 0x2C ','  */ {KEY_COMMA, 0},
    /* 0x2D '-'  */ {KEY_MINUS, 0},
    /* 0x2E '.'  */ {KEY_DOT, 0},
    /* 0x2F '/'  */ {KEY_SLASH, 0},
    /* 0x30-0x39 */
    {KEY_0, 0}, {KEY_1, 0}, {KEY_2, 0}, {KEY_3, 0}, {KEY_4, 0},
    {KEY_5, 0}, {KEY_6, 0}, {KEY_7, 0}, {KEY_8, 0}, {KEY_9, 0},
    /* 0x3A ':'  */ {KEY_SEMICOLON, S},
    /* 0x3B ';'  */ {KEY_SEMICOLON, 0},
    /* 0x3C '<'  */ {KEY_COMMA, S},
    /* 0x3D '='  */ {KEY_EQUAL, 0},
    /* 0x3E '>'  */ {KEY_DOT, S},
    /* 0x3F '?'  */ {KEY_SLASH, S},
    /* 0x40 '@'  */ {KEY_APOSTROPHE, S},
    /* A-Z */
    {KEY_A, S}, {KEY_B, S}, {KEY_C, S}, {KEY_D, S}, {KEY_E, S},
    {KEY_F, S}, {KEY_G, S}, {KEY_H, S}, {KEY_I, S}, {KEY_J, S},
    {KEY_K, S}, {KEY_L, S}, {KEY_M, S}, {KEY_N, S}, {KEY_O, S},
    {KEY_P, S}, {KEY_Q, S}, {KEY_R, S}, {KEY_S, S}, {KEY_T, S},
    {KEY_U, S}, {KEY_V, S}, {KEY_W, S}, {KEY_X, S}, {KEY_Y, S},
    {KEY_Z, S},
    /* 0x5B '['  */ {KEY_LBRACKET, 0},
    /* 0x5C '\\' */ {KEY_102ND, 0},
    /* 0x5D ']'  */ {KEY_RBRACKET, 0},
    /* 0x5E '^'  */ {KEY_6, S},
    /* 0x5F '_'  */ {KEY_MINUS, S},
    /* 0x60 '`'  */ {KEY_GRAVE, 0},
    /* a-z */
    {KEY_A, 0}, {KEY_B, 0}, {KEY_C, 0}, {KEY_D, 0}, {KEY_E, 0},
    {KEY_F, 0}, {KEY_G, 0}, {KEY_H, 0}, {KEY_I, 0}, {KEY_J, 0},
    {KEY_K, 0}, {KEY_L, 0}, {KEY_M, 0}, {KEY_N, 0}, {KEY_O, 0},
    {KEY_P, 0}, {KEY_Q, 0}, {KEY_R, 0}, {KEY_S, 0}, {KEY_T, 0},
    {KEY_U, 0}, {KEY_V, 0}, {KEY_W, 0}, {KEY_X, 0}, {KEY_Y, 0},
    {KEY_Z, 0},
    /* 0x7B '{'  */ {KEY_LBRACKET, S},
    /* 0x7C '|'  */ {KEY_102ND, S},
    /* 0x7D '}'  */ {KEY_RBRACKET, S},
    /* 0x7E '~'  */ {KEY_HASH, S},
};

/* ── Spanish QWERTY layout ────────────────────────────────────── */
static const keymap_entry_t keymap_es[KEYMAP_SIZE] = {
    /* 0x20 ' '  */ {KEY_SPACE, 0},
    /* 0x21 '!'  */ {KEY_1, S},
    /* 0x22 '"'  */ {KEY_2, S},
    /* 0x23 '#'  */ {KEY_3, A},
    /* 0x24 '$'  */ {KEY_4, S},
    /* 0x25 '%'  */ {KEY_5, S},
    /* 0x26 '&'  */ {KEY_6, S},
    /* 0x27 '\'' */ {KEY_MINUS, 0},
    /* 0x28 '('  */ {KEY_8, S},
    /* 0x29 ')'  */ {KEY_9, S},
    /* 0x2A '*'  */ {KEY_RBRACKET, S},
    /* 0x2B '+'  */ {KEY_RBRACKET, 0},
    /* 0x2C ','  */ {KEY_COMMA, 0},
    /* 0x2D '-'  */ {KEY_SLASH, 0},
    /* 0x2E '.'  */ {KEY_DOT, 0},
    /* 0x2F '/'  */ {KEY_7, S},
    /* 0x30-0x39 */
    {KEY_0, 0}, {KEY_1, 0}, {KEY_2, 0}, {KEY_3, 0}, {KEY_4, 0},
    {KEY_5, 0}, {KEY_6, 0}, {KEY_7, 0}, {KEY_8, 0}, {KEY_9, 0},
    /* 0x3A ':'  */ {KEY_DOT, S},
    /* 0x3B ';'  */ {KEY_COMMA, S},
    /* 0x3C '<'  */ {KEY_102ND, 0},
    /* 0x3D '='  */ {KEY_0, S},
    /* 0x3E '>'  */ {KEY_102ND, S},
    /* 0x3F '?'  */ {KEY_MINUS, S},
    /* 0x40 '@'  */ {KEY_2, A},
    /* A-Z */
    {KEY_A, S}, {KEY_B, S}, {KEY_C, S}, {KEY_D, S}, {KEY_E, S},
    {KEY_F, S}, {KEY_G, S}, {KEY_H, S}, {KEY_I, S}, {KEY_J, S},
    {KEY_K, S}, {KEY_L, S}, {KEY_M, S}, {KEY_N, S}, {KEY_O, S},
    {KEY_P, S}, {KEY_Q, S}, {KEY_R, S}, {KEY_S, S}, {KEY_T, S},
    {KEY_U, S}, {KEY_V, S}, {KEY_W, S}, {KEY_X, S}, {KEY_Y, S},
    {KEY_Z, S},
    /* 0x5B '['  */ {KEY_LBRACKET, A},
    /* 0x5C '\\' */ {KEY_GRAVE, A},
    /* 0x5D ']'  */ {KEY_RBRACKET, A},
    /* 0x5E '^'  */ {KEY_LBRACKET, S},   /* dead key */
    /* 0x5F '_'  */ {KEY_SLASH, S},
    /* 0x60 '`'  */ {KEY_LBRACKET, 0},   /* dead key */
    /* a-z */
    {KEY_A, 0}, {KEY_B, 0}, {KEY_C, 0}, {KEY_D, 0}, {KEY_E, 0},
    {KEY_F, 0}, {KEY_G, 0}, {KEY_H, 0}, {KEY_I, 0}, {KEY_J, 0},
    {KEY_K, 0}, {KEY_L, 0}, {KEY_M, 0}, {KEY_N, 0}, {KEY_O, 0},
    {KEY_P, 0}, {KEY_Q, 0}, {KEY_R, 0}, {KEY_S, 0}, {KEY_T, 0},
    {KEY_U, 0}, {KEY_V, 0}, {KEY_W, 0}, {KEY_X, 0}, {KEY_Y, 0},
    {KEY_Z, 0},
    /* 0x7B '{'  */ {KEY_APOSTROPHE, A},
    /* 0x7C '|'  */ {KEY_1, A},
    /* 0x7D '}'  */ {KEY_BACKSLASH, A},
    /* 0x7E '~'  */ {KEY_4, A},
};

/* ── Italian QWERTY layout ────────────────────────────────────── */
static const keymap_entry_t keymap_it[KEYMAP_SIZE] = {
    /* 0x20 ' '  */ {KEY_SPACE, 0},
    /* 0x21 '!'  */ {KEY_1, S},
    /* 0x22 '"'  */ {KEY_2, S},
    /* 0x23 '#'  */ {KEY_APOSTROPHE, A},
    /* 0x24 '$'  */ {KEY_4, S},
    /* 0x25 '%'  */ {KEY_5, S},
    /* 0x26 '&'  */ {KEY_6, S},
    /* 0x27 '\'' */ {KEY_MINUS, 0},
    /* 0x28 '('  */ {KEY_8, S},
    /* 0x29 ')'  */ {KEY_9, S},
    /* 0x2A '*'  */ {KEY_RBRACKET, S},
    /* 0x2B '+'  */ {KEY_RBRACKET, 0},
    /* 0x2C ','  */ {KEY_COMMA, 0},
    /* 0x2D '-'  */ {KEY_SLASH, 0},
    /* 0x2E '.'  */ {KEY_DOT, 0},
    /* 0x2F '/'  */ {KEY_7, S},
    /* 0x30-0x39 */
    {KEY_0, 0}, {KEY_1, 0}, {KEY_2, 0}, {KEY_3, 0}, {KEY_4, 0},
    {KEY_5, 0}, {KEY_6, 0}, {KEY_7, 0}, {KEY_8, 0}, {KEY_9, 0},
    /* 0x3A ':'  */ {KEY_DOT, S},
    /* 0x3B ';'  */ {KEY_COMMA, S},
    /* 0x3C '<'  */ {KEY_102ND, 0},
    /* 0x3D '='  */ {KEY_0, S},
    /* 0x3E '>'  */ {KEY_102ND, S},
    /* 0x3F '?'  */ {KEY_MINUS, S},
    /* 0x40 '@'  */ {KEY_SEMICOLON, A},
    /* A-Z */
    {KEY_A, S}, {KEY_B, S}, {KEY_C, S}, {KEY_D, S}, {KEY_E, S},
    {KEY_F, S}, {KEY_G, S}, {KEY_H, S}, {KEY_I, S}, {KEY_J, S},
    {KEY_K, S}, {KEY_L, S}, {KEY_M, S}, {KEY_N, S}, {KEY_O, S},
    {KEY_P, S}, {KEY_Q, S}, {KEY_R, S}, {KEY_S, S}, {KEY_T, S},
    {KEY_U, S}, {KEY_V, S}, {KEY_W, S}, {KEY_X, S}, {KEY_Y, S},
    {KEY_Z, S},
    /* 0x5B '['  */ {KEY_LBRACKET, A},
    /* 0x5C '\\' */ {KEY_GRAVE, 0},
    /* 0x5D ']'  */ {KEY_RBRACKET, A},
    /* 0x5E '^'  */ {KEY_EQUAL, S},
    /* 0x5F '_'  */ {KEY_SLASH, S},
    /* 0x60 '`'  */ {0, 0},   /* not directly available on IT layout */
    /* a-z */
    {KEY_A, 0}, {KEY_B, 0}, {KEY_C, 0}, {KEY_D, 0}, {KEY_E, 0},
    {KEY_F, 0}, {KEY_G, 0}, {KEY_H, 0}, {KEY_I, 0}, {KEY_J, 0},
    {KEY_K, 0}, {KEY_L, 0}, {KEY_M, 0}, {KEY_N, 0}, {KEY_O, 0},
    {KEY_P, 0}, {KEY_Q, 0}, {KEY_R, 0}, {KEY_S, 0}, {KEY_T, 0},
    {KEY_U, 0}, {KEY_V, 0}, {KEY_W, 0}, {KEY_X, 0}, {KEY_Y, 0},
    {KEY_Z, 0},
    /* 0x7B '{'  */ {KEY_LBRACKET, S | A},
    /* 0x7C '|'  */ {KEY_GRAVE, S},
    /* 0x7D '}'  */ {KEY_RBRACKET, S | A},
    /* 0x7E '~'  */ {KEY_SEMICOLON, S | A},
};

/* ── Active layout state ───────────────────────────────────────── */

static const keymap_entry_t *s_active_keymap = keymap_us;
static char s_active_code[8] = "US";

typedef struct {
    const char *code;
    const keymap_entry_t *keymap;
} layout_entry_t;

static const layout_entry_t layouts[] = {
    { "US",    keymap_us },
    { "DE",    keymap_de },
    { "CH-DE", keymap_ch_de },
    { "FR",    keymap_fr },
    { "UK",    keymap_uk },
    { "ES",    keymap_es },
    { "IT",    keymap_it },
};

#define NUM_LAYOUTS (sizeof(layouts) / sizeof(layouts[0]))

/* ── Public API ────────────────────────────────────────────────── */

esp_err_t layout_manager_init(const char *layout_code)
{
    if (!layout_code) layout_code = "US";

    for (int i = 0; i < NUM_LAYOUTS; i++) {
        if (strcmp(layouts[i].code, layout_code) == 0) {
            s_active_keymap = layouts[i].keymap;
            strncpy(s_active_code, layout_code, sizeof(s_active_code) - 1);
            ESP_LOGI(TAG, "Layout active: %s", s_active_code);
            return ESP_OK;
        }
    }

    ESP_LOGW(TAG, "Unknown layout '%s', falling back to US", layout_code);
    s_active_keymap = keymap_us;
    strncpy(s_active_code, "US", sizeof(s_active_code) - 1);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t layout_manager_char_to_scancode(char c, uint8_t *scancode, uint8_t *modifier)
{
    /* Handle special chars */
    if (c == '\n' || c == '\r') {
        *scancode = KEY_ENTER;
        *modifier = 0;
        return ESP_OK;
    }
    if (c == '\t') {
        *scancode = KEY_TAB;
        *modifier = 0;
        return ESP_OK;
    }
    if (c == '\b') {
        *scancode = KEY_BACKSPACE;
        *modifier = 0;
        return ESP_OK;
    }

    /* Printable ASCII range */
    if (c < 0x20 || c > 0x7E) {
        return ESP_ERR_NOT_FOUND;
    }

    int idx = c - KEYMAP_OFFSET;
    const keymap_entry_t *entry = &s_active_keymap[idx];

    if (entry->scancode == 0 && entry->modifier == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    *scancode = entry->scancode;
    *modifier = entry->modifier;
    return ESP_OK;
}

const char *layout_manager_get_active(void)
{
    return s_active_code;
}
