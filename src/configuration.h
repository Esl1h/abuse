/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef __CONFIG_HPP_
#define __CONFIG_HPP_

#include <string>
#include <vector>

#include "input/actions.h"

enum { HIGH_DETAIL,
       MEDIUM_DETAIL,
       LOW_DETAIL,
       POOR_DETAIL };


// key_bindings does not appear to be used anywhere
//void key_bindings(int player, int &left, int &right, int &up, int &down, int &b1, int &b2, int &b3,  int &b4);
void get_key_bindings();

// Prints the resolved action map, for --dump-bindings.
void print_action_map();

// One `bind=` line from abuserc. False when the line is malformed.
bool add_config_binding(char const *spec);
bool config_has_explicit_binds();

// keypreset= in abuserc: "classic" or "modern".
bool apply_config_key_preset(char const *name);
void get_movement(int player, int &x, int &y, int &b1, int &b2, int &b3, int &b4);

// -1 for the previous weapon, 1 for the next, 0 for neither, reporting each
// press once. Weapon switching is not one of the eight packet bits, so it
// cannot be read from the packet the way movement is; the engine has always
// driven it from key events instead. Going through the action map here is what
// lets a pad button or a rebind do it too.
//
// Call it after every event and once per tick: sampling only per tick would
// miss a tap that starts and ends inside one, and 15 Hz ticks are 66 ms.
int consume_weapon_change();
// The live map, writable, for the rebind screen. resolve() reads it every
// tick, so a change here is in force on the next one.
abuse::input::ActionMap &mutable_action_map();

// One binding as the player should read it: a key name, a pad button under the
// label its family prints, or a mouse button.
void describe_binding(abuse::input::Binding const &b, char *out, int out_size);

// Every binding as `bind=` lines, ready for the config file.
std::vector<std::string> format_all_bindings();

// From here on the map is the player's, so nothing rebuilds it from the
// legacy keys behind their back.
void mark_explicit_binds();

void config_cleanup();  // free any memory allocated
int get_keycode(char const *str);  // -1 means not a valid key code

#endif
