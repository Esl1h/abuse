/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef _SETUP_H_
#define _SETUP_H_

struct flags_struct
{
    short fullscreen;
    short nosound;
    short grabmouse;
    short xres;
    short yres;

    // Whether the Remastered mode may borrow the original sound when the
    // player has installed it. On, because the mode ships none of its own
    // and a mute game is worse than a borrowed one; classicsfx=off in
    // abuserc turns it back off. Never applies to what is distributed:
    // this is the player's own copy of data they downloaded themselves.
    bool classic_sfx = true;
};

struct keys_struct
{
    int left;
    int left_2;
    int right;
    int right_2;
    int up;
    int up_2;
    int down;
    int down_2;
    int b1;
    int b2;
    int b3;
    int b4;
};

// Where abuserc was actually read from, resolved once in setup(). The options
// screen writes back to the same file rather than working it out again, so the
// two can never disagree.
char const *config_file_path();

// True when abuserc or the command line named a language. False means nobody
// has chosen: the language came from the system locale, or from the default.
bool language_was_configured();

#endif // _SETUP_H_
