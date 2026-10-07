#ifndef FC__CONTROL_H
#define FC__CONTROL_H

void key_move_north(void);
void key_move_south(void);
void key_move_east(void);
void key_move_west(void);
void key_end_turn(void);
void key_cancel_action(void);
void request_toggle_map_grid(void);
void do_map_click(int x, int y);

#endif
