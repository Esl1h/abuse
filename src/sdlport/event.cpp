/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 2001 Anthony Kruize <trandor@labyrinth.net.au>
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#include <SDL3/SDL.h>

#include "common.h"
#include "input/gamepad.h"

#include "image.h"
#include "palette.h"
#include "video.h"
#include "event.h"
#include "timing.h"
#include "sprite.h"
#include "game.h"
#include "setup.h"

extern SDL_Window *window;
extern SDL_Surface *surface;
// Need the renderer to figure out mouse events
extern SDL_Renderer* renderer;

// Set while the pad's confirm button is held, so the menus can see it as a
// left click. Not part of PadState because it is about the pointer, not about
// the action map.
static bool g_pad_click = false;
extern flags_struct flags;
extern int get_key_binding(char const *dir, int i);
extern float mouse_yscale;
short mouse_buttons[5] = { 0, 0, 0, 0, 0 };
// From setup.cpp:
void video_change_settings(void);

void EventHandler::SysInit()
{
    // Ignore activate events
    // This event is gone in SDL2, should we be ignoring the replacement? Dunno
    //SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
}

// True while the pad's face buttons belong to the player, moving and shooting.
// Everywhere else they have to confirm and cancel instead, or the player ends
// up somewhere with no way out: paused, with only the keyboard able to resume,
// or in front of a window that answers to clicks and nothing else.
static bool pad_faces_are_gameplay()
{
    if (!the_game || !playing_state(the_game->state))
        return false;
    if (the_game->state == PAUSE_STATE)
        return false;
    if (wm && wm->has_visible_window())
        return false;
    return true;
}

void EventHandler::SysWarpMouse(ivec2 pos)
{
    // Calculate window position
    float fx = pos.x;
    float fy = pos.y / mouse_yscale;
    SDL_RenderCoordinatesToWindow(renderer, fx, fy, &fx, &fy);
    SDL_WarpMouseInWindow(window, fx, fy);
}

void EventHandler::SysPumpCursor()
{
    // Pixels per tick at 15 ticks a second, turned into what this call is
    // worth. Sampled against the clock and not counted per call, because a
    // modal loop spins as fast as it can and would fling the cursor across
    // the screen.
    static uint64_t last = 0;
    uint64_t now = SDL_GetTicks();
    if (last == 0 || now < last)
    {
        last = now;
        return;
    }

    uint64_t elapsed = now - last;
    if (elapsed < 16)           // about one step per display frame
        return;
    last = now;

    abuse::input::Deadzone const &dz = abuse::input::deadzone_for_axis(2);
    float x = abuse::input::pad_state().axis_scaled(2, dz);
    float y = abuse::input::pad_state().axis_scaled(3, dz);
    if (x == 0.0f && y == 0.0f)
    {
        abuse::input::Deadzone const &ldz = abuse::input::deadzone_for_axis(0);
        x = abuse::input::pad_state().axis_scaled(0, ldz);
        y = abuse::input::pad_state().axis_scaled(1, ldz);
    }

    // The d-pad as well, which is buttons and not an axis.
    //
    // Without this the sticks were the only way to move the pointer, and
    // every dialogue the 1995 code draws is picked with the pointer: a
    // player reported that the save-slot picker could not be driven with
    // the d-pad at all, which is exactly what this was.
    if (x == 0.0f && y == 0.0f)
    {
        abuse::input::PadState const &pad = abuse::input::pad_state();
        if (pad.button(SDL_GAMEPAD_BUTTON_DPAD_LEFT))  x -= 1.0f;
        if (pad.button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) x += 1.0f;
        if (pad.button(SDL_GAMEPAD_BUTTON_DPAD_UP))    y -= 1.0f;
        if (pad.button(SDL_GAMEPAD_BUTTON_DPAD_DOWN))  y += 1.0f;
    }

    if (x == 0.0f && y == 0.0f)
        return;

    int budget = (int)((float)abuse::input::cursor_settings().speed * 15.0f
                       * (float)elapsed / 1000.0f + 0.5f);
    if (budget < 1)
        budget = 1;

    abuse::input::CursorStep step = abuse::input::menu_cursor_step(x, y, budget);
    if (step.x == 0 && step.y == 0)
        return;

    // Through the system mouse: the windows decide what is under the cursor
    // from motion events, and the warp is what produces one.
    SetMousePos(m_pos + ivec2(step.x, step.y));
}

//
// IsPending()
// Are there any events in the queue?
//
int EventHandler::IsPending()
{
    if (!m_pending && SDL_PollEvent(NULL))
        m_pending = 1;

    return m_pending;
}

//
// Get and handle waiting events
//
void EventHandler::SysEvent(Event &ev)
{
    // No more events
    m_pending = 0;

    // NOTE : that the mouse status should be known
    // even if another event has occurred.
    ev.mouse_move.x = m_pos.x;
    ev.mouse_move.y = m_pos.y;
    ev.mouse_button = m_button;

    // Gather next event
    SDL_Event sdlev;
    if (!SDL_PollEvent(&sdlev))
        return; // This should not happen

    // Sort the mouse out
    int x, y;
    float fx, fy;
    uint8_t buttons = SDL_GetMouseState(&fx, &fy);

    // The menus and the save-slot picker are driven by the mouse: items react
    // to clicks and to hotkey letters, never to arrow keys. Making the pad's
    // confirm button act as a left click is what lets a player reach them
    // without putting the controller down. Only outside a level, where the
    // same button is the jump.
    if (g_pad_click && !pad_faces_are_gameplay())
        buttons |= SDL_BUTTON_MASK(1);
    // Make the window-relative position renderer-relative
    SDL_RenderCoordinatesFromWindow(renderer, fx, fy, &fx, &fy);
    // Don't care about subpixels
    x = (int) fx;
    y = (int) (fy * mouse_yscale);
    ev.mouse_move.x = x;
    ev.mouse_move.y = y;
    ev.type = EV_MOUSE_MOVE;

    // Left button
    if((buttons & SDL_BUTTON_MASK(1)) && !mouse_buttons[1])
    {
        ev.type = EV_MOUSE_BUTTON;
        mouse_buttons[1] = !mouse_buttons[1];
        ev.mouse_button |= LEFT_BUTTON;
    }
    else if(!(buttons & SDL_BUTTON_MASK(1)) && mouse_buttons[1])
    {
        ev.type = EV_MOUSE_BUTTON;
        mouse_buttons[1] = !mouse_buttons[1];
        ev.mouse_button &= (0xff - LEFT_BUTTON);
    }

    // Middle button
    if((buttons & SDL_BUTTON_MASK(2)) && !mouse_buttons[2])
    {
        ev.type = EV_MOUSE_BUTTON;
        mouse_buttons[2] = !mouse_buttons[2];
        ev.mouse_button |= LEFT_BUTTON;
        ev.mouse_button |= RIGHT_BUTTON;
    }
    else if(!(buttons & SDL_BUTTON_MASK(2)) && mouse_buttons[2])
    {
        ev.type = EV_MOUSE_BUTTON;
        mouse_buttons[2] = !mouse_buttons[2];
        ev.mouse_button &= (0xff - LEFT_BUTTON);
        ev.mouse_button &= (0xff - RIGHT_BUTTON);
    }

    // Right button
    if((buttons & SDL_BUTTON_MASK(3)) && !mouse_buttons[3])
    {
        ev.type = EV_MOUSE_BUTTON;
        mouse_buttons[3] = !mouse_buttons[3];
        ev.mouse_button |= RIGHT_BUTTON;
    }
    else if(!(buttons & SDL_BUTTON_MASK(3)) && mouse_buttons[3])
    {
        ev.type = EV_MOUSE_BUTTON;
        mouse_buttons[3] = !mouse_buttons[3];
        ev.mouse_button &= (0xff - RIGHT_BUTTON);
    }
    m_pos = ivec2(ev.mouse_move.x, ev.mouse_move.y);
    m_button = ev.mouse_button;

    // Sort out other kinds of events
    switch(sdlev.type)
    {
    case SDL_EVENT_QUIT:
        exit(0);
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        if (m_ignore_wheel_events)
            break;
        // Conceptually this can be in multiple directions, so use left/right
        // first because those match the bars on the button
        if (sdlev.wheel.x < 0)
        {
            ev.key = get_key_binding("b4", 0);
            ev.type = EV_KEY;
        }
        else if (sdlev.wheel.x > 0)
        {
            ev.key = get_key_binding("b3", 0);
            ev.type = EV_KEY;
        }
        else if (sdlev.wheel.y < 0)
        {
            ev.key = get_key_binding("b4", 0);
            ev.type = EV_KEY;
        }
        else if (sdlev.wheel.y > 0)
        {
            ev.key = get_key_binding("b3", 0);
            ev.type = EV_KEY;
        }
        if (ev.type == EV_KEY)
        {
            // We also need to immediately queue a "release" event or this will
            // be stuck down forever.
            Event *release_event = new Event();
            release_event->key = ev.key;
            release_event->type = EV_KEYRELEASE;
            Push(release_event);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        // These were the old mouse wheel handlers, but honestly, using
        // B4 and B5 for weapon switching works.
        switch(sdlev.button.button)
        {
        case 4:        // Mouse wheel goes up...
            ev.key = get_key_binding("b4", 0);
            ev.type = EV_KEYRELEASE;
            break;
        case 5:        // Mouse wheel goes down...
            ev.key = get_key_binding("b3", 0);
            ev.type = EV_KEYRELEASE;
            break;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        switch(sdlev.button.button)
        {
        case 4:        // Mouse wheel goes up...
            ev.key = get_key_binding("b4", 0);
            ev.type = EV_KEY;
            break;
        case 5:        // Mouse wheel goes down...
            ev.key = get_key_binding("b3", 0);
            ev.type = EV_KEY;
            break;
        }
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        // Default to EV_SPURIOUS
        ev.key = EV_SPURIOUS;
        if(sdlev.type == SDL_EVENT_KEY_DOWN)
        {
            ev.type = EV_KEY;
        }
        else
        {
            ev.type = EV_KEYRELEASE;
        }
        switch(sdlev.key.key)
        {
        case SDLK_DOWN:         ev.key = JK_DOWN; break;
        case SDLK_UP:           ev.key = JK_UP; break;
        case SDLK_LEFT:         ev.key = JK_LEFT; break;
        case SDLK_RIGHT:        ev.key = JK_RIGHT; break;
        case SDLK_LCTRL:        ev.key = JK_CTRL_L; break;
        case SDLK_RCTRL:        ev.key = JK_CTRL_R; break;
        case SDLK_LGUI:         ev.key = JK_COMMAND; break;
        case SDLK_RGUI:         ev.key = JK_COMMAND; break;
        case SDLK_LALT:         ev.key = JK_ALT_L; break;
        case SDLK_RALT:         ev.key = JK_ALT_R; break;
        case SDLK_LSHIFT:       ev.key = JK_SHIFT_L; break;
        case SDLK_RSHIFT:       ev.key = JK_SHIFT_R; break;
        case SDLK_NUMLOCKCLEAR: ev.key = JK_NUM_LOCK; break;
        case SDLK_HOME:         ev.key = JK_HOME; break;
        case SDLK_END:          ev.key = JK_END; break;
        case SDLK_BACKSPACE:    ev.key = JK_BACKSPACE; break;
        case SDLK_TAB:          ev.key = JK_TAB; break;
        case SDLK_RETURN:       ev.key = JK_ENTER; break;
        case SDLK_SPACE:        ev.key = JK_SPACE; break;
        case SDLK_CAPSLOCK:     ev.key = JK_CAPS; break;
        case SDLK_ESCAPE:       ev.key = JK_ESC; break;
        case SDLK_F1:           ev.key = JK_F1; break;
        case SDLK_F2:           ev.key = JK_F2; break;
        case SDLK_F3:           ev.key = JK_F3; break;
        case SDLK_F4:           ev.key = JK_F4; break;
        case SDLK_F5:           ev.key = JK_F5; break;
        case SDLK_F6:           ev.key = JK_F6; break;
        case SDLK_F7:           ev.key = JK_F7; break;
        case SDLK_F8:           ev.key = JK_F8; break;
        case SDLK_F9:           ev.key = JK_F9; break;
        case SDLK_F10:          ev.key = JK_F10; break;
        case SDLK_INSERT:       ev.key = JK_INSERT; break;
        case SDLK_KP_0:         ev.key = JK_INSERT; break;
        case SDLK_PAGEUP:       ev.key = JK_PAGEUP; break;
        case SDLK_PAGEDOWN:     ev.key = JK_PAGEDOWN; break;
        case SDLK_KP_8:         ev.key = JK_UP; break;
        case SDLK_KP_2:         ev.key = JK_DOWN; break;
        case SDLK_KP_4:         ev.key = JK_LEFT; break;
        case SDLK_KP_6:         ev.key = JK_RIGHT; break;
        case SDLK_F11:
            // FIXME: This should really be ALT-ENTER
            // Only handle key down
            if(ev.type == EV_KEY)
            {
                // Toggle fullscreen
                flags.fullscreen = !flags.fullscreen;
                video_change_settings();
            }
            ev.key = EV_SPURIOUS;
            break;
        case SDLK_F12:
        /* FIXME
            // Only handle key down
            if(ev.type == EV_KEY)
            {
                // Toggle grab mouse
                if(SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_ON)
                {
                    the_game->show_help("Grab Mouse: OFF\n");
                    SDL_WM_GrabInput(SDL_GRAB_OFF);
                }
                else
                {
                    the_game->show_help("Grab Mouse: ON\n");
                    SDL_WM_GrabInput(SDL_GRAB_ON);
                }
            }
            */
            ev.key = EV_SPURIOUS;
            break;
        case SDLK_PRINTSCREEN:    // print-screen key
            // Only handle key down
            if(ev.type == EV_KEY)
            {
                // Grab a screenshot
                SDL_SaveBMP(surface, "screenshot.bmp");
                the_game->show_help("Screenshot saved to: screenshot.bmp.\n");
            }
            ev.key = EV_SPURIOUS;
            break;
        default:
            ev.key = (int)sdlev.key.key;
            // Need to handle the case of shift being pressed
            // There has to be a better way
            if((sdlev.key.mod & SDL_KMOD_SHIFT) != 0)
            {
                if(sdlev.key.key >= SDLK_A &&
                    sdlev.key.key <= SDLK_Z)
                {
                    ev.key -= 32;
                }
                else if(sdlev.key.key >= SDLK_1 &&
                         sdlev.key.key <= SDLK_5)
                {
                    ev.key -= 16;
                }
                else
                {
                    switch(sdlev.key.key)
                    {
                    case SDLK_6:
                        ev.key = SDLK_CARET; break;
                    case SDLK_7:
                    case SDLK_9:
                    case SDLK_0:
                        ev.key -= 17; break;
                    case SDLK_8:
                        ev.key = SDLK_ASTERISK; break;
                    case SDLK_MINUS:
                        ev.key = SDLK_UNDERSCORE; break;
                    case SDLK_EQUALS:
                        ev.key = SDLK_PLUS; break;
                    case SDLK_COMMA:
                        ev.key = SDLK_LESS; break;
                    case SDLK_PERIOD:
                        ev.key = SDLK_GREATER; break;
                    case SDLK_SLASH:
                        ev.key = SDLK_QUESTION; break;
                    case SDLK_SEMICOLON:
                        ev.key = SDLK_COLON; break;
                    case SDLK_APOSTROPHE:
                        ev.key = SDLK_DBLAPOSTROPHE; break;
                    default:
                        break;
                    }
                }
            }
            break;
        }
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
    {
        // Opening it is what makes SDL deliver its events at all.
        SDL_Gamepad *added = SDL_OpenGamepad(sdlev.gdevice.which);
        abuse::input::pad_state().reset();
        abuse::input::set_pad_lost(false);

        // Pick the button labels from what SDL says the pad is, so the game
        // can name buttons the way they are printed on this controller.
        if (added)
        {
            using abuse::input::PadFamily;
            switch (SDL_GetGamepadType(added))
            {
            // STANDARD is SDL's word for a pad that follows the Xbox face
            // layout without being an Xbox pad. The 8BitDo Ultimate 2 tested
            // on this host reports it, and has A/B/X/Y printed on it, so
            // falling through to the generic "1/2/3/4" would name its buttons
            // wrong.
            case SDL_GAMEPAD_TYPE_STANDARD:
            case SDL_GAMEPAD_TYPE_XBOX360:
            case SDL_GAMEPAD_TYPE_XBOXONE:
                abuse::input::pad_family() = PadFamily::Xbox;
                break;
            case SDL_GAMEPAD_TYPE_PS3:
            case SDL_GAMEPAD_TYPE_PS4:
            case SDL_GAMEPAD_TYPE_PS5:
                abuse::input::pad_family() = PadFamily::PlayStation;
                break;
            case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
            case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
            case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
            case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
            case SDL_GAMEPAD_TYPE_GAMECUBE:
                abuse::input::pad_family() = PadFamily::Nintendo;
                break;
            default:
                abuse::input::pad_family() = PadFamily::Generic;
                break;
            }
        }
        ev.key = EV_SPURIOUS;
        break;
    }
    case SDL_EVENT_GAMEPAD_REMOVED:
    {
        // Whatever was held when the pad went away must not stay held.
        abuse::input::pad_state().disconnect();
        abuse::input::set_pad_lost(true);

        // Pausing is the point: a player mid-level with a controller that
        // stopped answering needs the game to wait, not to keep running while
        // they look for the cable.
        if (the_game && playing_state(the_game->state)
            && the_game->state != PAUSE_STATE)
            the_game->set_state(PAUSE_STATE);
        SDL_Gamepad *gone = SDL_GetGamepadFromID(sdlev.gdevice.which);
        if (gone)
            SDL_CloseGamepad(gone);
        ev.key = EV_SPURIOUS;
        break;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        // Task 3.3: the action map reads this state each tick. The synthetic
        // key press below stays for now, because everything outside the eight
        // packet actions still travels as a key code.
        abuse::input::pad_state().set_button(
            sdlev.gbutton.button,
            sdlev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);

        switch (sdlev.gbutton.button)
        {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            ev.key = get_key_binding("up", 0);
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            ev.key = get_key_binding("down", 0);
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            ev.key = get_key_binding("left", 0);
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            ev.key = get_key_binding("right", 0);
            break;
        case SDL_GAMEPAD_BUTTON_START:
            // Start opens the menu, which is this game's pause menu. Works in
            // and out of a level.
            ev.key = JK_ESC;
            break;
        case SDL_GAMEPAD_BUTTON_BACK:
            // The pause overlay, which the keyboard reaches with 'p'.
            ev.key = 'p';
            break;
        case SDL_GAMEPAD_BUTTON_SOUTH:
            // Confirm. Outside gameplay it also clicks, above, because the
            // menus have no keyboard navigation to fall back on.
            g_pad_click = sdlev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
            ev.key = pad_faces_are_gameplay() ? -1 : JK_ENTER;
            break;
        case SDL_GAMEPAD_BUTTON_NORTH:
            // The options screen, which the keyboard reaches with F2. Nothing
            // in the game is bound to this button, in a level or out of one.
            ev.key = JK_F2;
            break;
        case SDL_GAMEPAD_BUTTON_WEST:
            // The controls screen, F3 on the keyboard. Same reasoning.
            ev.key = JK_F3;
            break;
        case SDL_GAMEPAD_BUTTON_EAST:
            // Cancel. During play this is the special weapon, so it must stay
            // quiet or using the special would back out of the level.
            ev.key = pad_faces_are_gameplay() ? -1 : JK_ESC;
            break;
        default:
            // Still want to process this as a key press if only to allow the
            // controller to skip the intro screen.
            ev.key = -1;
        }
        ev.type = sdlev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ?
            EV_KEY : EV_KEYRELEASE;
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        abuse::input::pad_state().set_axis(sdlev.gaxis.axis, sdlev.gaxis.value);

        switch (sdlev.gaxis.axis)
        {
        case SDL_GAMEPAD_AXIS_LEFTX:
        case SDL_GAMEPAD_AXIS_LEFTY:
        {
            // Gameplay reads the left stick through the action map, which asks
            // pad_state each tick. These synthetic key events exist only so the
            // stick can also drive the menus, which read key events.
            //
            // They are emitted on transitions, from a direction the map agrees
            // is active, and no longer from the sign of the raw value at the
            // moment of release. Deciding from the sign is what left a
            // direction stuck when a stick settled a hair past centre.
            bool const horizontal = sdlev.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX;
            int &prev = horizontal ? m_left_stick_x_dir : m_left_stick_y_dir;

            abuse::input::Deadzone const &dz =
                abuse::input::deadzone_for_axis(sdlev.gaxis.axis);
            int now = 0;
            if (abuse::input::pad_state().axis_active(sdlev.gaxis.axis, -1, dz))
                now = -1;
            else if (abuse::input::pad_state().axis_active(sdlev.gaxis.axis, 1, dz))
                now = 1;

            if (now == prev)
            {
                ev.key = EV_SPURIOUS;
                break;
            }

            char const *name;
            if (prev != 0)
            {
                // Release what was held; the press for the new direction
                // follows on the next axis event, which a moving stick always
                // sends.
                name = horizontal ? (prev < 0 ? "left" : "right")
                                  : (prev < 0 ? "up" : "down");
                ev.key = get_key_binding(name, 0);
                ev.type = EV_KEYRELEASE;
                prev = 0;
            }
            else
            {
                name = horizontal ? (now < 0 ? "left" : "right")
                                  : (now < 0 ? "up" : "down");
                ev.key = get_key_binding(name, 0);
                ev.type = EV_KEY;
                prev = now;
            }
            break;
        }
        // Inside a level the right stick does not touch the mouse at all: the
        // aim is a vector from the player, recomputed every tick in view.cpp
        // from pad_state, and the crosshair is placed from that same vector.
        // Moving the mouse here put the crosshair at a per-axis position while
        // the shot went to the circle-clamped one, so the two disagreed
        // diagonally, and the warp fed itself back as mouse movement.
        case SDL_GAMEPAD_AXIS_RIGHTX:
            // Outside a level there is no player to orbit, so the stick is a
            // plain mouse, which is what drives the menus.
            if (!m_right_stick_locked
                && (abuse::input::pad_state().axis_active(sdlev.gaxis.axis, -1,
                        abuse::input::deadzone_for_axis(sdlev.gaxis.axis))
                    || abuse::input::pad_state().axis_active(sdlev.gaxis.axis, 1,
                        abuse::input::deadzone_for_axis(sdlev.gaxis.axis)))) {
                m_pos.x += sdlev.gaxis.value / m_right_stick_scale;
                ev.mouse_move.x = m_pos.x;
                SetMousePos(m_pos);
            }
            break;
        case SDL_GAMEPAD_AXIS_RIGHTY:
            if (!m_right_stick_locked
                && (abuse::input::pad_state().axis_active(sdlev.gaxis.axis, -1,
                        abuse::input::deadzone_for_axis(sdlev.gaxis.axis))
                    || abuse::input::pad_state().axis_active(sdlev.gaxis.axis, 1,
                        abuse::input::deadzone_for_axis(sdlev.gaxis.axis)))) {
                m_pos.y += sdlev.gaxis.value / m_right_stick_scale;
                ev.mouse_move.y = m_pos.y;
                SetMousePos(m_pos);
            }
            break;
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
            // Left trigger: special
            ev.key = get_key_binding("b1", 0);
            if (sdlev.gaxis.value > m_dead_zone)
            {
                // Go ahead and spam key-ups/key-downs, I guess
                ev.type = EV_KEY;
            }
            else
            {
                ev.type = EV_KEYRELEASE;
            }
            break;
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
            // Right trigger: fire
            ev.key = get_key_binding("b2", 0);
            if (sdlev.gaxis.value > m_dead_zone)
            {
                // Go ahead and spam key-ups/key-downs, I guess
                ev.type = EV_KEY;
            }
            else
            {
                ev.type = EV_KEYRELEASE;
            }
            break;
        }
    }
}
