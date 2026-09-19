/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See aim.h.
 *
 *  This software was released into the Public Domain.
 */

#include "aim.h"

#include <math.h>

#include "gamepad.h"

namespace abuse::input {

namespace {

AimVector g_aim;
bool g_pad_aim = false;

}

AimVector &aim_vector()
{
    return g_aim;
}

bool pad_aim_active()
{
    return g_pad_aim;
}

void set_pad_aim_active(bool active)
{
    g_pad_aim = active;
}

AimVector aim_offset_from_axes(float x, float y, int radius)
{
    if (radius < 1)
        radius = 1;

    // Clamp the vector to the circle rather than the square the axes describe,
    // so a diagonal reaches as far as a cardinal and no further. Without this
    // the crosshair travels about 40% further towards the corners.
    float len = sqrtf(x * x + y * y);
    if (len > 1.0f)
    {
        x /= len;
        y /= len;
    }

    AimVector v;
    v.x = (int)(x * (float)radius);
    v.y = (int)(y * (float)radius);
    return v;
}

AssistSettings &assist_settings()
{
    static AssistSettings g_assist;
    return g_assist;
}

AimVector assist_aim(AimVector const &aim, AimTarget const *targets, int count,
                     AssistSettings const &settings)
{
    if (settings.strength <= 0 || !targets || count <= 0)
        return aim;

    double aim_len = sqrt((double)aim.x * aim.x + (double)aim.y * aim.y);
    if (aim_len < 1.0)
        return aim;             // no direction to speak of, nothing to bias

    double ax = aim.x / aim_len;
    double ay = aim.y / aim_len;

    // cos of the cone half-angle: a candidate qualifies when the dot product
    // of the two unit vectors is at least this.
    double cone = cos((double)settings.cone_degrees * M_PI / 180.0);

    int best = -1;
    double best_dot = cone;
    for (int i = 0; i < count; i++)
    {
        double tx = targets[i].dx;
        double ty = targets[i].dy;
        double len = sqrt(tx * tx + ty * ty);
        if (len < 1.0)
            continue;           // on top of the player: gives no direction

        double dot = (ax * tx + ay * ty) / len;
        // Strictly greater, so the first of two equally aligned candidates
        // wins and the choice stays stable from tick to tick.
        if (dot > best_dot)
        {
            best_dot = dot;
            best = i;
        }
    }

    if (best < 0)
        return aim;

    double tx = targets[best].dx;
    double ty = targets[best].dy;
    double len = sqrt(tx * tx + ty * ty);
    tx /= len;
    ty /= len;

    double t = settings.strength / 100.0;
    if (t > 1.0)
        t = 1.0;

    double bx = ax + (tx - ax) * t;
    double by = ay + (ty - ay) * t;
    double blen = sqrt(bx * bx + by * by);
    if (blen < 1e-6)
        return aim;             // opposite directions cancelled out

    AimVector out;
    out.x = (int)(bx / blen * aim_len);
    out.y = (int)(by / blen * aim_len);
    return out;
}

bool update_aim_from_pad()
{
    Deadzone const &dz = deadzone_for_axis(2);   // the right stick axes
    float x = pad_state().axis_scaled(2, dz);    // SDL_GAMEPAD_AXIS_RIGHTX
    float y = pad_state().axis_scaled(3, dz);    // SDL_GAMEPAD_AXIS_RIGHTY

    if (x == 0.0f && y == 0.0f)
        return false;           // centred: the aim stays where it was

    g_aim = aim_offset_from_axes(x, y, aim_settings().radius);
    g_pad_aim = true;
    return true;
}

}
