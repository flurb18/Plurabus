#ifndef CONSTANTS_H
#define CONSTANTS_H

const int GAME_TIME_SECONDS = 300;
const int STARTUP_TIME_SECONDS = 3;
const int MENU_ITEMS_IN_VIEW = 6;
const int SPAWNER_SIZE = 8;
const int SPAWNER_PADDING = 10;
const int SPAWNER_UPDATE_TIME = 1;
const int STARTUP_FONT_SIZE = 32;
const int FONT_SIZE = 16;
const int PANEL_PADDING = 10;
const int MAX_WALL_HEALTH = 10;
const int STARTING_WALL_HEALTH = 10;
const int MAX_DOOR_HEALTH = 20;
const double MAX_SCALE = 18.0;
const int SUBMENU_HORIZONTAL_PADDING = 10;
const int MAX_TOWERS = 3;
const int TOWER_UPDATE_TIME = 4;
const int MAX_TOWER_HEALTH = 150;
const int TOWER_SIZE = 4;
const int TOWER_AOE_RADIUS_SQUARED = 400;
const int MAX_SUBSPAWNERS = 3;
const int SUBSPAWNER_SIZE = 5; //should be odd
const int SUBSPAWNER_UNIT_COST = 10;
const int SUBSPAWNER_UPDATE_TIME = 10;
const int ZAP_CLEAR_TIME = 6;
const int ZAP_EFFECTS_SUBDIVISION = 50;
const int ZAP_CENTER_EFFECTS_NUM = 7;
const int INIT_EVENT_BUFFER_SIZE = 16;
const int MAX_BOMBS = 1;
const int MAX_BOMB_HEALTH = 250;
const int BOMB_AOE_RADIUS = 20;
const int BOMB_SIZE = 5;
const int BOMB_CLEAR_TIME = 4;
const int FRAME_DELAY = 4; // ms
extern const char *TITLE;

#endif
