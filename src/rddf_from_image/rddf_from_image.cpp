#include <carmen/carmen.h>
#include "g2o/types/slam2d/se2.h"
#include "rddf_predict_optimizer.h"
#include <vector>
#include <algorithm>
#include <fstream>
#include <carmen/rddf_interface.h>
#include <carmen/rddf_messages.h>
#include <carmen/rddf_index.h>

using namespace g2o;

int camera;
carmen_localize_ackerman_globalpos_message ackerman_message;
carmen_point_t globalpos;
carmen_pose_3D_t pose;
carmen_behavior_selector_road_profile_message last_rddf_poses;
int rddf_received = 0;
int localize_received = 0;
std::vector<carmen_ackerman_traj_point_t> carmen_rddf_poses_from_spline_vec;

FILE *file_log;
FILE *file_log2;
int first_it = 0;
int first_it2 = 0;
char txt_name[150];
char save_buffer[500];
char txt_name2[150];
char save_buffer2[500];

double y_check = -1000.0;

struct Prediction
{
    double dy;
    double dtheta;
    double k1;
    double k2;
    double k3;
    double timestamp;
};

struct mystruct_comparer
{
    bool operator()(Prediction const& ms, double const i) const
    {
        return ms.timestamp < i;
    }
};

std::vector<Prediction> preds;

std::istream& operator>>(std::istream& is, Prediction& s)
{
    return is >> s.dy >> s.dtheta >> s.k1 >> s.k2 >> s.k3 >> s.timestamp;
}

double
euclidean_distance (double x1, double x2, double y1, double y2)
{
	return ( sqrt(pow(x2-x1,2) + pow(y2-y1,2)) );
}

bool compareByLength(const Prediction &a, const Prediction &b)
{
    return a.timestamp < b.timestamp;
}


///////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                           //
// Handlers                                                                                  //
//                                                                                           //
///////////////////////////////////////////////////////////////////////////////////////////////

void
image_handler(carmen_bumblebee_basic_stereoimage_message *image_msg)
{
	if (localize_received)
	{
		globalpos.theta = ackerman_message.globalpos.theta;
		globalpos.x = ackerman_message.globalpos.x;
		globalpos.y = ackerman_message.globalpos.y;

		double img_timestamp = image_msg->timestamp;

//			Prediction *found = std::lower_bound(preds.front(),
//											   preds.back(),
//											   img_timestamp,
//											   mystruct_comparer());
		int index = -1;
		for(int i = 0; i<preds.size(); i++)
		{
			if (preds[i].timestamp == img_timestamp)
			{
				index = i;
				break;
			}
			else if (preds[i].timestamp > img_timestamp)
				break;
		}

		if(index != -1)
		{
			printf("%f\n", preds[index].timestamp);

			gsl_interp_accel *acc;
			gsl_spline *phi_spline;
			double knots_x[4] = {0.0,  30/ 3.0, 2 * 30 / 3.0, 30.0};
			double knots_y[4] = {0.0, preds[index].k1, preds[index].k2, preds[index].k3};
			acc = gsl_interp_accel_alloc();
			const gsl_interp_type *type = gsl_interp_cspline;
			phi_spline = gsl_spline_alloc(type, 4);
			gsl_spline_init(phi_spline, knots_x, knots_y, 4);

			double half_points = 0.0;
			double acresc_points = 0.5;
			double store_points[int(30/acresc_points)+1];
			double store_thetas[int(30/acresc_points)+1];
			double points_dx = 0.1;
			int indice_points = 0;
			//for(int i = 0; i < 30*2 ; i++)
			while( half_points <= 30.0 )
			{
				if(half_points == 30)
				{
					double spline_y = gsl_spline_eval(phi_spline, half_points, acc);
					double spline_y2 = gsl_spline_eval(phi_spline, half_points - points_dx, acc);
					store_points[indice_points] = spline_y;
					store_thetas[indice_points] = carmen_normalize_theta(atan2(spline_y - spline_y2, points_dx));
				}
				else
				{
					double spline_y = gsl_spline_eval(phi_spline, half_points, acc);
					double spline_y2 = gsl_spline_eval(phi_spline, half_points + points_dx, acc);
					store_points[indice_points] = spline_y;
					store_thetas[indice_points] = carmen_normalize_theta(atan2(spline_y2 - spline_y, points_dx));
				}
				indice_points++;
				half_points += acresc_points;
			}

			double ref_theta = -1 * (globalpos.theta - preds[index].dtheta);
			double ref_x = -1 * sqrt(globalpos.x * globalpos.x + globalpos.y * globalpos.y) * cos(atan2(globalpos.y, globalpos.x) + ref_theta);
			double ref_y = -1 *(sqrt(globalpos.x * globalpos.x + globalpos.y * globalpos.y) * sin(atan2(globalpos.y, globalpos.x) + ref_theta)) + preds[index].dy;
			SE2 ref_pose(ref_x, ref_y, ref_theta);
			for (int i=0; i<indice_points; i++)
			{
				carmen_ackerman_traj_point_t waypoint;
				SE2 pose_in_rddf_reference(i*0.5, store_points[i], store_thetas[i]);
				SE2 pose_in_world_reference = ref_pose.inverse() * pose_in_rddf_reference;
				waypoint.x = pose_in_world_reference[0];
				waypoint.y = pose_in_world_reference[1];
				waypoint.theta = carmen_normalize_theta(pose_in_world_reference[2]);
				waypoint.v = 9.0;
				waypoint.phi = 0.2;
				carmen_rddf_poses_from_spline_vec.push_back(waypoint);
			}

			carmen_ackerman_traj_point_t *carmen_rddf_poses_from_spline = &carmen_rddf_poses_from_spline_vec[0];
			carmen_rddf_publish_road_profile_message(
				carmen_rddf_poses_from_spline,
				NULL,
				indice_points,
				0,
				NULL,
				NULL);
		}
	}
	localize_received = 0;

}

void
localize_ackerman_globalpos_message_handler(carmen_localize_ackerman_globalpos_message *globalpos_message)
{
	localize_received = 1;
	ackerman_message = *globalpos_message;

}


void
shutdown_module(int signo)
{
    if (signo == SIGINT) {
    	fclose(file_log);
        carmen_ipc_disconnect();
        printf("Rddf_predict: Disconnected.\n");
        exit(0);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                           //
// Subscribes                                                                                //
//                                                                                           //
///////////////////////////////////////////////////////////////////////////////////////////////

void
subscribe_messages()
{
	carmen_bumblebee_basic_subscribe_stereoimage(camera, NULL, (carmen_handler_t) image_handler, CARMEN_SUBSCRIBE_LATEST);
	carmen_localize_ackerman_subscribe_globalpos_message(NULL, (carmen_handler_t) localize_ackerman_globalpos_message_handler, CARMEN_SUBSCRIBE_LATEST);
//	carmen_subscribe_message((char *) CARMEN_BEHAVIOR_SELECTOR_ROAD_PROFILE_MESSAGE_NAME, (char *) CARMEN_BEHAVIOR_SELECTOR_ROAD_PROFILE_MESSAGE_FMT,
//					NULL, sizeof (carmen_behavior_selector_road_profile_message), (carmen_handler_t) rddf_handler, CARMEN_SUBSCRIBE_LATEST);
}

void
read_parameters(char **argv)
{
	camera = atoi(argv[1]);
}


int
main(int argc , char **argv)
{
	if(argc!=2)
	{
		printf("É necessário passar o ID da câmera como parâmetro.");
		exit(1);
	}

	std::ifstream input("novo.txt");
	Prediction s;
	while (input >> s)
	{
		preds.push_back(s);
	}

	std::sort(preds.begin(),
		  preds.end(),
		  compareByLength);

	for(int i=0; i<preds.size(); i++)
	{
		printf("%f\n", preds[i].timestamp);
	}

	carmen_ipc_initialize(argc, argv);

	signal(SIGINT, shutdown_module);

	read_parameters(argv);

	printf("Aguardando mensagem\n");

	subscribe_messages();

	carmen_ipc_dispatch();

	return 0;
}
