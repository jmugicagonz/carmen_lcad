#include "path_planner_astar.h"

#define THETA_SIZE 1
#define ASTAR_GRID_RESOLUTION 1.0

#define ACKERMAN_EXPANSION 1

#define MAX_VIRTUAL_LASER_SAMPLES 100000

#define DIST2D_D_P(x1,x2) (sqrt(((x1).x - (x2)->x) * ((x1).x - (x2)->x) + \
							((x1).y - (x2)->y) * ((x1).y - (x2)->y)))
carmen_mapper_virtual_laser_message virtual_laser_message;

carmen_point_t *final_goal = NULL;
int goal_received = 0;
int path_sended = 0;
int contador = 0;
int poses_size = 0;
carmen_ackerman_traj_point_t *carmen_rddf_poses_from_path;
std::vector<carmen_ackerman_traj_point_t> temp_rddf_poses_from_path;
carmen_localize_ackerman_globalpos_message current_globalpos_msg;
carmen_robot_ackerman_config_t robot_config;
carmen_obstacle_distance_mapper_map_message distance_map;
state_node_p ***astar_map;

using namespace boost::heap;
using namespace std;


void
draw_astar_object(carmen_ackerman_traj_point_t *object_pose, int color)
{
	/*

	  CARMEN_RED = 0,
	  CARMEN_BLUE = 1,
	  CARMEN_WHITE = 2,
	  CARMEN_YELLOW = 3,
	  CARMEN_GREEN = 4,
	  CARMEN_LIGHT_BLUE = 5,
	  CARMEN_BLACK = 6,
	  CARMEN_ORANGE = 7,
	  CARMEN_GREY = 8,
	  CARMEN_LIGHT_GREY = 9,
	  CARMEN_PURPLE = 10,

	 */
	virtual_laser_message.positions[virtual_laser_message.num_positions].x = object_pose->x;
	virtual_laser_message.positions[virtual_laser_message.num_positions].y = object_pose->y;
	virtual_laser_message.colors[virtual_laser_message.num_positions] = color;
	virtual_laser_message.num_positions++;
}

std::vector<state_node>
build_state_path(state_node *node)
{
	std::vector<state_node> state_path;
	int i = 0;

    while (node != NULL)
    {
        state_path.push_back(*node);
        node = node->parent;
        i++;
    }

    return(state_path);
}


std::vector<carmen_ackerman_traj_point_t>
build_rddf_poses( state_node *current_state, carmen_obstacle_distance_mapper_map_message *distance_map)
{
	std::vector<state_node> path;
	path = build_state_path(current_state->parent);
	std::reverse(path.begin(), path.end());
	std::vector<carmen_ackerman_traj_point_t> temp_rddf_poses_from_path;

	for (int i = 0; i < path.size(); i++)
	{
		temp_rddf_poses_from_path.push_back(path[i].state);
		printf("[build_rddf_poses] %lf %lf %lf %lf %lf\n", path[i].state.x, path[i].state.y, path[i].state.theta, path[i].state.v, path[i].state.phi);
		draw_astar_object(&path[i].state, CARMEN_GREEN);
	}

	return temp_rddf_poses_from_path;
}

void
calculate_phi_ahead(carmen_ackerman_traj_point_t *path, int num_poses)
{
	double L = robot_config.distance_between_front_and_rear_axles;

	for (int i = 0; i < (num_poses - 1); i++)
	{
		double delta_theta = carmen_normalize_theta(path[i + 1].theta - path[i].theta);
		double l = DIST2D(path[i], path[i + 1]);
		if (l < 0.01)
		{
			path[i].phi = 0.0;
			continue;
		}
		path[i].phi = atan(L * (delta_theta / l));
	}

	for (int i = 1; i < (num_poses - 1); i++)
	{
		path[i].phi = (path[i].phi + path[i - 1].phi + path[i + 1].phi) / 3.0;
	}
}


void
calculate_phi_back(carmen_ackerman_traj_point_t *path, int num_poses)
{
	double L = robot_config.distance_between_front_and_rear_axles;

	for (int i = (num_poses - 1); i > 0; i--)
	{
		double delta_theta = carmen_normalize_theta(path[i - 1].theta - path[i].theta);
		double l = DIST2D(path[i], path[i - 1]);
		if (l < 0.01)
		{
			path[i].phi = 0.0;
			continue;
		}
		path[i].phi = atan(L * (delta_theta / l));
	}

	for (int i = (num_poses - 2); i > 0; i--)
	{
		path[i].phi = (path[i].phi + path[i - 1].phi + path[i + 1].phi) / 3.0;
	}
}


void
compute_theta(carmen_ackerman_traj_point_t *path, int num_poses)
{
	for (int i = 0; i < (num_poses - 1); i++)
		path[i].theta = atan2(path[i + 1].y - path[i].y, path[i + 1].x - path[i].x);
	if (num_poses > 1)
		path[num_poses - 1].theta = path[num_poses - 2].theta;


}


void
calculate_theta_back(carmen_ackerman_traj_point_t *path, int num_poses)
{
	for (int i = 1; i < num_poses; i++)
//		path[i].theta = atan2(path[i - 1].y - path[i].y, path[i - 1].x - path[i].x);
		path[i].theta = carmen_normalize_theta(atan2(path[i].y - path[i - 1].y, path[i].x - path[i - 1].x) + M_PI);
}


void
calculate_theta_and_phi(carmen_ackerman_traj_point_t *poses_ahead, int num_poses_ahead,
		carmen_ackerman_traj_point_t *poses_back, int num_poses_back)
{
	compute_theta(poses_ahead, num_poses_ahead);
	poses_back[0].theta = poses_ahead[0].theta;
	calculate_theta_back(poses_back, num_poses_back);

	calculate_phi_ahead(poses_ahead, num_poses_ahead);
	poses_back[0].phi = poses_ahead[0].phi;
	calculate_phi_back(poses_back, num_poses_back);
}


/* GSL - GNU Scientific Library
 * Multidimensional Minimization
 * https://www.gnu.org/software/gsl/doc/html/multimin.html
 *
 * Sebastian Thrun
 */


//Function to be minimized summation[x(i+1)-2x(i)+x(i-1)]
double
my_f(const gsl_vector *v, void *params)
{
	list<carmen_ackerman_traj_point_t> *p = (list<carmen_ackerman_traj_point_t> *) params;
	int i, j, size = (p->size() - 2);           //we have to discount the first and last point that wont be optimized
	double a = 0.0, b = 0.0, sum = 0.0;

	double x_prev = p->front().x;				//x(i-1)
	double x      = gsl_vector_get(v, 0);		//x(i)
	double x_next = gsl_vector_get(v, 1);		//x(i+1)

	double y_prev = p->front().y;
	double y      = gsl_vector_get(v, size);
	double y_next = gsl_vector_get(v, size+1);

	for (i = 2, j = (size+2); i < size; i++, j++)
	{
		a = x_next - (2*x) + x_prev;
		b = y_next - (2*y) + y_prev;
		sum += (a*a + b*b);

		x_prev = x;
		x      = x_next;
		x_next = gsl_vector_get(v, i);

		y_prev = y;
		y      = y_next;
		y_next = gsl_vector_get(v, j);
	}

	x_prev = x;
	x      = x_next;
	x_next = p->back().x;

	y_prev = y;
	y      = y_next;
	y_next = p->back().y;

	a = x_next - (2*x) + x_prev;
	b = y_next - (2*y) + y_prev;
	sum += (a*a + b*b);

	return (sum);
}


//The gradient of f, df = (df/dx, df/dy)
//derivative in each point [2x(i-2)-8x(i-1)+12x(i)-8x(i+1)+2x(i+2)]
void
my_df (const gsl_vector *v, void *params, gsl_vector *df)
{
	list<carmen_ackerman_traj_point_t> *p = (list<carmen_ackerman_traj_point_t> *) params;
	int i, j, size =(p->size() - 2);

	double x_prev2= 0;
	double x_prev = p->front().x;
	double x      = gsl_vector_get(v, 0);
	double x_next = gsl_vector_get(v, 1);
	double x_next2= gsl_vector_get(v, 2);
	double sum_x  =  (10*x) - (8*x_next) + (2*x_next2) - (4*x_prev);
	gsl_vector_set(df, 0, sum_x);

	double y_prev2= 0;
	double y_prev = p->front().y;
	double y      = gsl_vector_get(v, size);
	double y_next = gsl_vector_get(v, size+1);
	double y_next2= gsl_vector_get(v, size+2);
	double sum_y  = (10*y) - (8*y_next) + (2*y_next2) - (4*y_prev);
	gsl_vector_set(df, size, sum_y);

	for (i = 3, j = (size+3); i < size; i++, j++)
	{
		x_prev2= x_prev;
		x_prev = x;
		x      = x_next;
		x_next = x_next2;
		x_next2= gsl_vector_get(v, i);
		sum_x = (2*x_prev2) - (8*x_prev) + (12*x) - (8*x_next) + (2*x_next2);
		gsl_vector_set(df, (i-2), sum_x);

		y_prev2= y_prev;
		y_prev = y;
		y      = y_next;
		y_next = y_next2;
		y_next2= gsl_vector_get(v, j);
		sum_y = (2*y_prev2) - (8*y_prev) + (12*y) - (8*y_next) + (2*y_next2);
		gsl_vector_set(df, (j-2), sum_y);
	}

	x_prev2= x_prev;
	x_prev = x;
	x      = x_next;
	x_next = x_next2;
	x_next2= p->back().x;
	sum_x  = (2*x_prev2) - (8*x_prev) + (12*x) - (8*x_next) + (2*x_next2);
	gsl_vector_set(df, size-2, sum_x);

	y_prev2= y_prev;
	y_prev = y;
	y      = y_next;
	y_next = y_next2;
	y_next2= p->back().y;
	sum_y  = (2*y_prev2) - (8*y_prev) + (12*y) - (8*y_next) + (2*y_next2);
	gsl_vector_set(df, (2*size)-2, sum_y);

	x_prev2= x_prev;
	x_prev = x;
	x      = x_next;
	x_next = x_next2;
	sum_x  = (2*x_prev2) - (8*x_prev) + (10*x) - (4*x_next);
	gsl_vector_set(df, size-1, sum_x);

	y_prev2= y_prev;
	y_prev = y;
	y      = y_next;
	y_next = y_next2;
	sum_y  = (2*y_prev2) - (8*y_prev) + (10*y) - (4*y_next);
	gsl_vector_set(df, (2*size)-1, sum_y);
}

// Compute both f and df together
void
my_fdf (const gsl_vector *x, void *params, double *f, gsl_vector *df)
{
	*f = my_f(x, params);
	my_df(x, params, df);
}


int
smooth_rddf_using_conjugate_gradient(carmen_ackerman_traj_point_t *poses_ahead, int num_poses_ahead,
		carmen_ackerman_traj_point_t *poses_back, int num_poses_back)
{
	int iter = 0;
	int status, i = 0, j = 0, size;

	const gsl_multimin_fdfminimizer_type *T;
	gsl_multimin_fdfminimizer *s;
	gsl_vector *v;
	gsl_multimin_function_fdf my_func;

	list<carmen_ackerman_traj_point_t>::iterator it;
	list<carmen_ackerman_traj_point_t> path;

	for (i = (num_poses_back - 1); i > 0; i--) // skip poses_back[0], because it is equal to poses_ahead[0]
		path.push_back(poses_back[i]);

	for (i = 0; i < num_poses_ahead; i++)
		path.push_back(poses_ahead[i]);

	if (path.size() < 5)
		return (1);

	size = path.size();

	my_func.n = (2 * size) - 4;
	my_func.f = my_f;
	my_func.df = my_df;
	my_func.fdf = my_fdf;
	my_func.params = &path;

	v = gsl_vector_alloc ((2 * size) - 4);

	static int count = 0;
	count++;
//	FILE *plot = fopen("gnuplot_smooth_lane.m", "w");

//	fprintf(plot, "a%d = [\n", count);
	it = path.begin();
//	fprintf(plot, "%f %f\n", it->x, it->y);

	it++; // skip the first pose
	for (i = 0, j = (size - 2); i < (size - 2); i++, j++, it++)
	{
//		fprintf(plot, "%f %f\n", it->x, it->y);

		gsl_vector_set (v, i, it->x);
		gsl_vector_set (v, j, it->y);
	}

//	fprintf(plot, "%f %f]\n\n", it->x, it->y);

	T = gsl_multimin_fdfminimizer_conjugate_fr;
	s = gsl_multimin_fdfminimizer_alloc (T, (2 * size) - 4);

	gsl_multimin_fdfminimizer_set (s, &my_func, v, 0.1, 0.01);  //(function_fdf, gsl_vector, step_size, tol)

	//Algumas vezes o código não realiza a otimização porque dá erro no do-while abaixo
	do
	{
		iter++;
		status = gsl_multimin_fdfminimizer_iterate (s);
		if (status) // error code
			return (0);

		status = gsl_multimin_test_gradient (s->gradient, 0.2);   //(gsl_vector, epsabs) and  |g| < epsabs
		// status == 0 (GSL_SUCCESS), if a minimum has been found
	} while (status == GSL_CONTINUE && iter < 999);

//	printf("status %d, iter %d\n", status, iter);
//	fflush(stdout);
	it = path.begin();

//	fprintf(plot, "b%d = [   \n%f %f\n", count, it->x, it->y);

	it++; // skip the first pose
	for (i = 0, j = (size - 2); i < (size - 2); i++, j++, it++)
	{
		it->x = gsl_vector_get (s->x, i);
		it->y = gsl_vector_get (s->x, j);

//		fprintf(plot, "%f %f\n", it->x, it->y);
	}

//	fprintf(plot, "%f %f]\n\n", it->x, it->y);
//	fprintf(plot, "\nplot (a%d(:,1), a%d(:,2), b%d(:,1), b%d(:,2)); \nstr = input (\"a   :\");\n\n", count, count, count, count);
//	fclose(plot);

	it = path.begin();
	it++;
	for (i = (num_poses_back - 2); i > 0; i--, it++) // skip first and last poses
		poses_back[i] = *it;
	poses_back[0] = *it;

	for (i = 0; i < num_poses_ahead - 1; i++, it++) // skip last pose
		poses_ahead[i] = *it;

	calculate_theta_and_phi(poses_ahead, num_poses_ahead, poses_back, num_poses_back);

	gsl_multimin_fdfminimizer_free (s);
	gsl_vector_free (v);

	return (1);
}

//////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                           //
// Publishers                                                                                //
//                                                                                           //
///////////////////////////////////////////////////////////////////////////////////////////////


void
publish_astar_draw()
{
	virtual_laser_message.host = carmen_get_host();
	carmen_mapper_publish_virtual_laser_message(&virtual_laser_message, carmen_get_time());
//	virtual_laser_message.num_positions = 0;
}


void
astar_publish_rddf_poses(carmen_ackerman_traj_point_t *poses_ahead, int size)
{
	carmen_ackerman_traj_point_t last_pose;
	last_pose.x = poses_ahead->x;
	last_pose.y = poses_ahead->y;
	last_pose.theta = poses_ahead->theta;
	last_pose.v = poses_ahead->v;
	last_pose.phi = poses_ahead->phi;
	int annotations[2] = { 1, 2 };
	int annotation_codes[2] = { 1, 2 };

	carmen_rddf_publish_road_profile_message(poses_ahead, &last_pose, size, 1, annotations, annotation_codes);
	//carmen_rddf_publish_road_profile_message(&last_pose, poses_ahead, 1, size, annotations, annotation_codes);
}


///////////////////////////////////////////////////////////////////////////////////////////////


void
astar_mount_rddf_message(state_node *current_state, state_node *goal_state, carmen_obstacle_distance_mapper_map_message *distance_map)
{
	temp_rddf_poses_from_path = build_rddf_poses(current_state, distance_map);
	carmen_rddf_poses_from_path = &temp_rddf_poses_from_path[0];
	poses_size = temp_rddf_poses_from_path.size();
/*
	printf("Otimização iniciada\n");
	carmen_ackerman_traj_point_t last_pose;
	last_pose.x = carmen_rddf_poses_from_path->x;
	last_pose.y = carmen_rddf_poses_from_path->y;
	last_pose.theta = carmen_rddf_poses_from_path->theta;
	last_pose.v = carmen_rddf_poses_from_path->v;
	last_pose.phi = carmen_rddf_poses_from_path->phi;
	smooth_rddf_using_conjugate_gradient(carmen_rddf_poses_from_path, temp_rddf_poses_from_path.size(), &last_pose, 1);
	printf("Otimização feita!\n");

*/
	astar_publish_rddf_poses(carmen_rddf_poses_from_path, poses_size);
	publish_astar_draw();


	//printf("%f\n",DIST2D(current_globalpos_msg.globalpos, goal_state->state));
	printf("Chegou ao fim do path!\n");
}


state_node*
create_state_node(double x, double y, double theta, double v, double phi, double g, double h, state_node *parent)
{
	state_node *new_state = (state_node*) malloc(sizeof(state_node));
	new_state->state.x = x;
	new_state->state.y = y;
	new_state->state.theta = theta;
	new_state->state.v = v;
	new_state->state.phi = phi;
	new_state->f = g+h;
	new_state->g = g;
	new_state->h = h;
	new_state->parent = parent;
	new_state->is_open = 1;
	new_state->was_visited = 1;
	new_state->is_obstacle = 0;

	return (new_state);
}


static void
get_pos_message(carmen_point_t robot_pose)
{
	if (goal_received != 0)
	{
		printf("Pos_message_handler: %lf %lf %lf\n", robot_pose.x, robot_pose.y, robot_pose.theta);
		compute_astar_path(&robot_pose, final_goal, robot_config , &distance_map);
		goal_received = 0;
	}
}


double
obstacle_distance(double x, double y, carmen_obstacle_distance_mapper_map_message *distance_map)
{
	if( NULL == distance_map)
		exit(1);

    carmen_point_t p;
    p.x = x;
    p.y = y;

    return (carmen_obstacle_avoider_distance_from_global_point_to_obstacle(&p, distance_map));
}


int
get_distance_map_x(int x, carmen_obstacle_distance_mapper_map_message *distance_map)
{
	return round((double) (x * distance_map->config.resolution) + distance_map->config.x_origin);
}


int
get_distance_map_y(int y, carmen_obstacle_distance_mapper_map_message *distance_map)
{
	return round((double) (y * distance_map->config.resolution) + distance_map->config.y_origin);
}


void
alloc_astar_map(carmen_obstacle_distance_mapper_map_message *distance_map)
{
	int i, j, z;
	int theta_size = THETA_SIZE;
	int x_size = round(distance_map->config.x_size / ASTAR_GRID_RESOLUTION + 1);
	int y_size = round(distance_map->config.y_size / ASTAR_GRID_RESOLUTION + 1);
	double pos_x = 0.0;
	double pos_y = 0.0;
	printf("sizemap = %d %d \n", x_size, y_size);
	astar_map = (state_node_p ***)calloc(x_size, sizeof(state_node_p**));
	carmen_test_alloc(astar_map);

	for (i = 0; i < x_size; i++)
	{
		astar_map[i] = (state_node_p **)calloc(y_size, sizeof(state_node_p*));
		carmen_test_alloc(astar_map[i]);

		for (j = 0; j < y_size; j++)
		{
			astar_map[i][j] = (state_node_p*)calloc(theta_size, sizeof(state_node_p));
			carmen_test_alloc(astar_map[i][j]);

			for (z = 0; z < THETA_SIZE; z++)
			{
				astar_map[i][j][z]= (state_node_p) malloc(sizeof(state_node));
				carmen_test_alloc(astar_map[i][j][z]);
				pos_x = get_distance_map_x(i, distance_map);
				pos_y = get_distance_map_y(j, distance_map);
				if(obstacle_distance(pos_x, pos_y, distance_map) < 2.0)
					astar_map[i][j][z]->is_obstacle = 1;
				else
					astar_map[i][j][z]->is_obstacle = 0;

				astar_map[i][j][z]->was_visited = 0;

			}
		}
	}
}

void
clear_astar_map(carmen_obstacle_distance_mapper_map_message *distance_map)
{
	int i, j, k, z;
	int x_size = round(distance_map->config.x_size / ASTAR_GRID_RESOLUTION + 1);
	int y_size = round(distance_map->config.y_size / ASTAR_GRID_RESOLUTION + 1);

	for (i = 0; i < x_size; i++)
		for (j = 0; j < y_size; j++)
			for (k = 0; k < THETA_SIZE; k++)
				astar_map[i][j][k] = NULL;
}


int
get_astar_map_theta(double theta)
{
	theta = theta < 0 ? (2 * M_PI + theta) : theta;
	int resolution = (int) round(360/THETA_SIZE);

	return  (int)round((carmen_radians_to_degrees(theta) / resolution)) % (int)round(360 / resolution);
}


int
get_astar_map_x(double x, carmen_obstacle_distance_mapper_map_message *distance_map)
{
	return round((double) (x - distance_map->config.x_origin) / distance_map->config.resolution);
}


int
get_astar_map_y(double y, carmen_obstacle_distance_mapper_map_message *distance_map)
{
	return round((double) (y - distance_map->config.y_origin) / distance_map->config.resolution);
}


bool
my_list_ordenation (state_node *a, state_node *b)
{
	return (a->g+ a->h > b->g + b->h);
}


discrete_pos_node*
get_current_pos(state_node* current_state, carmen_obstacle_distance_mapper_map_message *distance_map)
{
	discrete_pos_node *current_pos = (discrete_pos_node*) malloc(sizeof(discrete_pos_node));
	current_pos->x = get_astar_map_x(current_state->state.x, distance_map);
	current_pos->y = get_astar_map_y(current_state->state.y, distance_map);
	current_pos->theta = get_astar_map_theta(current_state->state.theta);

	return current_pos;
}


void
insert_open(vector<state_node*> &open, state_node *current_state, carmen_obstacle_distance_mapper_map_message *distance_map)
{
	open.push_back(current_state);
	discrete_pos_node *current_pos = get_current_pos(current_state, distance_map);
	astar_map[current_pos->x][current_pos->y][current_pos->theta] = current_state;
}


state_node*
get_lowest_rank(fibonacci_heap<state_node*, boost::heap::compare<StateNodePtrComparator>> &open)
{
	state_node *current_state = open.top();
	return current_state;
}


int
is_goal(state_node* current_state, state_node* goal_state)
{
	if(DIST2D(current_state->state, goal_state->state) < 1.5 && abs(carmen_radians_to_degrees(current_state->state.theta) - carmen_radians_to_degrees(goal_state->state.theta)) < 10)
		return 1;
	else
		return 0;
}


state_node*
pop_lowest_rank(fibonacci_heap<state_node*, boost::heap::compare<StateNodePtrComparator>> &open)
{
	state_node *current_state = open.top();
	open.pop();
	return current_state;
}


//vector<state_node*>
double
reed_shepp_path(state_node *current, state_node *goal_state)
{
	int rs_pathl;
	int rs_numero;
	double tr;
	double ur;
	double vr;
	double distance_traveled = 0.0;
	double distance_traveled_old = 0.0;
	carmen_ackerman_traj_point_t rs_points[6]; // Por alguma razão, com o valor 5 acontece stack smashing às vezes quando o rs_pathl == 5
	double v_step;
	double step_weight;
	double path_cost = 0.0;
	carmen_ackerman_traj_point_t point_old = {0, 0, 0, 0, 0};

	vector<state_node*> rs_path_nodes;

	rs_init_parameters(robot_config.max_phi, robot_config.distance_between_front_and_rear_axles);
	double rs_length = reed_shepp(current->state, goal_state->state, &rs_numero, &tr, &ur, &vr);

	rs_pathl = constRS(rs_numero, tr, ur, vr, current->state, rs_points);
	for (int i = rs_pathl-1; i > 0 /*rs_pathl*/; i--)
	{
		carmen_ackerman_traj_point_t point = rs_points[i];
		if (rs_points[i].v < 0.0)
		{
			v_step = 2.0;
			step_weight = 1.0;
		}
		else
		{
			v_step = -2.0;
			step_weight = 1.0;
		}
		while (DIST2D(point, rs_points[i-1]) > 1.0)
		{
			distance_traveled_old = distance_traveled;
			point_old = point;
			point = carmen_libcarmodel_recalc_pos_ackerman(point, v_step, rs_points[i].phi,
					1.0, &distance_traveled, DELTA_T, robot_config);
//			draw_astar_object(&point, CARMEN_PURPLE);
//			path_cost += step_weight * (distance_traveled - distance_traveled_old);
			path_cost += DIST2D(point, point_old);
//			printf("[rs] Comparação de pontos: %f %f\n", DIST2D(point, point_old), distance_traveled - distance_traveled_old);

//        	state_node_p new_state = (state_node_p) malloc(sizeof(state_node));
//       	new_state->state = point;
//      	rs_path_nodes.push_back(new_state);
		}
	}
	/*
	//return path_cost;
	std::reverse(rs_path_nodes.begin(), rs_path_nodes.end());
	state_node_p ant_state = (state_node_p) malloc(sizeof(state_node));
	ant_state = current;
	for (int i = 0; i<rs_path_nodes.size(); i++)
	{
		rs_path_nodes[i]->parent = ant_state;
		ant_state = rs_path_nodes[i];
	}

//	return rs_path_nodes;
 *
 * */
//	publish_astar_draw();
//	printf("[reed_shepp] %f\n", path_cost);
	return path_cost;
}


int
hitObstacle(vector<state_node*> path, carmen_obstacle_distance_mapper_map_message *distance_map)
{
	for(int i = 0; i < path.size(); i++)
	{
//		printf("[hitObstacle] %f %f %f \n", path[i]->state.x, path[i]->state.y, path[i]->state.theta);
		discrete_pos_node *current_pos = get_current_pos(path[i], distance_map);
		if(astar_map[current_pos->x][current_pos->y][current_pos->theta]->is_obstacle == 1)
			return 1;
	}
	return 0;
}


vector<state_node*>
expansion(state_node *current, state_node *goal_state, carmen_obstacle_distance_mapper_map_message *distance_map)
{

	double add_x[3] = {-1.0, 0.0, 1.0};
    double add_y[3] = {-1.0, 0.0, 1.0};
    vector<state_node*> neighbor;

    double target_phi, distance_traveled = 0.0;
    double steering_acceleration[3] = {-0.25, 0.0, 0.25};
    double target_v[2]   = {2.0, -2.0};
    int size_for;

    if (ACKERMAN_EXPANSION)
        	size_for = sizeof(target_v)/sizeof(target_v[0]);
        else
        	size_for = 3;

    for (int i = 0; i < size_for; i++)
    {
    	for (int j = 0; j<3; j++)
    	{
        	state_node_p new_state = (state_node_p) malloc(sizeof(state_node));
        	if(ACKERMAN_EXPANSION)
        	{
        		target_phi = carmen_clamp(-robot_config.max_phi, (current->state.phi + steering_acceleration[j]), robot_config.max_phi);
        		new_state->state = carmen_libcarmodel_recalc_pos_ackerman(current->state, target_v[i], target_phi, 1.0 , &distance_traveled, DELTA_T, robot_config);
        	}
        	else
        	{
        		new_state->state.x = current->state.x + add_x[i];
				new_state->state.y = current->state.y + add_y[j];
				new_state->state.theta = current->state.theta;
        	}
			discrete_pos_node *current_pos = get_current_pos(new_state, distance_map);
			if(astar_map[current_pos->x][current_pos->y][current_pos->theta]->is_obstacle == 1)
			{
				free(new_state);
			}
			else
			{
//				printf("[expansion] %f %f %f\n", new_state->state.x, new_state->state.y, new_state->state.theta);
				draw_astar_object(&new_state->state, CARMEN_RED);
				neighbor.push_back(new_state);
			}
    	}
    }
    publish_astar_draw();
    return neighbor;
}


double
movementcost(state_node *current, state_node *neighbor)
{
	return DIST2D(current->state, neighbor->state);
}

int
node_exist(vector<state_node*> &list, state_node *current, carmen_obstacle_distance_mapper_map_message *distance_map)
{
	for (int i = 0; i < list.size(); i++)
	{
		discrete_pos_node *current_pos = get_current_pos(current, distance_map);
		discrete_pos_node *list_pos = get_current_pos(list[i], distance_map);
		if(current_pos->x == list_pos->x && current_pos->y == list_pos->y)
		{
			return i+1;
		}
	}
	return 0;
}


vector<state_node*>
expansion_discrete(state_node *current, state_node *goal_state, carmen_obstacle_distance_mapper_map_message *distance_map)
{

	double add_x[3] = {-1.0, 0.0, 1.0};
    double add_y[3] = {-1.0, 0.0, 1.0};
    vector<state_node*> neighbor;

    int size_for = 3;

    for (int i = 0; i < size_for; i++)
    {
    	for (int j = 0; j<3; j++)
    	{
        	state_node_p new_state = (state_node_p) malloc(sizeof(state_node));

			new_state->state.x = current->state.x + add_x[i];
			new_state->state.y = current->state.y + add_y[j];
			new_state->state.theta = current->state.theta;

			discrete_pos_node *current_pos = get_current_pos(new_state, distance_map);
			if(astar_map[current_pos->x][current_pos->y][current_pos->theta]->is_obstacle == 1)
			{
				free(new_state);
			}
			else
			{
//				draw_astar_object(&new_state->state, CARMEN_ORANGE);
				neighbor.push_back(new_state);
			}
    	}
    }
//    publish_astar_draw();

    return neighbor;
}


double
dijkstra(state_node *start_state, state_node *goal, carmen_obstacle_distance_mapper_map_message *distance_map)
{
		int indice = 0;
		int it_number = 0;
		double cost = 0.0;
		vector<state_node*> neighbor;
		state_node *goal_state, *current;
//		start_state = current_state;
		goal_state = goal;
		vector<state_node*> open;

		// Following the code from: http://theory.stanford.edu/~amitp/GameProgramming/ImplementationNotes.html
		insert_open(open, start_state, distance_map);
		while (!open.empty() && DIST2D(open.back()->state, goal_state->state) > 1.5)
		{
			current = open.back();
			open.pop_back();
			//printf("[dijkstra]current = %f %f %f\n", current->state.x, current->state.y, current->state.theta);
			neighbor = expansion_discrete(current, goal_state, distance_map);
			while(it_number< neighbor.size())
			{
				cost = current->g + movementcost(current, neighbor[it_number]);
				indice = node_exist(open, neighbor[it_number], distance_map);
				if(indice != 0 && cost < neighbor[it_number]->g)
				{
					open.erase(open.begin() + (indice-1));
				}
	        	discrete_pos_node *current_pos = get_current_pos(neighbor[it_number], distance_map);

				if(astar_map[current_pos->x][current_pos->y][current_pos->theta]->was_visited == 0 || (astar_map[current_pos->x][current_pos->y][current_pos->theta]->is_closed == 0 && node_exist(open, neighbor[it_number], distance_map) == 0))
				{
					neighbor[it_number]->g = cost;
					neighbor[it_number]->h = DIST2D(neighbor[it_number]->state, goal->state);
					neighbor[it_number]->parent = current;
					open.push_back(neighbor[it_number]);
					sort(open.begin(), open.end(), my_list_ordenation);
				}
				it_number++;
			}
			it_number = 0;
			neighbor.clear();

		}
//		virtual_laser_message.num_positions = 0;

		if(open.empty())
		{
			printf("Caminho não encontrado [dijkstra]\n");
			return -1;
		}
		current = open.back();
		build_rddf_poses(current, distance_map);
//		printf("[dijkstra] %f\n", current->g);
		publish_astar_draw();
		open.clear();
//		exit(1);
		return current->g;
}


double
g(state_node *current)
{
	return current->g;
}


double
h(state_node *current, state_node *goal, carmen_obstacle_distance_mapper_map_message *distance_map)
{
	double rs = reed_shepp_path(current, goal) * 1.3;
//	double rs = 0;
//	double ho = dijkstra(current, goal, distance_map);
	double ho = 0;
	printf("[h]rs = %f\tho = %f\n", rs, ho);
	return max(rs, ho);
//	return DIST2D(current->state, goal->state);
}


void
compute_astar_path(carmen_point_t *robot_pose, carmen_point_t *goal_pose, carmen_robot_ackerman_config_t robot_config,
		carmen_obstacle_distance_mapper_map_message *distance_map)
{
	fibonacci_heap<state_node*, boost::heap::compare<StateNodePtrComparator>> heap_open_list;
	vector<state_node*> rs_path;
	int cont_rs = 0;
	int it_number = 0;
	int indice = 0;
	double cost = 0.0;
	vector<state_node*> neighbor;
	state_node *start_state, *goal_state, *current;
	start_state = create_state_node(robot_pose->x, robot_pose->y, robot_pose->theta, 0.0, 0.0, 0.0, DIST2D_P(robot_pose, goal_pose), NULL);
	goal_state = create_state_node(goal_pose->x, goal_pose->y, goal_pose->theta, 0.0, 0.0, DBL_MAX, DBL_MAX, NULL);
	alloc_astar_map(distance_map);

	start_state->is_open = 1;
	start_state->was_visited = 1;
	start_state->is_obstacle = 0;
	// OPEN = priority queue containing START
	heap_open_list.push(start_state);
	discrete_pos_node *current_pos = get_current_pos(start_state, distance_map);
	astar_map[current_pos->x][current_pos->y][current_pos->theta] = start_state;

	// Following the code from: http://theory.stanford.edu/~amitp/GameProgramming/ImplementationNotes.html
	while (!heap_open_list.empty() && is_goal(get_lowest_rank(heap_open_list), goal_state) != 1)
	{
		state_node *current= heap_open_list.top();
		heap_open_list.pop();
		discrete_pos_node *current_pos = get_current_pos(current, distance_map);

/* Usado em expansão analítica
		if(cont_rs%5==0)
		{
			rs_path = reed_shepp_path(current, goal_state, distance_map);
			if(hitObstacle(rs_path, distance_map) == 0)
			{
				rs_path.front()->parent = current;
				current = rs_path.back();
				current->g = 0;
				current->h = 0;
				open.push_back(current);
				sort(open.begin(), open.end(), my_list_ordenation);
				printf("Reed Shepp encontrou o caminho \n");
				break;
			}
		}
*/
		neighbor = expansion(current, goal_state, distance_map);
		while(it_number< neighbor.size())
		{
			cost = g(current) + movementcost(current, neighbor[it_number]);
        	discrete_pos_node *current_pos = get_current_pos(neighbor[it_number], distance_map);
//        	printf("current = %f %f %f\n", neighbor[it_number]->state.x, neighbor[it_number]->state.y, neighbor[it_number]->state.theta);
			// if neighbor in OPEN and cost less than g(neighbor):
			if(astar_map[current_pos->x][current_pos->y][current_pos->theta]->was_visited == 1 && (astar_map[current_pos->x][current_pos->y][current_pos->theta]->is_open == 1 &&
				astar_map[current_pos->x][current_pos->y][current_pos->theta]->g > neighbor[it_number]->g))
			{
				// remove neighbor from OPEN, because new path is better
				astar_map[current_pos->x][current_pos->y][current_pos->theta]->is_open = 0;
			}

			// if neighbor in CLOSED and cost less than g(neighbor): ⁽²⁾
			if(astar_map[current_pos->x][current_pos->y][current_pos->theta]->was_visited == 1 && (astar_map[current_pos->x][current_pos->y][current_pos->theta]->is_closed == 1 &&
				astar_map[current_pos->x][current_pos->y][current_pos->theta]->g > neighbor[it_number]->g))
			{
				// remove neighbor from CLOSED
				astar_map[current_pos->x][current_pos->y][current_pos->theta]->is_closed = 0;
			}

			// if neighbor not in OPEN and neighbor not in CLOSED: -- Também precisa verificar se ele ainda não foi visitado, que significa que o neighbor não existe ainda
			if (astar_map[current_pos->x][current_pos->y][current_pos->theta]->was_visited == 0 || (astar_map[current_pos->x][current_pos->y][current_pos->theta]->is_closed == 0 &&
				astar_map[current_pos->x][current_pos->y][current_pos->theta]->is_open == 0))
			{
			// set g(neighbor) to cost
				neighbor[it_number]->parent = current;
				neighbor[it_number]->g = cost;

				neighbor[it_number]->h = h(neighbor[it_number], goal_state, distance_map);

				neighbor[it_number]->f = neighbor[it_number]->g + neighbor[it_number]->h;
/*
				if(neighbor[it_number]->state.v < 0)
				{
					neighbor[it_number]->f *= 1.1;
				}

				if(neighbor[it_number]->state.v != current->state.v)
				{
					neighbor[it_number]->f += 1;
				}
*/
				neighbor[it_number]->is_open = 1;
				neighbor[it_number]->is_obstacle = 0;
				neighbor[it_number]->was_visited = 1;
				// add neighbor to OPEN
				draw_astar_object(&neighbor[it_number]->state, 0);
				publish_astar_draw();
				heap_open_list.push(neighbor[it_number]);
				astar_map[current_pos->x][current_pos->y][current_pos->theta] = neighbor[it_number];
				// set priority queue rank to g(neighbor) + h(neighbor) -- Já é feito pela heap
				// set neighbor's parent to current -- Já foi feito anteriormente
			}
			it_number++;

		}
		it_number = 0;
		neighbor.clear();
		rs_path.clear();
		cont_rs++;

	}
	if(heap_open_list.empty())
		printf("Caminho não encontrado\n");
	else
	{
		virtual_laser_message.num_positions = 0;
		current = pop_lowest_rank(heap_open_list);
		astar_mount_rddf_message(current, goal_state, distance_map);
		path_sended = 1;
	}
	heap_open_list.clear();
	clear_astar_map(distance_map);
	virtual_laser_message.num_positions = 0;
	printf("Terminou compute_astar_path !\n");
//	exit(1);

}


void
send_new_rddf_poses()
{
	if(DIST2D_D_P(current_globalpos_msg.globalpos, final_goal) > 4.0)
	{
		static double last_time_stamp = 0.0;
		if ((carmen_get_time() - last_time_stamp) > 0.5)
		{
			last_time_stamp = carmen_get_time();
//			printf("Poses Atualizadas: %f %f %f %f %f %f\n", carmen_rddf_poses_from_path->x, carmen_rddf_poses_from_path->y, carmen_rddf_poses_from_path->theta, final_goal->x, final_goal->y, DIST2D_D_P(current_globalpos_msg.globalpos, final_goal));
			while(DIST2D_D_P(current_globalpos_msg.globalpos, carmen_rddf_poses_from_path) > 1.5)
			{
				carmen_rddf_poses_from_path++;
				contador++;
				//printf("descontado: %d %f\n", contador, carmen_rddf_poses_from_path->x);
			}
			astar_publish_rddf_poses(carmen_rddf_poses_from_path, poses_size - contador);
//			printf("Poses enviadas! %lf\n",  current_globalpos_msg.globalpos.x);
		}
	}
	else
	{
		contador = 0;
		poses_size = 0;
		printf("Chegou ao goal!\n");
		temp_rddf_poses_from_path.clear();
		exit(0);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                           //
// Handlers                                                                                  //
//                                                                                           //
///////////////////////////////////////////////////////////////////////////////////////////////


static void
localize_globalpos_handler(carmen_localize_ackerman_globalpos_message *msg)
{
	current_globalpos_msg = *msg;
	get_pos_message(msg->globalpos/*, msg->v, msg->timestamp*/);
	if(path_sended)
		send_new_rddf_poses();
}


void
carmen_rddf_play_end_point_message_handler(carmen_rddf_end_point_message *rddf_end_point_message)
{
	final_goal = &(rddf_end_point_message->point);
	goal_received = 1;
	printf("carmen_rddf_play_end_point: %lf %lf %lf\n", rddf_end_point_message->point.x, rddf_end_point_message->point.y, rddf_end_point_message->point.theta);
//	printf ("Recebeu Goal!!!!!  %d\n", rddf_end_point_message->number_of_poses);
}


static void
carmen_obstacle_distance_mapper_compact_map_message_handler(carmen_obstacle_distance_mapper_compact_map_message *message)
{
	static carmen_obstacle_distance_mapper_compact_map_message *compact_distance_map = NULL;

	if (compact_distance_map == NULL)
	{
		carmen_obstacle_distance_mapper_create_new_map(&distance_map, message->config, message->host, message->timestamp);
		compact_distance_map = (carmen_obstacle_distance_mapper_compact_map_message *) (calloc(1, sizeof(carmen_obstacle_distance_mapper_compact_map_message)));
		carmen_obstacle_distance_mapper_cpy_compact_map_message_to_compact_map(compact_distance_map, message);
		carmen_obstacle_distance_mapper_uncompress_compact_distance_map_message(&distance_map, message);
	}
	else
	{
		carmen_obstacle_distance_mapper_clear_distance_map_message_using_compact_map(&distance_map, compact_distance_map, DISTANCE_MAP_HUGE_DISTANCE);
		carmen_obstacle_distance_mapper_free_compact_distance_map(compact_distance_map);
		carmen_obstacle_distance_mapper_cpy_compact_map_message_to_compact_map(compact_distance_map, message);
		carmen_obstacle_distance_mapper_uncompress_compact_distance_map_message(&distance_map, message);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                           //
// Subscribes                                                                                //
//                                                                                           //
///////////////////////////////////////////////////////////////////////////////////////////////


void
carmen_rddf_play_subscribe_messages()
{
	carmen_localize_ackerman_subscribe_globalpos_message(NULL, (carmen_handler_t) localize_globalpos_handler, CARMEN_SUBSCRIBE_LATEST);
	carmen_obstacle_distance_mapper_subscribe_compact_map_message(NULL, (carmen_handler_t) carmen_obstacle_distance_mapper_compact_map_message_handler, CARMEN_SUBSCRIBE_LATEST);
	carmen_rddf_subscribe_end_point_message(NULL, (carmen_handler_t) carmen_rddf_play_end_point_message_handler, CARMEN_SUBSCRIBE_LATEST);
}


///////////////////////////////////////////////////////////////////////////////////////////////


void
path_planner_astar_initialize()
{
	memset(&virtual_laser_message, 0, sizeof(carmen_mapper_virtual_laser_message));
	virtual_laser_message.positions = (carmen_position_t *) calloc(MAX_VIRTUAL_LASER_SAMPLES, sizeof(carmen_position_t));
	virtual_laser_message.colors = (char *) calloc(MAX_VIRTUAL_LASER_SAMPLES, sizeof(char));
	virtual_laser_message.host = carmen_get_host();
}


void
shutdown_module(int signo)
{
    if (signo == SIGINT) {
        carmen_ipc_disconnect();
        printf("path_planner_astar_main: Disconnected.\n");
        exit(0);
    }
}


static void
carmen_rddf_play_get_parameters(int argc, char** argv)
{
	carmen_param_t param_list[] =
	{
			{(char *) "robot",	(char *) "length",								  		CARMEN_PARAM_DOUBLE, &robot_config.length,							 			1, NULL},
			{(char *) "robot",	(char *) "width",								  		CARMEN_PARAM_DOUBLE, &robot_config.width,								 			1, NULL},
			{(char *) "robot", 	(char *) "distance_between_rear_wheels",		  		CARMEN_PARAM_DOUBLE, &robot_config.distance_between_rear_wheels,			 		1, NULL},
			{(char *) "robot", 	(char *) "distance_between_front_and_rear_axles", 		CARMEN_PARAM_DOUBLE, &robot_config.distance_between_front_and_rear_axles, 		1, NULL},
			{(char *) "robot", 	(char *) "distance_between_front_car_and_front_wheels",	CARMEN_PARAM_DOUBLE, &robot_config.distance_between_front_car_and_front_wheels,	1, NULL},
			{(char *) "robot", 	(char *) "distance_between_rear_car_and_rear_wheels",	CARMEN_PARAM_DOUBLE, &robot_config.distance_between_rear_car_and_rear_wheels,		1, NULL},
			{(char *) "robot", 	(char *) "max_velocity",						  		CARMEN_PARAM_DOUBLE, &robot_config.max_v,									 		1, NULL},
			{(char *) "robot", 	(char *) "max_steering_angle",					  		CARMEN_PARAM_DOUBLE, &robot_config.max_phi,								 		1, NULL},
			{(char *) "robot", 	(char *) "maximum_acceleration_forward",				CARMEN_PARAM_DOUBLE, &robot_config.maximum_acceleration_forward,					1, NULL},
			{(char *) "robot", 	(char *) "maximum_acceleration_reverse",				CARMEN_PARAM_DOUBLE, &robot_config.maximum_acceleration_reverse,					1, NULL},
			{(char *) "robot", 	(char *) "maximum_deceleration_forward",				CARMEN_PARAM_DOUBLE, &robot_config.maximum_deceleration_forward,					1, NULL},
			{(char *) "robot", 	(char *) "maximum_deceleration_reverse",				CARMEN_PARAM_DOUBLE, &robot_config.maximum_deceleration_reverse,					1, NULL},
			{(char *) "robot", 	(char *) "maximum_steering_command_rate",				CARMEN_PARAM_DOUBLE, &robot_config.maximum_steering_command_rate,					1, NULL},
			{(char *) "robot", 	(char *) "understeer_coeficient",						CARMEN_PARAM_DOUBLE, &robot_config.understeer_coeficient,							1, NULL}
		};

	int num_items = sizeof(param_list) / sizeof(param_list[0]);
	carmen_param_install_params(argc, argv, param_list, num_items);
}


int
main(int argc, char **argv)
{
	carmen_ipc_initialize(argc, argv);

	carmen_param_check_version(argv[0]);

	carmen_rddf_play_get_parameters(argc, argv);

	carmen_rddf_play_subscribe_messages();

	path_planner_astar_initialize();

	signal(SIGINT, shutdown_module);

	printf("Até aqui está funcionando!\n");

	carmen_ipc_dispatch();

	return (0);
}
