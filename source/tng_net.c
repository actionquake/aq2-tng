#include "g_local.h"

void announce_server_populating()
{
    char template[] = "{ \"title\": \"Join %s\", \"description\": \"%s\", \"url\":\"%s\" }";
    char content[512];
    char title[] = "My Server";
    char description[] = "This is my server.";
    char url[] = "http://myserver.com";

    Com_sprintf(content, sizeof(content), template, title, description, url);

    lc_discord_webhook(content);
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