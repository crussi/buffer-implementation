#ifndef INPUT_H
#define INPUT_H

#include "editor.h"

void input_handle_next_event(Editor *e);       // real terminal loop
void input_handle_key(Editor *e, int key);     // exported for testing
void input_handle_mouse(Editor *e, int y, int x); // exported for testing

#endif