/*
 * behavior_selector.h
 *
 *  Created on: 28/09/2012
 *      Author: romulo
 */

#ifndef BEHAVIOR_SELECTOR_H_
#define BEHAVIOR_SELECTOR_H_

#include <carmen/carmen.h>
#include <carmen/rddf_messages.h>
#include <carmen/frenet_path_planner_messages.h>
#include <carmen/obstacle_distance_mapper_interface.h>
#include "SampleFilter.h"

enum
{
	NONE,
	OBSTACLE,
	MOVING_OBSTACLE,
	CENTRIPETAL_ACCELERATION,
	ANNOTATION,
	KEEP_SPEED_LIMIT,
	PARKING_MANOUVER,
	WAIT_SWITCH_VELOCITY_SIGNAL,
	INTERMEDIATE_VELOCITY,
	STOP_AT_FINAL_GOAL
};

typedef struct
{
	int valid;
	int path_id;
	double s_distance_without_collision_with_moving_object;	// Distance in seconds. This is related to a path.
	int possible_collision_mo_pose_index;					// Future position in the path of a possibly colliding moving object
	double possible_collision_mo_sv;						// Velocity allong the path of the possibly colliding moving object
	double possible_collision_mo_dv;						// Velocity orthogonal to the path of the possibly colliding moving object
	double s_distance_to_moving_object_in_parallel_lane;	// Distance in seconds. This is related to a lane defined by the d distance of farthest point of a path
	int possible_collision_mo_in_parallel_lane_pose_index;	// Future position in the parallel path (the path not starting at the robot pose, but always at the same final d distance from the main path) of a possibly colliding moving object
	double possible_collision_mo_in_parallel_lane_sv;		// Velocity allong the parallel path of the possibly colliding moving object
	double possible_collision_mo_in_parallel_lane_dv;		// Velocity orthogonal to the parallel path of the possibly colliding moving object
	double s_distance_without_collision_with_static_object;	// Distance in seconds. This is related to a path.
	int static_object_pose_index;							// Future position in the path of a colliding static object
	bool mo_in_front;
	bool path_has_no_collision;
} path_collision_info_t;

typedef struct
{
	carmen_point_t pose;
	double s;
	double sv;
	double d;
	double dv;
} moving_object_pose_info_t;


#ifdef __cplusplus
extern "C" {
#endif

	void change_distance_between_waypoints_and_goals(double dist_between_waypoints, double change_goal_dist);

	void behavior_selector_initialize(carmen_robot_ackerman_config_t config, double dist_between_waypoints,
			double change_goal_dist, carmen_behavior_selector_algorithm_t f_planner, carmen_behavior_selector_algorithm_t p_planner, double max_velocity_reverse);

	void behavior_selector_update_robot_pose(carmen_ackerman_traj_point_t robot_pose);

	void behavior_selector_update_rddf(carmen_rddf_road_profile_message *rddf_msg, int rddf_num_poses_by_velocity, double timestamp);

	void behavior_selector_update_map(carmen_obstacle_distance_mapper_map_message *map);

	void behavior_selector_publish_periodic_messages();

	void behavior_selector_set_algorithm(carmen_behavior_selector_algorithm_t algorithm, carmen_behavior_selector_state_t state);

	int behavior_selector_set_state(carmen_behavior_selector_state_t state);

	int behavior_selector_set_goal_source(carmen_behavior_selector_goal_source_t goal_source);

	void behavior_selector_add_goal(carmen_point_t goal);

	void behavior_selector_clear_goal_list();

	void behavior_selector_remove_goal();

	void behavior_selector_update_annotations(carmen_rddf_annotation_message *message);

	carmen_behavior_selector_algorithm_t get_current_algorithm();

	void behavior_selector_get_full_state(carmen_behavior_selector_state_t *current_state_out, carmen_behavior_selector_algorithm_t *following_lane_planner_out,
			carmen_behavior_selector_algorithm_t *parking_planner_out, carmen_behavior_selector_goal_source_t *current_goal_source_out);

	int behavior_selector_get_state();

	carmen_ackerman_traj_point_t *behavior_selector_get_goal_list(int *goal_list_size_out);
	int *behavior_selector_get_goal_type();

	carmen_ackerman_traj_point_t get_robot_pose();
	double get_max_v_reverse();
	void set_max_v_reverse(double v);
	double get_max_v();
	void set_max_v(double v);
	carmen_robot_ackerman_config_t *get_robot_config();

	carmen_rddf_road_profile_message *get_last_rddf_message();

	carmen_ackerman_traj_point_t *set_goal_list(int &goal_list_size, carmen_ackerman_traj_point_t *&first_goal, int &first_goal_type,
			carmen_rddf_road_profile_message *rddf, path_collision_info_t path_collision_info,
			carmen_moving_objects_point_clouds_message *current_moving_objects, double timestamp);

	double distance_between_waypoints_and_goals();
	bool red_traffic_light_ahead(carmen_ackerman_traj_point_t current_robot_pose_v_and_phi, double timestamp);
	bool busy_pedestrian_track_ahead(carmen_ackerman_traj_point_t current_robot_pose_v_and_phi, double timestamp);
	bool stop_sign_ahead(carmen_ackerman_traj_point_t current_robot_pose_v_and_phi);

	void publish_dynamic_annotation(carmen_vector_3D_t annotation_point, double orientation, char *annotation_description,
			int annotation_type, int annotation_code, double timestamp);

	void publish_new_best_path(int best_path, double timestamp);

	int set_goal_velocity(carmen_ackerman_traj_point_t *goal, carmen_ackerman_traj_point_t *current_robot_pose_v_and_phi,
			int goal_type, carmen_rddf_road_profile_message *rddf, path_collision_info_t path_collision_info, double timestamp);

	carmen_moving_objects_point_clouds_message *behavior_selector_moving_objects_tracking(carmen_frenet_path_planner_set_of_paths *set_of_paths,
			carmen_obstacle_distance_mapper_map_message *distance_map);

	int run_decision_making_state_machine(carmen_behavior_selector_state_message *decision_making_state_msg,
			carmen_ackerman_traj_point_t current_robot_pose_v_and_phi, carmen_ackerman_traj_point_t *goal, int goal_type,
			double timestamp);

	carmen_annotation_t *get_nearest_velocity_related_annotation(carmen_rddf_annotation_message annotation_message,
			carmen_ackerman_traj_point_t *current_robot_pose_v_and_phi, bool wait_start_moving);

	double get_distance_to_act_on_annotation(double v0, double va, double distance_to_annotation);

	carmen_ackerman_traj_point_t displace_pose(carmen_ackerman_traj_point_t robot_pose, double displacement);

	/**
	 * @brief Report whether the first goal is a moving obstacle.
	 */
	bool is_moving_obstacle_ahead();

	double datmo_speed_front();
	double datmo_get_moving_obstacle_distance(carmen_ackerman_traj_point_t robot_pose,
			carmen_robot_ackerman_config_t *robot_config);

#ifdef __cplusplus
}
#endif

#endif /* BEHAVIOR_SELECTOR_H_ */
