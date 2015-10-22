#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "house.h"
#include "constants.h"
#include "dg_scripts.h"

#define MAX_MAP 72
#define MAX_MAP_DIR 6
#define MAX_MAP_FOLLOW 4

#define SECT_EMPTY	20 /* anything greater than num sect types */
#define SECT_STRANGE	(SECT_EMPTY + 1)
#define SECT_HERE	(SECT_STRANGE + 1)

#define DOOR_NS   -1
#define DOOR_EW   -2
#define DOOR_UP   -3
#define DOOR_DOWN -4
#define VDOOR_NS  -5
#define VDOOR_EW  -6
#define NUM_DOOR_TYPES 6

char *strfrmt(char *str, int w, int h, int justify, int hpad, int vpad);
char *strpaste(char *str1, char *str2, char *joiner);

extern struct room_data *world;
int add_to_save_list(zone_vnum zone, int type);
int can_edit_zone(struct char_data *ch, int number);

struct map_info_type
{
  int  sector_type;
  char *disp;
};

struct map_info_type door_info[] =
{
  { VDOOR_EW,  " `m+`n " },
  { VDOOR_NS,  " `m+`n "},
  { DOOR_DOWN, "  `r_`n" },
  { DOOR_UP,   "`r+`n  " },
  { DOOR_EW,   " `b-`n " },
  { DOOR_NS,   " `b|`n " }, 
};

struct map_info_type map_info[] =
{
  { SECT_INSIDE,       "[ ]"     }, /* 0 */
  { SECT_CITY,         "[C]"     },
  { SECT_FIELD,        "[`y.`n]" },
  { SECT_FOREST,       "[`g@`n]" },
  { SECT_HILLS,        "[`Mm`n]" },
  { SECT_MOUNTAIN,     "[`rM`n]" }, /* 5 */
  { SECT_WATER_SWIM,   "[`c~`n]" },
  { SECT_WATER_NOSWIM, "[`b#`n]" },
  { SECT_FLYING,       "[`C^`n]" },
  { SECT_UNDERWATER,   "[`bU`n]" },
  { SECT_SEA,          "[`BO`n]" }, /* 10 */
  { SECT_SWAMP,        "[`mV`n]" },
  { SECT_SAND,         "[`Y*`n]" },
  { -1,                ""        },
  { -1,                ""        },
  { -1,                ""        }, /* 15 */
  { -1,                ""        },
  { -1,                ""        },
  { -1,                ""        },
  { -1,                ""        },
  { SECT_EMPTY,        "   "     }, /* 20 */
  { SECT_STRANGE,      "[`R?`n]" },
  { SECT_HERE,         "[!]"     },
};


int map[MAX_MAP][MAX_MAP];
int offsets[4][2] ={ {-2, 0},{ 0, 2},{ 2, 0},{ 0, -2} };
int door_offsets[6][2] ={ {-1, 0},{ 0, 1},{ 1, 0},{ 0, -1},{ -1, 1},{ -1, -1} }; 
int door_marks[6] = { DOOR_NS, DOOR_EW, DOOR_NS, DOOR_EW, DOOR_UP, DOOR_DOWN };
int vdoor_marks[4] = { VDOOR_NS, VDOOR_EW, VDOOR_NS, VDOOR_EW };

/* Heavily modified - Edward */
void MapArea(room_rnum room, struct char_data *ch, int x, int y, int min, int max, sh_int xpos, sh_int ypos)
{
  room_rnum prospect_room;
  struct room_direction_data *pexit;
  int door;
  sh_int prospect_xpos, prospect_ypos;

  if (map[x][y] < 0)
    return; /* this is a door */

  /* marks the room as visited */
  if(room == IN_ROOM(ch))
    map[x][y] = SECT_HERE;
  else
    map[x][y] = world[room].sector_type;

  if ( (x < min) || ( y < min) || ( x > max ) || ( y > max) ) return;

  /* Check for exits */
  for ( door = 0; door < MAX_MAP_DIR; door++ ) {

    if( door < MAX_MAP_FOLLOW &&
        xpos+door_offsets[door][0] >= 0 &&
        xpos+door_offsets[door][0] <= world[room].ns_size &&
        ypos+door_offsets[door][1] >= 0 &&
        ypos+door_offsets[door][1] <= world[room].ew_size)
    { /* Virtual exit */

      map[x+door_offsets[door][0]][y+door_offsets[door][1]] = vdoor_marks[door] ;
      if (map[x+offsets[door][0]][y+offsets[door][1]] == SECT_EMPTY )
        MapArea(room,ch,x + offsets[door][0], y + offsets[door][1], min, max, xpos+door_offsets[door][0], ypos+door_offsets[door][1]);
      continue;
    }

    if ( (pexit = world[room].dir_option[door]) > 0  &&   
         (pexit->to_room > 0 ) &&
         (!IS_SET(pexit->exit_info, EX_CLOSED))) { /* A real exit */

      /* But is the door here... */
      switch (door) {
      case NORTH:
        if(xpos > 0 || ypos!=pexit->position) continue;
	break;
      case SOUTH:
        if(xpos < world[room].ns_size || ypos!=pexit->position) continue;
	break;
      case EAST:
        if(ypos < world[room].ew_size || xpos!=pexit->position) continue;
	break;
      case WEST:
        if(ypos > 0 || xpos!=pexit->position) continue;
	break;
      }


 /*     if ( (x < min) || ( y < min) || ( x > max ) || ( y > max) ) return;*/
      prospect_room = pexit->to_room;
         
      /* one way into area OR maze */	
      if ( world[prospect_room].dir_option[rev_dir[door]] &&
           world[prospect_room].dir_option[rev_dir[door]]->to_room != room) { 
        map[x][y] = SECT_STRANGE;
	return;
      }

      map[x+door_offsets[door][0]][y+door_offsets[door][1]] = door_marks[door] ;

      prospect_xpos = prospect_ypos = 0;
      switch (door) {
      case NORTH:
        prospect_xpos = world[prospect_room].ns_size;
      case SOUTH:
        prospect_ypos = world[prospect_room].dir_option[rev_dir[door]] ? world[prospect_room].dir_option[rev_dir[door]]->position : world[prospect_room].ew_size/2;
      break;
      case WEST:
        prospect_ypos = world[prospect_room].ew_size;
      case EAST:
        prospect_xpos = world[prospect_room].dir_option[rev_dir[door]] ? world[prospect_room].dir_option[rev_dir[door]]->position : world[prospect_room].ns_size/2;
      }

      if ( door < MAX_MAP_FOLLOW && map[x+offsets[door][0]][y+offsets[door][1]] == SECT_EMPTY ) {
        MapArea(pexit->to_room,ch,x + offsets[door][0], y + offsets[door][1], min, max, prospect_xpos, prospect_ypos);
      }
    } /* end if exit there */
  }
  return;
}

/* Returns a string representation of the map */
char *StringMap(int min, int max)
{
  static char strmap[MAX_MAP*MAX_MAP*7 + MAX_MAP*2 + 1];
  char *mp = strmap;
  int x, y;

  /* every row */
  for (x = min; x <= max; x++) { 
    /* every column */
    for (y = min; y <= max; y++) { 
      strcpy(mp, (map[x][y]<0) ? \
	      door_info[NUM_DOOR_TYPES + map[x][y]].disp : \
	      map_info[map[x][y]].disp);
      mp += strlen((map[x][y]<0) ? \
	      door_info[NUM_DOOR_TYPES + map[x][y]].disp : \
	      map_info[map[x][y]].disp);
    }
    strcpy(mp, "\r\n");
    mp+=2;
  }
  *mp='\0';
  return strmap;
}

/* Display a nicely formatted map with a legend */
void perform_map( struct char_data *ch, char *argument )
{
  int size, centre, x, y, min, max;
  char *strtmp;
  char arg1[10];
  one_argument( argument, arg1 );
  size = atoi(arg1);
  size = URANGE(13,size,MAX_MAP); /* Should be an odd number */
  
  centre = MAX_MAP/2;

  min = centre - size/2;
  max = centre + size/2;

  for (x = 0; x < MAX_MAP; ++x)
      for (y = 0; y < MAX_MAP; ++y)
           map[x][y]=SECT_EMPTY;

  /* starts the mapping with the centre room */
  MapArea(IN_ROOM(ch), ch, centre, centre, min, max, world[IN_ROOM(ch)].ns_size/2, world[IN_ROOM(ch)].ew_size/2); 

  /* marks the center, where ch is */
  map[centre][centre] = SECT_HERE;

  send_to_char( "\r\n", ch);
  send_to_char( "[ Mapping System ]________________________________________________ \r\n", 
ch );
  send_to_char( "| Legend :  [!]You   [ ]Inside  [C]City  [`y.`n]Field       [`g@`n]Forest |\r\n", ch );
  send_to_char( "|   Up   `r+`n  [`Y*`n]Sand  [`mV`n]Swamp   [`Mm`n]Hill  [`rM`n]Mountain              |\r\n", ch );
  send_to_char( "|   Down `r_`n  [`c~`n]Swim  [`b#`n]Boat    [`BO`n]Sea   [`bU`n]Underwater  [`C^`n]Flying |\r\n", ch );
  send_to_char( "|-----------------------------------------------------------------|\r\n", ch );

  strtmp = strdup(strpaste("", StringMap(min, max), "|             "));
  send_to_char(strpaste(strtmp, "", "             |"), ch);
  free(strtmp);

  send_to_char( "|_________________________________________________________________|\r\n", ch );
  return;
}

/* Display a string with the map beside it */
void str_and_map(char *str, struct char_data *ch ) {
  int size, centre, x, y, min, max;

  size = 5;
  centre = MAX_MAP/2;
  min = centre - size/2;
  max = centre + size/2;

  for (x = 0; x < MAX_MAP; ++x)
      for (y = 0; y < MAX_MAP; ++y)
           map[x][y]=SECT_EMPTY;

  /* starts the mapping with the center room */
  MapArea(IN_ROOM(ch), ch, centre, centre, min, max, world[IN_ROOM(ch)].ns_size/2, world[IN_ROOM(ch)].ew_size/2); 
  map[centre][centre] = SECT_HERE;

  send_to_char(strpaste(strfrmt(str, 61, 5, 0, 1, 1), StringMap(min, max), " | "), ch);
}

ACMD(do_map) {
    perform_map(ch, " ");
}


