#ifndef __GTKDECK_COUNTDOWN_H__
#define __GTKDECK_COUNTDOWN_H__

#include "../deck_plugin.h"

void countdown_config(DeckPlugin *self, GtkBox *parent);
void countdown_exec(DeckPlugin *self);
void countdown_save(DeckPlugin *self, char *group, GKeyFile *key_file);
void countdown_load(DeckPlugin *self, char *group, GKeyFile *key_file);

#endif
