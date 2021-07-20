/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/

//-------------------------------------------------------------
// Completions: this file is included in editline.c
//-------------------------------------------------------------

static void edit_complete(rp_env_t* env, editor_t* eb, ssize_t idx) {
  editor_start_modify(eb);

  eb->pos = completions_apply(env->completions, idx, eb->input, eb->pos);
  edit_refresh(env,eb);
}

static void editor_append_completion(rp_env_t* env, editor_t* eb, ssize_t idx, ssize_t width, bool numbered, bool selected ) {
  const char* display = completions_get_display(env->completions, idx);
  if (display == NULL) return;
  if (numbered) {
    char buf[32];
    snprintf(buf, 32, "\x1B[90m%s%zd \x1B[0m", (selected ? (eb->is_utf8 ? "\xE2\x86\x92" : "*") : " "), 1 + idx);
    sbuf_append(eb->extra, buf);
    width -= 3;
  }

  if (width <= 0) {
    sbuf_append(eb->extra, display);
  }
  else {
    // fit to width
    const char* sc = str_skip_until_fit( display, width, eb->is_utf8);
    if (sc != display) {
      sbuf_append( eb->extra, "...");
      sc = str_skip_until_fit( display, width - 3, eb->is_utf8);
    }
    sbuf_append( eb->extra, sc);
    // fill out with spaces
    ssize_t n = width - str_column_width(sc, eb->is_utf8);
    while( n-- > 0 ) { sbuf_append( eb->extra," "); }  
  }
}

// 2 and 3 column output up to 80 wide
#define RP_DISPLAY2_MAX    35
#define RP_DISPLAY2_COL    (3+RP_DISPLAY2_MAX)
#define RP_DISPLAY2_WIDTH  (2*RP_DISPLAY2_COL + 2)    // 78

#define RP_DISPLAY3_MAX    22
#define RP_DISPLAY3_COL    (3+RP_DISPLAY3_MAX)
#define RP_DISPLAY3_WIDTH  (3*RP_DISPLAY3_COL + 2*2)  // 79

static void editor_append_completion2(rp_env_t* env, editor_t* eb, ssize_t idx1, ssize_t idx2, ssize_t selected ) {  
  editor_append_completion(env, eb, idx1, RP_DISPLAY2_COL, true, (idx1 == selected) );
  sbuf_append( eb->extra, "  ");
  editor_append_completion(env, eb, idx2, RP_DISPLAY2_COL, true, (idx2 == selected) );
}

static void editor_append_completion3(rp_env_t* env, editor_t* eb, ssize_t idx1, ssize_t idx2, ssize_t idx3, ssize_t selected ) {  
  editor_append_completion(env, eb, idx1, RP_DISPLAY3_COL, true, (idx1 == selected) );
  sbuf_append( eb->extra, "  ");
  editor_append_completion(env, eb, idx2, RP_DISPLAY3_COL, true, (idx2 == selected));
  sbuf_append( eb->extra, "  ");
  editor_append_completion(env, eb, idx3, RP_DISPLAY3_COL, true, (idx3 == selected) );
}

static ssize_t edit_completions_max_width( rp_env_t* env, editor_t* eb, ssize_t count ) {
  ssize_t max_width = 0;
  for( ssize_t i = 0; i < count; i++) {
    const char* display = completions_get_display(env->completions,i);
    if (display != NULL) {
      ssize_t w = str_column_width( display, eb->is_utf8);
      if (w > max_width) max_width = w;
    }
  }
  return max_width;
}

static void edit_completion_menu(rp_env_t* env, editor_t* eb, bool more_available) {
  ssize_t count = completions_count(env->completions);
  ssize_t count_displayed = count;
  assert(count > 1);
  ssize_t selected = 0;
  ssize_t columns = 1;
  ssize_t percolumn = count;

again:
  // show first 9 (or 8) completions
  sbuf_clear(eb->extra);
  ssize_t twidth = term_get_width(env->term);
  if (count > 3 && twidth > RP_DISPLAY3_WIDTH && edit_completions_max_width(env, eb, 9) <= RP_DISPLAY3_MAX) {
    // display as a 3 column block
    count_displayed = (count > 9 ? 9 : count);
    columns = 3;
    percolumn = 3;
    for (ssize_t rw = 0; rw < percolumn; rw++) {
      if (rw > 0) sbuf_append(eb->extra, "\n");
      editor_append_completion3(env, eb, rw, percolumn+rw, (2*percolumn)+rw, selected);
    }
  }
  else if (count > 4 && twidth > RP_DISPLAY2_WIDTH && edit_completions_max_width(env, eb, 8) <= RP_DISPLAY2_MAX) {
    // display as a 2 column block if some entries are too wide for three columns
    count_displayed = (count > 8 ? 8 : count);
    columns = 2;
    percolumn = (count_displayed <= 6 ? 3 : 4);
    for (ssize_t rw = 0; rw < percolumn; rw++) {
      if (rw > 0) sbuf_append(eb->extra, "\n");
      editor_append_completion2(env, eb, rw, percolumn+rw, selected);
    }
  }
  else {
    // display as a list
    count_displayed = (count > 9 ? 9 : count);
    columns = 1;
    percolumn = count_displayed;
    for (ssize_t i = 0; i < count_displayed; i++) {
      if (i > 0) sbuf_append(eb->extra, "\n");
      editor_append_completion(env, eb, i, -1, true /* numbered */, selected == i);
    }
  }
  if (count > count_displayed) {
    sbuf_append(eb->extra, "\n\x1B[90m(press shift-tab to see all further completions)\x1B[0m");
  }
  edit_refresh(env, eb);

  // read here; if not a valid key, push it back and return to main event loop
  code_t c = tty_read(env->tty);
  sbuf_clear(eb->extra);
  if (c >= '1' && c <= '9' && (ssize_t)(c - '1') < count) {
    selected = (c - '1');
    c = KEY_SPACE;
  }
  else if (c == KEY_TAB || c == KEY_DOWN) {
    selected++;
    if (selected >= count_displayed) selected = 0;
    goto again;
  }
  else if (c == KEY_UP) {
    selected--;
    if (selected < 0) selected = count_displayed - 1;
    goto again;
  }
  if (c == KEY_RIGHT) {
    if (columns > 1 && selected + percolumn < count_displayed) selected += percolumn;
    goto again;
  }
  if (c == KEY_LEFT) {
    if (columns > 1 && selected - percolumn >= 0) selected -= percolumn;
    goto again;
  }
  else if (c == KEY_END) {
    selected = count_displayed - 1;
    goto again;
  }
  else if (c == KEY_HOME) {
    selected = 0;
    goto again;
  }
  else if (c == KEY_F1) {
    edit_show_help(env, eb);
    goto again;
  }
  else if (c == KEY_ESC) {
    completions_clear(env->completions);
    edit_refresh(env,eb);
    c = 0; // ignore and return
  }
  else if (c == KEY_ENTER || c == KEY_SPACE) {  
    // select the current entry
    assert(selected < count);
    c = 0;      
    edit_complete(env, eb, selected);
  }
  else if ((c == KEY_PAGEDOWN || c == KEY_SHIFT_TAB || c == KEY_LINEFEED) && count > 9) {
    // show all completions
    c = 0;
    if (more_available) {
      // generate all entries (up to the max (= 1000))
      count = completions_generate(env, env->completions, sbuf_string(eb->input), eb->pos, RP_MAX_COMPLETIONS_TO_SHOW);
    }
    rowcol_t rc;
    edit_get_rowcol(env,eb,&rc);
    edit_clear(env,eb);
    edit_write_prompt(env,eb,0,false);
    term_write(env->term, "\r\n");
    for(ssize_t i = 0; i < count; i++) {
      const char* display = completions_get_display(env->completions, i);
      if (display != NULL) {
        // term_writef(env->term, "\x1B[90m%3d \x1B[0m%s\r\n", i+1, (cm->display != NULL ? cm->display : cm->replacement ));          
        term_write(env->term, display);         
        term_write(env->term, "\r\n"); 
      }
    }
    if (count >= RP_MAX_COMPLETIONS_TO_SHOW) {
      term_write(env->term, "\x1B[90m... and more.\x1B[0m\r\n");
    }
    for(ssize_t i = 0; i < rc.row+1; i++) {
      term_write(env->term, " \r\n");
    }
    eb->cur_rows = 0;
    edit_refresh(env,eb);      
  }
  // done
  completions_clear(env->completions);      
  if (c != 0) tty_code_pushback(env->tty,c);
}

static void edit_generate_completions(rp_env_t* env, editor_t* eb) {
  debug_msg( "edit: complete: %zd: %s\n", eb->pos, sbuf_string(eb->input) );
  if (eb->pos <= 0) return;
  ssize_t count = completions_generate(env, env->completions, sbuf_string(eb->input), eb->pos, 10);
  if (count <= 0) {
    // no completions
    term_beep(env->term); 
  }
  else if (count == 1) {
    // complete if only one match    
    edit_complete(env,eb,0);
  }
  else {
    edit_completion_menu( env, eb, count>=10 /* possibly more available? */);    
  }
}