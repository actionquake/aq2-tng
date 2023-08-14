//-----------------------------------------------------------------------------
//
//  $Header:$

/*
 * $History:$
 * 
 */
/*
        botchat.c
*/

#include "../g_local.h"

#include "botchat.h"


/*
 *  In each of the following strings:
 *            - the first %s is the attacker/enemy which the string is
 *              directed to
 */

#define DBC_WELCOMES 13
char *ltk_welcomes[DBC_WELCOMES] =
{
        "Greetings all!",
        "Hello %s! Prepare to die!!",
		"%s? Who the hell is that?",
		"I say, %s, have you got a license for that face?",
		"%s, how do you see where you're going with those b*ll*cks in your eyes?",
		"Give your arse a rest, %s. try talking through your mouth",
		"Damn! I hoped Thresh would be here...",
		"Hey Mom, they gave me a gun!",
		"Hey, any of you guys played before?",
		"Hi %s, I wondered if you'd show your face around here again.",
		"Nice :-) :-) :-)",
		"OK %s, let's get this over with"
		"SUG BALLE SUG BALLE SUG BALLE"
};

#define DBC_KILLEDS 13
char *ltk_killeds[DBC_KILLEDS] =
{
        "B*stard! %s messed up my hair.",
        "All right, %s. Now I'm feeling put out!",
		"Why you little!",
		"Ooooh, %s, that smarts!",
		"%s's mother cooks socks in hell!",
		"Hey, %s, how about a match - your face and my arse!",
		"Was %s talking to me, or chewing a brick?",
		"Aw, %s doesn't like me...",
		"It's clobberin' time, %s",
		"Hey, I was tying my shoelace!",
		"Oh - now I know how strawberry jam feels...",
		"laaaaaaaaaaaaaaag!",
		"One feels like chicken tonight...."
};

#define DBC_INSULTS 16
char *ltk_insults[DBC_INSULTS] =
{
        "Hey, %s. Your mother was a hamster...!",
        "%s; Eat my dust!",
        "Hahaha! Hook, Line and Sinker, %s!",
		"I'm sorry, %s, did I break your concentration?",
		"Unlike certain other bots, %s, I can kill with an English accent..",
		"Get used to disappointment, %s",
		"You couldn't organise a p*ss-up in a brewery, %s",
		"%s, does your mother know you're out?",
		"Hey, %s, one guy wins, the other prick loses...",
		"Oh %s, ever thought of taking up croquet instead?",
		"Yuck! I've got some %s on my shoe!",
		"Mmmmm... %s chunks!",
		"Hey everyone, %s was better than the Pirates of Penzance",
		"Oh - good play %s ... hehehe",
		"Errm, %s, have you ever thought of taking up croquet instead?",
		"Ooooooh - I'm sooooo scared %s"
};



#define AQTION_KILLDS 8
char *aqtion_killeds[AQTION_KILLDS] =
{
		"SUG BALLE SUG BALLE"
        "hahahaha",
		"lol",
		"Try using a real gun, %s!",
		"rofl",
		"you are getting better, I think",
		"nice shot",
		"scared me half to death, %s"
};

#define AQTION_INSULTS 8
char *aqtion_insults[AQTION_INSULTS] =
{
		"Check out the Discord server!  -->  https://discord.aq2world.com   <--",
        "Have you visited the forums recently?  -->  https://forums.aq2world.com  <--",
        "no achievement for u!",
		"%s is probably still using a ball mouse lol",
		"well that was unforunate",
		"gottem",
		"peek a boooo!",
		"meh!"
};


void LTK_Chat (edict_t *bot, edict_t *object, int speech)
{
        char final[150];
        char *text = NULL;

        if ((!object) || (!object->client)) {
			return;
		}

		if(!ltk_classic->value){
			if (speech == DBC_WELCOME) {
					text = ltk_welcomes[rand()%DBC_WELCOMES];
			} else if (speech == DBC_KILLED) {
					text = ltk_killeds[rand()%DBC_KILLEDS];
			} else if (speech == DBC_INSULT) {
					text = ltk_insults[rand()%DBC_INSULTS];
			} else {
					if( debug_mode )
							gi.bprintf (PRINT_HIGH, "LTK_Chat: Unknown speech type attempted!(out of range)");
					return;
			}
		} else {
			if (speech == DBC_WELCOME)
				return;
			else if (speech == DBC_KILLED)
					text = aqtion_killeds[rand()%AQTION_KILLDS];
			else if (speech == DBC_INSULT)
					text = aqtion_insults[rand()%AQTION_INSULTS];
			else
			{
					if( debug_mode )
							gi.bprintf (PRINT_HIGH, "LTK_Chat: Unknown speech type attempted!(out of range)");
					return;
			}
		}

        sprintf (final, text, object->client->pers.netname);

        LTK_Say (bot, final);
}


/*
==================
Bot_Say
==================
*/
void LTK_Say (edict_t *ent, char *what)
{
	int		j;
	edict_t	*other;
	char	text[2048];

        Com_sprintf (text, sizeof(text), "%s: ", ent->client->pers.netname);

        if (*what == '"')
        {
                what++;
                what[strlen(what)-1] = 0;
        }
        strcat(text, what);

	// don't let text be too long for malicious reasons
        if (strlen(text) > 200)
                text[200] = '\0';

	strcat(text, "\n");

	if (dedicated->value)
		gi.cprintf(NULL, PRINT_CHAT, "%s", text);

	for (j = 1; j <= game.maxclients; j++)
	{
		other = &g_edicts[j];
		if (!other->inuse)
			continue;
		if (!other->client)
			continue;
        if (Q_stricmp(other->classname, "bot") == 0)
            continue;
		gi.cprintf(other, PRINT_CHAT, "%s", text);
	}
}

