#include <prob_map.h>
#include "obstacle_distance_mapper_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

//Subscribers
void carmen_obstacle_distance_mapper_subscribe_message(carmen_obstacle_distance_mapper_map_message *message,
		carmen_handler_t handler, carmen_subscribe_t subscribe_how);

void carmen_obstacle_distance_mapper_subscribe_multi_height_map_message(carmen_obstacle_distance_mapper_map_message *message,
		carmen_handler_t handler, carmen_subscribe_t subscribe_how, int height_level);

void carmen_obstacle_distance_mapper_subscribe_compact_map_message(carmen_obstacle_distance_mapper_compact_map_message *message,
		carmen_handler_t handler, carmen_subscribe_t subscribe_how);

void carmen_obstacle_distance_mapper_subscribe_multi_height_compact_map_message(carmen_obstacle_distance_mapper_compact_map_message *message,
		carmen_handler_t handler, carmen_subscribe_t subscribe_how, int height_level);

void carmen_obstacle_distance_mapper_subscribe_compact_lane_contents_message(carmen_obstacle_distance_mapper_compact_map_message *message,
		carmen_handler_t handler, carmen_subscribe_t subscribe_how);

void carmen_behaviour_selector_subscribe_compact_lane_contents_message(carmen_obstacle_distance_mapper_compact_map_message *message,
		carmen_handler_t handler, carmen_subscribe_t subscribe_how);

//Publishers
void carmen_obstacle_distance_mapper_publish_distance_map_message(carmen_prob_models_distance_map *distance_map,
		double timestamp);

void carmen_obstacle_distance_mapper_publish_multi_height_distance_map_message(carmen_prob_models_distance_map *distance_map,
		double timestamp, int height_level);

void carmen_obstacle_distance_mapper_publish_compact_distance_map_message(carmen_obstacle_distance_mapper_compact_map_message *message,
		double timestamp);

void carmen_obstacle_distance_mapper_publish_multi_height_compact_distance_map_message(carmen_obstacle_distance_mapper_compact_map_message *message,
		double timestamp, int height_level);

void carmen_obstacle_distance_mapper_publish_compact_lane_contents_message(carmen_obstacle_distance_mapper_compact_map_message *message,
		double timestamp);

void carmen_behaviour_selector_publish_compact_lane_contents_message(carmen_obstacle_distance_mapper_compact_map_message *message, double timestamp);


//Others
void carmen_obstacle_distance_mapper_create_new_map(carmen_obstacle_distance_mapper_map_message *distance_map,
		carmen_map_config_t config, char *host, double timestamp);

cell_coords_t carmen_obstacle_distance_mapper_get_map_cell_from_configs(carmen_map_config_t distance_map_config, carmen_map_config_t compact_lane_map_config,
		short int x, short int y);

void carmen_obstacle_distance_mapper_cpy_compact_map_message_to_compact_map(carmen_obstacle_distance_mapper_compact_map_message *compact_map,
		carmen_obstacle_distance_mapper_compact_map_message *message);

void carmen_obstacle_distance_mapper_clear_distance_map_using_compact_map(carmen_prob_models_distance_map *map,
		carmen_obstacle_distance_mapper_compact_map_message *cmap, int value);

void carmen_obstacle_distance_mapper_clear_distance_map_message_using_compact_map(carmen_obstacle_distance_mapper_map_message *map,
		carmen_obstacle_distance_mapper_compact_map_message *cmap, int value);

void carmen_obstacle_distance_mapper_uncompress_compact_distance_map(carmen_prob_models_distance_map *map,
		carmen_obstacle_distance_mapper_compact_map_message *cmap);

void carmen_obstacle_distance_mapper_uncompress_compact_distance_map_message(carmen_obstacle_distance_mapper_map_message *map,
		carmen_obstacle_distance_mapper_compact_map_message *cmap);

void carmen_obstacle_distance_mapper_overwrite_distance_map_message_with_compact_distance_map(carmen_obstacle_distance_mapper_map_message *map,
		carmen_obstacle_distance_mapper_compact_map_message *cmap);

void carmen_obstacle_distance_mapper_overwrite_distance_map_with_compact_distance_map(carmen_prob_models_distance_map *map,
		carmen_obstacle_distance_mapper_compact_map_message *cmap);

void carmen_obstacle_distance_mapper_create_compact_distance_map(carmen_obstacle_distance_mapper_compact_map_message *cmap,
		carmen_prob_models_distance_map *map, int value);

void carmen_obstacle_distance_mapper_free_compact_distance_map(carmen_obstacle_distance_mapper_compact_map_message *map);

void carmen_obstacle_distance_mapper_create_distance_map_from_distance_map_message(carmen_prob_models_distance_map *map,
		carmen_obstacle_distance_mapper_map_message *message);

void carmen_obstacle_distance_mapper_free_distance_map(carmen_prob_models_distance_map *map);


#ifdef __cplusplus
}
#endif


// @}

