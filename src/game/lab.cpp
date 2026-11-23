#include "game.h"

#ifdef LAB_BUILD
#include "lab/lab_api.h"
#include <cmath>

namespace server
{
    bool lab_local_client(int cn);
}

namespace game
{
    void applylook(physent *d, float yaw, float pitch);
}

#ifdef LAB_AIMBOT
extern "C" void lab_aimbot_update(void);
#endif

static lab_snapshot snapshot = {};
static const lab_snapshot empty_snapshot = {};

static bool lab_local_match()
{
    return haslocalclients() && !hasnonlocalclients() && !curpeer && !connpeer
        && game::player1 && server::lab_local_client(game::player1->clientnum)
        && client::isready && !client::demoplayback && !m_edit(game::gamemode)
        && !m_demo(game::gamemode) && gs_playing(game::gamestate) && game::maptime > 0;
}

static lab_vec3 lab_point(const vec &p)
{
    lab_vec3 result = {p.x, p.y, p.z};
    return result;
}

namespace game
{
    static void lab_build_snapshot()
    {
        snapshot = {};
        if(!lab_local_match()) return;

        snapshot.active = 1;
        snapshot.millis = lastmillis;
        snapshot.eye = lab_point(player1->o);
        snapshot.muzzle = lab_point(player1->muzzletag());
        snapshot.yaw = player1->yaw;
        snapshot.pitch = player1->pitch;
        snapshot.weapon = player1->weapselect;
        snapshot.fire_held = player1->action[AC_PRIMARY] || player1->action[AC_SECONDARY];

        loopv(players)
        {
            gameent *d = players[i];
            // Only bots can be targets in this lab.
            if(!d || d == player1 || d->actortype != A_BOT) continue;
            if(m_team(gamemode, mutators) && d->team == player1->team) continue;
            if(snapshot.enemy_count == LAB_MAX_ENEMIES) break;

            lab_enemy &enemy = snapshot.enemies[snapshot.enemy_count++];
            enemy.id = d->clientnum;
            enemy.team = d->team;
            enemy.alive = d->state == CS_ALIVE;
            vec head = d->headtag(), hit;
            enemy.head = lab_point(head);
            enemy.velocity = lab_point(vec(d->vel).add(d->falling));
            enemy.visible = enemy.alive && raycubelos(player1->o, head, hit);
        }
    }

    void lab_update_snapshot()
    {
        lab_build_snapshot();
#ifdef LAB_AIMBOT
        lab_aimbot_update();
#endif
    }
}

extern "C" const lab_snapshot *lab_get_snapshot(void)
{
    if(!lab_local_match()) return &empty_snapshot;
    return &snapshot;
}

extern "C" void lab_set_view(float yaw, float pitch)
{
    if(!lab_local_match() || !game::player1->isalive() || !game::allowmove(game::player1)) return;
    if(!std::isfinite(yaw) || !std::isfinite(pitch)) return;
    game::applylook(game::player1, yaw, pitch);
}

static void lab_dump()
{
    const lab_snapshot *s = lab_get_snapshot();
    conoutf(colourwhite, "lab active=%d millis=%u enemies=%u", s->active, s->millis, s->enemy_count);
    conoutf(colourwhite, "eye=(%g %g %g) muzzle=(%g %g %g) yaw=%g pitch=%g weapon=%d fire=%d",
        s->eye.x, s->eye.y, s->eye.z, s->muzzle.x, s->muzzle.y, s->muzzle.z,
        s->yaw, s->pitch, s->weapon, s->fire_held);
    for(uint32_t i = 0; i < s->enemy_count; ++i)
    {
        const lab_enemy &e = s->enemies[i];
        conoutf(colourwhite, "enemy id=%d team=%d alive=%d visible=%d head=(%g %g %g) velocity=(%g %g %g)",
            e.id, e.team, e.alive, e.visible, e.head.x, e.head.y, e.head.z,
            e.velocity.x, e.velocity.y, e.velocity.z);
    }
}
COMMAND(0, lab_dump, "");
#endif
