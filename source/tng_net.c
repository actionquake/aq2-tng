#include "g_local.h"
#include <jansson.h>

void announce_server_populating()
{
    json_t *srv_announce = json_object();
    json_object_set_new(srv_announce, "hostname", json_string(hostname->string));
    json_object_set_new(srv_announce, "server_ip", json_string(server_ip->string));
    json_object_set_new(srv_announce, "server_port", json_integer(server_port->value));
    json_object_set_new(srv_announce, "player_count", json_integer(CountRealPlayers()));
    json_object_set_new(srv_announce, "maxclients", json_integer(maxclients->value));
    json_object_set_new(srv_announce, "mapname", json_string(level.mapname));

    json_t *root = json_object();
    json_object_set_new(root, "srv_announce", srv_announce);
    json_object_set_new(root, "webhook_url", json_string(sv_curl_discord_server_url->string));

    char *message = json_dumps(root, 0); // 0 is the flags parameter, you can set it to JSON_INDENT(4) for pretty printing

    lc_server_announce("/srv_announce_filling", message);

    free(message);
    json_decref(root);
    game.srv_announce_timeout = level.framenum;
}

qboolean ready_to_announce()
{
    if (level.framenum % 100 == 0 && level.lc_recently_sent)
        return true;
}

// qboolean announce_to_discord()
// {
//     if (ready_to_announce()) {
//         announce_server_populating();
//         level.lc_recently_sent = true;
//         return true;
//     }
// }

// qboolean evaluate_discord_announcement()
// {
//     int i;

//     for (i = NOTIFY_NONE; i < NOTIFY_MAX; i++) {
//         if (level.lc_recently_sent[i] 
//         }
        
//     }
// }