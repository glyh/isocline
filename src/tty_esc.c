/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <locale.h>

#include "tty.h"

/*-------------------------------------------------------------
Decoding escape sequences to key codes.
This is a bit tricky as there is no clear standard; see:
- <http://www.leonerd.org.uk/hacks/fixterms/>.
- <https://en.wikipedia.org/wiki/ANSI_escape_code#CSI_(Control_Sequence_Introducer)_sequences>
- <https://www.xfree86.org/current/ctlseqs.html>
- <https://www.ecma-international.org/wp-content/uploads/ECMA-48_5th_edition_june_1991.pdf>

Generally, for our purposes we accept a subset of escape sequence as:

  escseq ::= ESC 
          |  ESC char
          |  ESC start special? (number (';' modifiers)?)? final

where:
  char         ::= [\x00-\xFF]               # any character
  special      ::= [:<=>?]             
  number       ::= [0-9+]
  modifiers    ::= [1-9]      
  intermediate ::= [\x20-\x2F]               # !"#$%&'()*+,-./
  final        ::= [\x40-\x7F]               # @A–Z[\]^_`a–z{|}~
  ESC          ::= '\x1B'
  CSI          ::= ESC '['
  SS3          ::= ESC 'O'

In ECMA48 `special? (number (';' modifiers)?)?` is the more liberal `[\x30-\x3F]*` 
but that seems never used for key codes. If the number (vtcode or unicode) or the 
modifiers are not given, we assume these are '1'. 
We then accept the following standard sequences to encode keys:

  key ::= ESC                                              # lone ESC
       |  ESC char                                         # Alt+char
       |  ESC '[' special? vtcode  ';' modifiers '~'       # vt100 codes
       |  ESC '[' special? '1'     ';' modifiers [A-Z]     # xterm codes
       |  ESC 'O' special? '1'     ';' modifiers [A-Za-z]  # SS3 codes
       |  ESC '[' special? unicode ';' modifiers 'u'       # direct unicode code

Moreover, we translate the following deprecated special cases that do 
not fit into the standard key codes or escape sequence grammar:
---------------------------------------------------------------
  ESC '?' ..         ~>  ESC 'O' ..                 # vt52 treated as SS3
  ESC '[' 'O' ..     ~>  ESC '[' ..                 # ESC '[' 'O' [P-S]  is used in xterm for F1-F4
  ESC '[' .. '@'     ~>  ESC '[' '3' '~'            # Del on Mach
  ESC '[' .. '9'     ~>  ESC '[' '2' '~'            # Ins on Mach
  ESC .. [^@$]       ~>  ESC .. '~'                 # ETerm,xrvt,urxt: ^ = ctrl, $ = shift, @ = alt
  ESC '[' '[' [A-E]  ~>  ESC '[' .. [M-Q]           # Linux F1-F5
  ESC '[' [a-d]      ~>  ESC '[' '1' ';' '2' [A-D]  # Eterm cursor+shift
  ESC 'o' [a-d]      ~>  ESC '[' '1' ';' '5' [A-D]  # Eterm cursor+ctrl
  ESC 'O' [1-9] fin  ~>  ESC 'O' '1' ';' [1-9] fin  # Haiku puts modifiers as parameter 1

The modifiers are:
------------------
  1:  -           5: ctrl            9: alt  (for minicom)
  2:  shift       6: shift+ctrl
  3:  alt         7: alt+ctrl
  4:  shift+alt   8: shift+alt+ctrl

The different encodings fox vt100, xterm, and SS3 are:

vt100:  ESC [ vtcode ';' modifiers '~'
--------------------------------------
  1:  Home       10-15: F1-F5
  2:  Ins        16   : F5
  3:  Del        17-21: F6-F10
  4:  End        23-26: F11-F14
  5:  PageUp     28   : F15
  6:  PageDn     29   : F16
  7:  Home       31-34: F17-F20
  8:  End

xterm: ESC [ 1 ';' modifiers [A-Z]
-----------------------------------
  A: Up          N: F2        
  B: Down        O: F3       
  C: Right       P: F4       
  D: Left        Q: F5       
  E: '5'         R: F6       
  F: End         S: F7       
  G:             T: F8       
  H: Home        U: PageDn       
  I: PageUp      V: PageUp       
  J:             W: F11      
  K:             X: F12      
  L: Ins         Y: End      
  M: F1          Z: shift+Tab

SS3:   ESC 'O' 1 ';' modifiers [A-Za-z]
---------------------------------------
  (normal)                       (numpad)
  A: Up          N:              a: Up        n:           
  B: Down        O:              b: Down      o: 
  C: Right       P: F1           c: Right     p: Ins  
  D: Left        Q: F2           d: Left      q: End  
  E: '5'         R: F3           e:           r: Down 
  F: End         S: F4           f:           s: PageDn
  G:             T: F5           g:           t: Left 
  H: Home        U: F6           h:           u: '5'
  I: Tab         V: F7           i:           v: Right
  J:             W: F8           j: '*'       w: Home 
  K:             X: F9           k: '+'       x: Up 
  L:             Y: F10          l: ','       y: PageUp 
  M: \x0A '\n'   Z: shift+Tab    m: '-'       z:  
    
-------------------------------------------------------------*/

//-------------------------------------------------------------
// Decode escape sequences
//-------------------------------------------------------------

static code_t esc_decode_vt( uint32_t vt_code ) {
  switch(vt_code) {
    case 1: return KEY_HOME; 
    case 2: return KEY_INS;
    case 3: return KEY_DEL;
    case 4: return KEY_END;          
    case 5: return KEY_PAGEUP;
    case 6: return KEY_PAGEDOWN;
    case 7: return KEY_HOME;
    case 8: return KEY_END;          
    default: 
      if (vt_code >= 10 && vt_code <= 15) return KEY_F(1  + (vt_code - 10));
      if (vt_code == 16) return KEY_F5; // minicom
      if (vt_code >= 17 && vt_code <= 21) return KEY_F(6  + (vt_code - 17));
      if (vt_code >= 23 && vt_code <= 26) return KEY_F(11 + (vt_code - 23));
      if (vt_code >= 28 && vt_code <= 29) return KEY_F(15 + (vt_code - 28));
      if (vt_code >= 31 && vt_code <= 34) return KEY_F(17 + (vt_code - 31));
  }
  return KEY_NONE;
}

static code_t esc_decode_unicode( tty_t* tty, uint32_t unicode ) {
  // push unicode and pop the lead byte to return
  tty_cpush_unicode(tty,unicode);
  char c = 0;
  tty_cpop(tty,&c);
  return KEY_CHAR(c);
}

static code_t esc_decode_xterm( char xcode ) {
  // ESC [
  switch(xcode) {
    case 'A': return KEY_UP;
    case 'B': return KEY_DOWN;
    case 'C': return KEY_RIGHT;
    case 'D': return KEY_LEFT;
    case 'E': return '5';          // numpad 5
    case 'F': return KEY_END;
    case 'H': return KEY_HOME;
    case 'Z': return KEY_TAB | MOD_SHIFT;
    // Freebsd:
    case 'I': return KEY_PAGEUP;  
    case 'L': return KEY_INS;   
    case 'M': return KEY_F1;
    case 'N': return KEY_F2;
    case 'O': return KEY_F3;
    case 'P': return KEY_F4;       // note: differs from <https://en.wikipedia.org/wiki/ANSI_escape_code#CSI_(Control_Sequence_Introducer)_sequences>
    case 'Q': return KEY_F5;
    case 'R': return KEY_F6;
    case 'S': return KEY_F7;
    case 'T': return KEY_F8;
    case 'U': return KEY_PAGEDOWN; // Mach
    case 'V': return KEY_PAGEUP;   // Mach
    case 'W': return KEY_F11;
    case 'X': return KEY_F12;    
    case 'Y': return KEY_END;      // Mach    
  }
  return KEY_NONE;
}

static code_t esc_decode_ss3( char ss3_code ) {
  // ESC O 
  switch(ss3_code) {
    case 'A': return KEY_UP;
    case 'B': return KEY_DOWN;
    case 'C': return KEY_RIGHT;
    case 'D': return KEY_LEFT;
    case 'E': return '5';           // numpad 5
    case 'F': return KEY_END;
    case 'H': return KEY_HOME;
    case 'I': return KEY_TAB;
    case 'Z': return KEY_TAB | MOD_SHIFT;
    case 'M': return KEY_LINEFEED; 
    case 'P': return KEY_F1;
    case 'Q': return KEY_F2;
    case 'R': return KEY_F3;
    case 'S': return KEY_F4;
    // on Mach
    case 'T': return KEY_F5;
    case 'U': return KEY_F6;
    case 'V': return KEY_F7;
    case 'W': return KEY_F8;
    case 'X': return KEY_F9;  // = on vt220
    case 'Y': return KEY_F10;
    // numpad
    case 'a': return KEY_UP;
    case 'b': return KEY_DOWN;
    case 'c': return KEY_RIGHT;
    case 'd': return KEY_LEFT;
    case 'j': return '*';
    case 'k': return '+';
    case 'l': return ',';
    case 'm': return '-'; 
    case 'n': return KEY_DEL; // '.'
    case 'o': return '/'; 
    case 'p': return KEY_INS;
    case 'q': return KEY_END;  
    case 'r': return KEY_DOWN; 
    case 's': return KEY_PAGEDOWN; 
    case 't': return KEY_LEFT; 
    case 'u': return '5';
    case 'v': return KEY_RIGHT;
    case 'w': return KEY_HOME;  
    case 'x': return KEY_UP; 
    case 'y': return KEY_PAGEUP;   
  }
  return KEY_NONE;
}

static void tty_read_csi_num(tty_t* tty, char* ppeek, uint32_t* num) {
  *num = 1; // default
  int count = 0;
  uint32_t i = 0;
  while (*ppeek >= '0' && *ppeek <= '9' && count < 16) {    
    char digit = *ppeek - '0';
    if (!tty_readc_noblock(tty,ppeek)) break;  // peek is not modified in this case 
    count++;
    i = 10*i + (uint8_t)digit; 
  }
  if (count > 0) *num = i;
}

static code_t tty_read_csi(tty_t* tty, char c1, char peek) {
  // CSI starts with 0x9b (c1=='[') | ESC [ (c1=='[')
  // also process SS3 which starts with ESC O, ESC o, or ESC ? (on a vt52)  (c1=='O' or 'o' or '?')
  
  // "special" characters (includes non-standard '[' for linux function keys)
  char special = 0;
  if (strchr(":<=>?[",peek) != NULL) { 
    special = peek;
    if (!tty_readc_noblock(tty,&peek)) {  
      tty_cpush_char(tty,special); // recover
      return (KEY_CHAR(c1) | MOD_ALT);       // Alt+any
    }
  }

  // treat vt52 as standard SS3 (<https://www.xfree86.org/current/ctlseqs.html#VT52-Style%20Function%20Keys>)
  if (c1 == '?') {
    special = '?';
    c1 = 'O'; 
  }

  // handle xterm: ESC [ O [P-S]  and treat O as a special in that case.
  if (c1 == '[' && peek == 'O') {
    if (tty_readc_noblock(tty,&peek)) {
      if (peek >= 'P' && peek <= 'S') {
        // ESC [ O [P-S]   : used for F1-F4 on xterm
        special = 'O';  // make the O a special and continue
      }
      else {
        tty_cpush_char(tty,peek); // recover
        peek = 'O';
      }
    }
  }

  // up to 2 parameters that default to 1
  uint32_t num1 = 1;
  uint32_t num2 = 1;
  tty_read_csi_num(tty,&peek,&num1);
  if (peek == ';') {
    if (!tty_readc_noblock(tty,&peek)) return KEY_NONE;
    tty_read_csi_num(tty,&peek,&num2);
  }

  // the final character (we do not allow 'intermediate characters')
  char   final = peek;
  code_t modifiers = 0;

  debug_msg("tty: escape sequence: ESC %c %c %d;%d %c\n", c1, (special == 0 ? '_' : special), num1, num2, final);
  
  // Adjust special cases into standard ones.
  if ((final == '@' || final == '9') && c1 == '[' && num1 == 1) {
    // ESC [ @, ESC [ 9  : on Mach
    if (final == '@')      num1 = 3; // DEL
    else if (final == '9') num1 = 2; // INS 
    final = '~';
  }
  else if (final == '^' || final == '$' || final == '@') {  
    // Eterm/rxvt/urxt  
    if (final=='^') modifiers |= MOD_CTRL;
    if (final=='$') modifiers |= MOD_SHIFT;
    if (final=='@') modifiers |= MOD_SHIFT | MOD_CTRL;
    final = '~';
  }
  if (c1 == '[' && special == '[' && (final >= 'A' && final <= 'E')) {
    // ESC [ [ [A-E]  : linux F1-F5 codes
    final = 'M' + (final - 'A');  // map to xterm M-Q codes.
  }
  else if (c1 == '[' && final >= 'a' && final <= 'd') {
    // ESC [ [a-d]  : on Eterm for shift+ cursor
    modifiers |= MOD_SHIFT;
    final = 'A' + (final - 'a');
  }
  else if (c1 == 'o' && final >= 'a' && final <= 'd') {
    // ESC o [a-d]  : on Eterm these are ctrl+cursor
    c1 = '[';
    modifiers |= MOD_CTRL;
    final = 'A' + (final - 'a');  // to uppercase A - D.
  }
  else if (c1 == 'O' && num2 == 1 && num1 > 1 && num1 <= 8) {
    // on haiku the modifier can be parameter 1, make it parameter 2 instead
    num2 = num1;
    num1 = 1;
  }

  // parameter 2 determines the modifiers
  if (num2 > 1 && num2 <= 9) {
    if (num2 == 9) num2 = 3; // iTerm2 in xterm mode
    num2--;
    if (num2 & 0x1) modifiers |= MOD_SHIFT;
    if (num2 & 0x2) modifiers |= MOD_ALT;
    if (num2 & 0x4) modifiers |= MOD_CTRL;
  }

  // and translate
  code_t code = KEY_NONE;
  if (final == '~') {
    // vt codes
    code = esc_decode_vt(num1);
  }
  else if (final == 'u' && c1 == '[') {
    // unicode
    code = esc_decode_unicode(tty,num1);
  }
  else if (c1 == 'O' && ((final >= 'A' && final <= 'Z') || (final >= 'a' && final <= 'z'))) {
    // ss3
    code = esc_decode_ss3(final);
  }
  else if (num1 == 1 && final >= 'A' && final <= 'Z') {
    // xterm 
    code = esc_decode_xterm(final);
  }
  if (code == KEY_NONE) debug_msg("tty: ignore escape sequence: ESC %c1 %d;%d %c\n", c1, num1, num2, final);
  return (code != KEY_NONE ? (code | modifiers) : KEY_NONE);
}

internal code_t tty_read_esc(tty_t* tty) {
  char peek = 0;
  if (!tty_readc_noblock(tty,&peek)) return KEY_ESC; // ESC
  if (peek == '[') {
    if (!tty_readc_noblock(tty,&peek)) return ('[' | MOD_ALT);  // ESC [
    return tty_read_csi(tty,'[',peek);  // ESC [ ...
  }
  else if (peek == 'O' || peek == 'o' || peek == '?' /*vt52*/) {
    // SS3 ?
    char c1 = peek;
    if (!tty_readc_noblock(tty,&peek)) return (KEY_CHAR(c1) | MOD_ALT);  // ESC [Oo?]
    return tty_read_csi(tty,c1,peek);  // ESC [Oo?] ...
  }
  else {
    return (KEY_CHAR(peek) | MOD_ALT);  // ESC any    
  }  
}