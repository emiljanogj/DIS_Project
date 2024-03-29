#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <webots/robot.h>
#include <webots/motor.h>
#include <webots/differential_wheels.h>
#include <webots/distance_sensor.h>
#include <webots/light_sensor.h>
#include <webots/emitter.h>
#include <webots/receiver.h>

#define MAX_ACC 1000.0 // Maximum amount speed can change in 128 ms
#define NB_SENSOR 8 // Number of proximity sensors

#define DATASIZE 2*NB_SENSOR+6 // Number of elements in particle

// Fitness definitions
#define MAX_DIFF (2*MAX_SPEED) // Maximum difference between wheel speeds
#define ROB_NB 5
#define NB_SENSORS	  8	  // Number of distance sensors
#define MIN_SENS          350     // Minimum sensibility value
#define MAX_SENS          4096    // Maximum sensibility value
#define MAX_SPEED         1000     // Maximum speed
/*Webots 2018b*/
#define MAX_SPEED_WEB      6.28    // Maximum speed webots
/*Webots 2018b*/
#define FLOCK_SIZE	  5	  // Size of flock
#define TIME_STEP	  64	  // [ms] Length of time step

#define AXLE_LENGTH 		0.052	// Distance between wheels of robot (meters)
#define SPEED_UNIT_RADS		0.00628	// Conversion factor from speed unit to radian per second
#define WHEEL_RADIUS		0.0205	// Wheel radius (meters)
#define DELTA_T			0.064	// Timestep (seconds)


#define RULE1_THRESHOLD     0.20   // Threshold to activate aggregation rule. default 0.20
#define RULE1_WEIGHT        (1.5/10)	   // Weight of aggregation rule. default 0.6/10

#define RULE2_THRESHOLD     0.15   // Threshold to activate dispersion rule. default 0.15
#define RULE2_WEIGHT        (0.02/10)	   // Weight of dispersion rule. default 0.02/10

// Consistency disrupts the obstacle avoidance behavior, so we try to keep it low. Migratory urge will make up for it
#define RULE3_WEIGHT        (1.5/10)   // Weight of consistency rule. default 1.0/10

#define MIGRATION_WEIGHT    (0.1/10)   // Wheight of attraction towards the common goal. default 0.01/10

#define MIGRATORY_URGE 1 // Tells the ROB_NB if they should just go forward or move towards a specific migratory direction

#define ABS(x) ((x>=0)?(x):-(x))

#define REYNOLD_PARAMS      3
#define ROBOTS 5
#define MAX_ROB 5
#define ROB_RAD 0.035
#define ARENA_SIZE 1.89

#define NB_SENSOR 8                     // Number of proximity sensors

/* PSO definitions */
#define SWARMSIZE 10                    // Number of particles in swarm
#define NB 1                            // Number of neighbors on each side
#define LWEIGHT 2.0                     // Weight of attraction to personal best
#define NBWEIGHT 2.0                    // Weight of attraction to neighborhood best
#define VMAX 20.0                       // Maximum velocity particle can attain
#define MININIT -20.0                   // Lower bound on initialization value
#define MAXINIT 20.0                    // Upper bound on initialization value
#define ITS 10                         // Number of iterations to run
#define DATASIZE 2*NB_SENSOR+6          // Number of elements in particle (2 Neurons with 8 proximity sensors 
                                        // + 2 recursive/lateral conenctions + 1 bias)

/* Neighborhood types */
#define STANDARD    -1
#define RAND_NB      0
#define DATASIZE1    3                  // Datasize for 2nd (3 weights + 2 recursive connections + 1 bias)
#define NCLOSE_NB    1
#define FIXEDRAD_NB  2

/* Fitness definitions */
#define FIT_ITS 10 // Number of fitness steps to run during evolution

#define FINALRUNS 10
#define NEIGHBORHOOD STANDARD
#define RADIUS 0.8
#define TIME_STEP    64


WbDeviceTag ds[NB_SENSOR];
WbDeviceTag emitter;
WbDeviceTag rec;
WbDeviceTag left_motor, right_motor;
double good_w[DATASIZE] = {-11.15, -16.93, -8.20, -18.11, -17.99, 8.55, -8.89, 3.52, 29.74,
			     -7.48, 5.61, 11.16, -9.54, 4.58, 1.41, 2.09, 26.50, 23.11,
			     -3.44, -3.78, 23.20, 8.41};

// int e_puck_matrix[16] = {17,29,34,10,8,-38,-56,-76,-72,-58,-36,8,10,36,28,18}; // Maze
int e_puck_matrix[16] = {17,29,70,10,8,-80,-56,-76,-72,-58,-36,8,10,36,28,18};

double fit;
int msl, msr;			// Wheel speeds
float msl_w, msr_w;
int bmsl, bmsr, sum_sensors;	// Braitenberg parameters
int rob_nb;			// Robot number
float rob_x, rob_z, rob_theta;  // Robot position and orientation
int distances[NB_SENSORS];	// Array for the distance sensor readings
int robot_id_u, robot_id;	// Unique and normalized (between 0 and FLOCK_SIZE-1), robot ID	
float loc[FLOCK_SIZE][3];	// X, Z, Theta of all ROB_NB
float prev_loc[ROB_NB][3];	// Previous X, Z, Theta values
float speed[ROB_NB][2];	// Speeds calculated with Reynold's rules
int initialized[ROB_NB];	// != 0 if initial positions have been received
float migr[2];
double best_reynolds[DATASIZE1];	// Best reynolds weights found from the pso
double best_braiten[DATASIZE];   // Best braitenberg weights found from the pso
int max_sens;
int braiten;
int i,j,k;

void reset(void) {
    
    printf("Called the reset function \n");
    wb_robot_init();
    char text[4];
    int i;
    text[1]='s';
    text[3]='\0';
    for (i=0;i<NB_SENSOR;i++) {
        text[0]='p';
        text[2]='0'+i;
        ds[i] = wb_robot_get_device(text); // distance sensors
    }
    emitter = wb_robot_get_device("emitter_epuck");
    rec = wb_robot_get_device("receiver_epuck");
    left_motor = wb_robot_get_device("left wheel motor");
    right_motor = wb_robot_get_device("right wheel motor");
    wb_motor_set_position(left_motor, INFINITY);
    wb_motor_set_position(right_motor, INFINITY);
    char* robot_name; 
    robot_name=(char*) wb_robot_get_name(); 
    sscanf(robot_name,"epuck%d",&robot_id_u); // read robot id from the robot's name
    robot_id = robot_id_u%FLOCK_SIZE;
    for(i=0;i<NB_SENSOR;i++) {
        wb_distance_sensor_enable(ds[i],64);
        }

    for(i=0;i<ROB_NB;i++){
      initialized[i]  = 0;
    }
    wb_receiver_enable(rec,32);
    migr[0] = 0;
    migr[1] = 0;

}

// Generate random number from 0 to 1
double rnd() {
    return (double)rand()/RAND_MAX;
}

// Generate Gaussian random number with 0 mean and 1 std
double gauss(void) {
    double x1, x2, w;

    do {
        x1 = 2.0 * rnd() - 1.0;
        x2 = 2.0 * rnd() - 1.0;
        w = x1*x1 + x2*x2;
    } while (w >= 1.0);

    w = sqrt((-2.0 * log(w))/w);
    return(x1*w);
}

// S-function to transform v variable to [0,1]
double s(double v) {
    if (v > 5)
        return 1.0;
    else if (v < -5)
        return 0.0;
    else
        return 1.0/(1.0 + exp(-1*v));
}
void limitf(float *number, int limit) {
	if (*number > limit)
		*number = (float)limit;
	if (*number < -limit)
		*number = (float)-limit;
}

/*
 * Keep given int number within interval {-limit, limit}
 */
void limit(int *number, int limit) {
	if (*number > limit)
		*number = limit;
	if (*number < -limit)
		*number = -limit;
}

// Find the fitness for obstacle avoidance of the passed controller
double fitfunc(double weights[DATASIZE],int its) {

    printf("Weights in the fitfunc: \n");
    for(i=0;i<=DATASIZE;i++){
      printf("weight[%d] = %f\n",i,weights[i]);
    }
    double left_speed,right_speed; // Wheel speeds
    double old_left, old_right; // Previous wheel speeds (for recursion)
    //int left_encoder,right_encoder;
    double ds_value[NB_SENSOR];

    // Fitness variables
    double fit_speed;           // Speed aspect of fitness
    double fit_diff;            // Speed difference between wheels aspect of fitness
    double fit_sens;            // Proximity sensing aspect of fitness
    double sens_val[NB_SENSOR]; // Average values for each proximity sensor
    double fitness;             // Fitness of controller

    // Initially no fitness measurements
    fit_speed = 0.0;
    fit_diff = 0.0;
    for (i=0;i<NB_SENSOR;i++) {
        sens_val[i] = 0.0;
    }
    fit_sens = 0.0;
    old_left = 0.0;
    old_right = 0.0;

    // Evaluate fitness repeatedly
    for (j=0;j<its;j++) {
        if (braiten) j--;            // Loop forever

        ds_value[0] = (double) wb_distance_sensor_get_value(ds[0]);
        ds_value[1] = (double) wb_distance_sensor_get_value(ds[1]);
        ds_value[2] = (double) wb_distance_sensor_get_value(ds[2]);
        ds_value[3] = (double) wb_distance_sensor_get_value(ds[3]);
        ds_value[4] = (double) wb_distance_sensor_get_value(ds[4]);
        ds_value[5] = (double) wb_distance_sensor_get_value(ds[5]);
        ds_value[6] = (double) wb_distance_sensor_get_value(ds[6]);
        ds_value[7] = (double) wb_distance_sensor_get_value(ds[7]);

        // Feed proximity sensor values to neural net
        left_speed = 0.0;
        right_speed = 0.0;
        for (i=0;i<NB_SENSOR;i++) {
            left_speed += weights[i]*ds_value[i];
            right_speed += weights[i+NB_SENSOR+1]*ds_value[i];
        }
        left_speed /= 200.0;
        right_speed /= 200.0;

        // Add the recursive connections
        left_speed += weights[2*NB_SENSOR+2]*(old_left+MAX_SPEED)/(2*MAX_SPEED);
        left_speed += weights[2*NB_SENSOR+3]*(old_right+MAX_SPEED)/(2*MAX_SPEED);
        right_speed += weights[2*NB_SENSOR+4]*(old_left+MAX_SPEED)/(2*MAX_SPEED);
        right_speed += weights[2*NB_SENSOR+5]*(old_right+MAX_SPEED)/(2*MAX_SPEED);

        // Add neural thresholds
        left_speed += weights[NB_SENSOR];
        right_speed += weights[2*NB_SENSOR+1];
        // Apply neuron transform 
        left_speed = MAX_SPEED*(2.0*s(left_speed)-1.0);
        right_speed = MAX_SPEED*(2.0*s(right_speed)-1.0);

        // Make sure we don't accelerate too fast
        if (left_speed - old_left > MAX_ACC) left_speed = old_left+MAX_ACC;
        if (left_speed - old_left < -MAX_ACC) left_speed = old_left-MAX_ACC;
        if (right_speed - old_right > MAX_ACC) left_speed = old_right+MAX_ACC;
        if (right_speed - old_right < -MAX_ACC) left_speed = old_right-MAX_ACC;

        // Make sure speeds are within bounds
        if (left_speed > MAX_SPEED) left_speed = MAX_SPEED;
        if (left_speed < -1.0*MAX_SPEED) left_speed = -1.0*MAX_SPEED;
        if (right_speed > MAX_SPEED) right_speed = MAX_SPEED;
        if (right_speed < -1.0*MAX_SPEED) right_speed = -1.0*MAX_SPEED;

        // Set new old speeds
        old_left = left_speed;
        old_right = right_speed;

        /*left_encoder = wb_differential_wheels_get_left_encoder();
        right_encoder = wb_differential_wheels_get_right_encoder();
        if (left_encoder>9000) wb_differential_wheels_set_encoders(0,right_encoder);
        if (right_encoder>1000) wb_differential_wheels_set_encoders(left_encoder,0);*/
        // Set the motor speeds
        //wb_differential_wheels_set_speed((int)left_speed,(int)right_speed); 
        float msl_w = left_speed*MAX_SPEED_WEB/1000;
        float msr_w = right_speed*MAX_SPEED_WEB/1000; 
        wb_motor_set_velocity(left_motor, (int)msl_w);
        wb_motor_set_velocity(right_motor, (int)msr_w);
        wb_robot_step(128); // run one step

        // Get current fitness value

        // Average speed
        fit_speed += (fabs(left_speed) + fabs(right_speed))/(2.0*MAX_SPEED);
        // Difference in speed
        fit_diff += fabs(left_speed - right_speed)/MAX_DIFF;
        // Sensor values
        for (i=0;i<NB_SENSOR;i++) {
            sens_val[i] += ds_value[i]/MAX_SENS;
        }
    }

    // Find most active sensor
    for (i=0;i<NB_SENSOR;i++) {
        if (sens_val[i] > fit_sens) fit_sens = sens_val[i];
    }
    // Average values over all steps
    fit_speed /= its;
    fit_diff /= its;
    fit_sens /= its;
    
    printf("fit_speed = %f,fit_dif = %f,fit_sens = %f\n",fit_speed,fit_diff,fit_sens);

    // Better fitness should be higher
    fitness = fit_speed*(1.0 - sqrt(fit_diff))*(1.0 - fit_sens);

    return fitness;
}



float fitfunc1(double weights[DATASIZE1],int its) {

            double left_speed,right_speed; // Wheel speeds
            double old_left, old_right; // Previous wheel speeds (for recursion)
            //int left_encoder,right_encoder;
            double ds_value[NB_SENSOR];

            // Fitness variables
            double fit_speed;           // Speed aspect of fitness
            double fit_diff;            // Speed difference between wheels aspect of fitness
            double fit_sens;            // Proximity sensing aspect of fitness
            double fitness;  
	float avg_loc[2] = {0,0};	// Flock average positions
	float avg_speed[2] = {0,0};	// Flock average speeds
	float cohesion[2] = {0,0};
	float dispersion[2] = {0,0};
	float consistency[2] = {0,0};
	fit_speed = 0.0;
           fit_diff = 0.0;

           fit_sens = 0.0;
           old_left = 0.0;
           old_right = 0.0;

	for(int it=0;it<its;it++){
	/* Compute averages over the whole flock */
              	for(i=0; i<ROB_NB; i++) {
              		if (i == robot_id) 
              		{	
              			// don't consider yourself for the average 
              			continue;
              		}
                        	for (j=0;j<2;j++) 
              		{
              			avg_speed[j] += speed[i][j];
              			avg_loc[j] += loc[i][j];
              		}
              	}

                      for (j=0;j<2;j++) 
              	{

                        	avg_speed[j] /= ROB_NB - 1;
                        	avg_loc[j] /= ROB_NB-1;


                      }

              	/* Reynold's rules */

              	/* Rule 1 - Aggregation/Cohesion: move towards the center of mass */
              	for (j=0;j<2;j++) {
              		// If center of mass is too far
              		if (sqrt(pow(loc[robot_id][0]-avg_loc[0],2)+pow(loc[robot_id][1]-avg_loc[1],2)) > RULE1_THRESHOLD) 
              		{
                       		cohesion[j] = avg_loc[j] - loc[robot_id][j];   // Relative distance to the center of the swarm
              		}
              	}



              	/* Rule 2 - Dispersion/Separation: keep far enough from flockmates */
              	for (k=0;k<ROB_NB;k++) {
              		if (k != robot_id) {        // Loop on flockmates only
              			// If neighbor k is too close (Euclidean distance)
              			if (pow(loc[robot_id][0]-loc[k][0],2)+pow(loc[robot_id][1]-loc[k][1],2) < RULE2_THRESHOLD) 
              			{
              				for (j=0;j<2;j++) 
              				{
              					dispersion[j] += 1/(loc[robot_id][j] -loc[k][j]);	// Relative distance to k
              				}
              			}
              		}
              	}

              	/* Rule 3 - Consistency/Alignment: match the speeds of flockmates */
                      consistency[0] = 0;
                      consistency[1] = 0;
                      /* add code for consistency[j]*/
                      for (j=0;j<2;j++) 
                      {
                          consistency[j] = avg_speed[j] - speed[robot_id][j];
                      }

                      // aggregation of all behaviors with relative influence determined by weights
              //        printf("id = %d, cx = %f, cy = %f\n", robot_id, cohesion[0], cohesion[1]);
                      // if(robot_id == 0)
                      // printf("id = %d, %f %f, %f %f, %f %f\n", robot_id, cohesion[0], -cohesion[1], dispersion[0], -dispersion[1], consistency[0], -consistency[1]);

                      for (j=0;j<2;j++) 
              	{
                               speed[robot_id][j] = cohesion[j] * RULE1_WEIGHT*weights[0];
                               speed[robot_id][j] +=  dispersion[j] * RULE2_WEIGHT*weights[1];
                               speed[robot_id][j] +=  consistency[j] * RULE3_WEIGHT*weights[2]   ;
                       }
                      speed[robot_id][1] *= -1; //y axis of webots is inverted

                      left_speed = speed[robot_id][0];
                      right_speed = speed[robot_id][1];

                      left_speed += weights[REYNOLD_PARAMS+1]*(old_left+MAX_SPEED)/(2*MAX_SPEED);
                      left_speed += weights[REYNOLD_PARAMS+2]*(old_right+MAX_SPEED)/(2*MAX_SPEED);
                      right_speed += weights[REYNOLD_PARAMS+1]*(old_left+MAX_SPEED)/(2*MAX_SPEED);
                      right_speed += weights[REYNOLD_PARAMS+2]*(old_right+MAX_SPEED)/(2*MAX_SPEED);

                      left_speed += weights[REYNOLD_PARAMS];
                      right_speed += weights[2*REYNOLD_PARAMS];
                      // Apply neuron transform 
                      left_speed = MAX_SPEED*(2.0*s(left_speed)-1.0);
                      right_speed = MAX_SPEED*(2.0*s(right_speed)-1.0);

                      // Make sure we don't accelerate too fast
                      if (left_speed - old_left > MAX_ACC) left_speed = old_left+MAX_ACC;
                      if (left_speed - old_left < -MAX_ACC) left_speed = old_left-MAX_ACC;
                      if (right_speed - old_right > MAX_ACC) left_speed = old_right+MAX_ACC;
                      if (right_speed - old_right < -MAX_ACC) left_speed = old_right-MAX_ACC;

                      // Make sure speeds are within bounds
                      if (left_speed > MAX_SPEED) left_speed = MAX_SPEED;
                      if (left_speed < -1.0*MAX_SPEED) left_speed = -1.0*MAX_SPEED;
                      if (right_speed > MAX_SPEED) right_speed = MAX_SPEED;
                      if (right_speed < -1.0*MAX_SPEED) right_speed = -1.0*MAX_SPEED;

                      // Set new old speeds
                      old_left = left_speed;
                      old_right = right_speed;

                      left_speed += weights[2*NB_SENSOR+2]*(old_left+MAX_SPEED)/(2*MAX_SPEED);
                      left_speed += weights[2*NB_SENSOR+3]*(old_right+MAX_SPEED)/(2*MAX_SPEED);
                      right_speed += weights[2*NB_SENSOR+4]*(old_left+MAX_SPEED)/(2*MAX_SPEED);
                      right_speed += weights[2*NB_SENSOR+5]*(old_right+MAX_SPEED)/(2*MAX_SPEED);
                      float msl_w = left_speed*MAX_SPEED_WEB/1000;
                      float msr_w = right_speed*MAX_SPEED_WEB/1000; 
                      wb_motor_set_velocity(left_motor, (int)msl_w);
                      wb_motor_set_velocity(right_motor, (int)msr_w);
                      wb_robot_step(128); // run one step

                      // Get current fitness value

                      // Average speed
                      fit_speed += (fabs(left_speed) + fabs(right_speed))/(2.0*MAX_SPEED);
                      // Difference in speed
                      fit_diff += fabs(left_speed - right_speed)/MAX_DIFF;
        }
         // Average values over all steps
    fit_speed /= its;
    fit_diff /= its;

    /*Motivation for the fitness function
    1 - sqrt(fit_diff) encourages the robots to go straight, i.e,no turning
    fit_speed encourages the robots to approach the target with max speed
    */
    fitness = fit_speed*(1.0 - sqrt(fit_diff));

    return fitness;



}

void compute_wheel_speeds(int *msl, int *msr) 
{
	// Compute wanted position from Reynold's speed and current location
	//float x = speed[robot_id][0]*cosf(loc[robot_id][2]) - speed[robot_id][1]*sinf(loc[robot_id][2]); // x in robot coordinates
	//float z = -speed[robot_id][0]*sinf(loc[robot_id][2]) - speed[robot_id][1]*cosf(loc[robot_id][2]); // z in robot coordinates

	float x = speed[robot_id][0]*cosf(loc[robot_id][2]) + speed[robot_id][1]*sinf(loc[robot_id][2]); // x in robot coordinates
	float z = -speed[robot_id][0]*sinf(loc[robot_id][2]) + speed[robot_id][1]*cosf(loc[robot_id][2]); // z in robot coordinates
//	printf("id = %d, x = %f, y = %f\n", robot_id, x, z);
	float Ku = 0.2;   // Forward control coefficient
	float Kw = 1;  // Rotational control coefficient
	float range = sqrtf(x*x + z*z);	  // Distance to the wanted position
	float bearing = -atan2(x, z);	  // Orientation of the wanted position

	// Compute forward control
	float u = Ku*range*cosf(bearing);
	// Compute rotational control
	float w = Kw*bearing;

	// Convert to wheel speeds!

	*msl = (u - AXLE_LENGTH*w/2.0) * (1000.0 / WHEEL_RADIUS);
	*msr = (u + AXLE_LENGTH*w/2.0) * (1000.0 / WHEEL_RADIUS);
//	printf("bearing = %f, u = %f, w = %f, msl = %f, msr = %f\n", bearing, u, w, msl, msr);
	limit(msl,MAX_SPEED);
	limit(msr,MAX_SPEED);
}

void update_self_motion(int msl, int msr) {
	float theta = loc[robot_id][2];

	// Compute deltas of the robot
	float dr = (float)msr * SPEED_UNIT_RADS * WHEEL_RADIUS * DELTA_T;
	float dl = (float)msl * SPEED_UNIT_RADS * WHEEL_RADIUS * DELTA_T;
	float du = (dr + dl)/2.0;
	float dtheta = (dr - dl)/AXLE_LENGTH;

	// Compute deltas in the environment
	float dx = -du * sinf(theta);
	float dz = -du * cosf(theta);

	// Update position
	loc[robot_id][0] += dx;
	loc[robot_id][1] += dz;
	loc[robot_id][2] += dtheta;

	// Keep orientation within 0, 2pi
	if (loc[robot_id][2] > 2*M_PI) loc[robot_id][2] -= 2.0*M_PI;
	if (loc[robot_id][2] < 0) loc[robot_id][2] += 2.0*M_PI;
}


void reynolds_rules(bool with_pso,double buffer[DATASIZE1]) {
	int i, j, k;			// Loop counters
	float avg_loc[2] = {0,0};	// Flock average positions
	float avg_speed[2] = {0,0};	// Flock average speeds
	float cohesion[2] = {0,0};
	float dispersion[2] = {0,0};
	float consistency[2] = {0,0};
	
	/* Compute averages over the whole flock */
	for(i=0; i<FLOCK_SIZE; i++) {
		if (i == robot_id) 
		{	
			// don't consider yourself for the average 
			continue;
		}
            	for (j=0;j<2;j++)
            	{
                  	  avg_speed[j] += speed[i][j];
  		  avg_loc[j] += loc[i][j];
  		}
            }
	
        for (j=0;j<2;j++) 
	{
          	avg_speed[j] /= FLOCK_SIZE-1;
          	avg_loc[j] /= FLOCK_SIZE-1;

          	
        }
	
	/* Reynold's rules */
	
	/* Rule 1 - Aggregation/Cohesion: move towards the center of mass */
	for (j=0;j<2;j++) {
		// If center of mass is too far
		if (sqrt(pow(loc[robot_id][0]-avg_loc[0],2)+pow(loc[robot_id][1]-avg_loc[1],2)) > RULE1_THRESHOLD) 
		{
         		cohesion[j] = avg_loc[j] - loc[robot_id][j];   // Relative distance to the center of the swarm
		}
	}
	
  
  
	/* Rule 2 - Dispersion/Separation: keep far enough from flockmates */
	for (k=0;k<FLOCK_SIZE;k++) {
		if (k != robot_id) {        // Loop on flockmates only
			// If neighbor k is too close (Euclidean distance)
			if (pow(loc[robot_id][0]-loc[k][0],2)+pow(loc[robot_id][1]-loc[k][1],2) < RULE2_THRESHOLD) 
			{
				for (j=0;j<2;j++) 
				{
					dispersion[j] += 1/(loc[robot_id][j] -loc[k][j]);	// Relative distance to k
				}
			}
		}
	}
  
	/* Rule 3 - Consistency/Alignment: match the speeds of flockmates */
        consistency[0] = 0;
        consistency[1] = 0;
        /* add code for consistency[j]*/
        for (j=0;j<2;j++) 
        {
            consistency[j] = avg_speed[j] -speed[robot_id][j]; 
        }
  
        // aggregation of all behaviors with relative influence determined by weights
//        printf("id = %d, cx = %f, cy = %f\n", robot_id, cohesion[0], cohesion[1]);
        if(robot_id == 0)
        printf("id = %d, %f %f, %f %f, %f %f\n", robot_id, cohesion[0], -cohesion[1], dispersion[0], -dispersion[1], consistency[0], -consistency[1]);
        
        for (j=0;j<2;j++) 
  	{    if(!with_pso){
        	      // if no pso, apply the manually adjusted weights
                 speed[robot_id][j] = cohesion[j] * RULE1_WEIGHT;
                 speed[robot_id][j] +=  dispersion[j] * RULE2_WEIGHT;
                 speed[robot_id][j] +=  consistency[j] * RULE3_WEIGHT;
                 }
                 else{
                   // apply the pso adjusted weights
                   speed[robot_id][j] = cohesion[j] * buffer[0];
                   speed[robot_id][j] += dispersion[j] * buffer[1];
                   speed[robot_id][j] += consistency[j] * buffer[2]; 
                 }
         }
        speed[robot_id][1] *= -1; //y axis of webots is inverted
        
        //move the robot according to some migration rule
        if(MIGRATORY_URGE == 0){
          speed[robot_id][0] += 0*0.01*cos(loc[robot_id][2] + M_PI/2);
          speed[robot_id][1] += 0*0.01*sin(loc[robot_id][2] + M_PI/2);
        }
        else {
              printf("Inside migratory urge \n");
              printf("migr[0] = %f,migr[1] = %f\n",migr[0],migr[1]);
            /* Implement migratory urge */
              speed[robot_id][0] += MIGRATION_WEIGHT*(migr[0] -loc[robot_id][0]);
              speed[robot_id][1] -= MIGRATION_WEIGHT*(migr[1]-loc[robot_id][1]);
        }
}


void braiten_reynolds(bool with_pso){ 	
	     
	     if(with_pso){
  	       printf("Called braiten reynolds with pso \n");
	     }
	     else{
  	       printf("Called braiten reynolds without pso \n");
	     }
	     bmsl = 0; bmsr = 0;
                sum_sensors = 0;
                max_sens = 0;
                msl = 0; msr = 0;  
                max_sens = 0;
                double buffer[DATASIZE1] = {0,0,0};
		/* Braitenberg */
		for(i=0;i<NB_SENSORS;i++) 
		{
                            distances[i]=wb_distance_sensor_get_value(ds[i]); //Read sensor values
                            sum_sensors += distances[i]; // Add up sensor values
                            max_sens = max_sens>distances[i]?max_sens:distances[i]; // Check if new highest sensor value
                            
			    // Weighted sum of distance sensor values for Braitenberg vehicle
                            bmsr += e_puck_matrix[i] * distances[i];
                            bmsl += e_puck_matrix[i+NB_SENSORS] * distances[i];
                      }
                
		// Adapt Braitenberg values (empirical tests)
		bmsl/=MIN_SENS; bmsr/=MIN_SENS;
                      bmsl+=66; bmsr+=72;
              
		/* Get information */
		int count = 0;
		while (wb_receiver_get_queue_length(rec) > 0) 
		{
                      	    double* inbuffer = (double*) wb_receiver_get_data(rec);
                          // printf("inbuffer = %s\n",inbuffer);
                          if(!with_pso){
                            rob_nb = (int)inbuffer[1];
                            rob_x = inbuffer[2];
                            rob_z = inbuffer[3];
                            rob_theta = inbuffer[4];
                            }
                           else{
                            rob_nb = (int)inbuffer[1];
                            rob_x = inbuffer[2];
                            rob_z = inbuffer[3];
                            rob_theta = inbuffer[4];
                            for(int k=0;k<DATASIZE;k++){
                  	      buffer[k] = inbuffer[7+k];
                  	      }
                           }
                        printf("rob_nb = %d,rob_x = %lf,rob_z = %lf,rob_theta = %lf\n",rob_nb,rob_x,rob_z,rob_theta);
                        if ((int) rob_nb/FLOCK_SIZE == (int) robot_id/FLOCK_SIZE) {

			if (initialized[rob_nb] == 0) {
				// Get initial positions
				loc[rob_nb][0] = rob_x; //x-position
				loc[rob_nb][1] = rob_z; //z-position
				loc[rob_nb][2] = rob_theta; //theta
				prev_loc[rob_nb][0] = loc[rob_nb][0];
				prev_loc[rob_nb][1] = loc[rob_nb][1];
				initialized[rob_nb] = 1;
			} else {
				// Get position update
//				printf("\n got update robot[%d] = (%f,%f) \n",rob_nb,loc[rob_nb][0],loc[rob_nb][1]);
				prev_loc[rob_nb][0] = loc[rob_nb][0];
				prev_loc[rob_nb][1] = loc[rob_nb][1];
				loc[rob_nb][0] = rob_x; //x-position
				loc[rob_nb][1] = rob_z; //z-position
				loc[rob_nb][2] = rob_theta; //theta
			}
			
			speed[rob_nb][0] = (1/DELTA_T)*(loc[rob_nb][0]-prev_loc[rob_nb][0]);
			speed[rob_nb][1] = (1/DELTA_T)*(loc[rob_nb][1]-prev_loc[rob_nb][1]);
			count++;
			}
			wb_receiver_next_packet(rec);
		}
		
		
		// Compute self position & speed
		prev_loc[robot_id][0] = loc[robot_id][0];
		prev_loc[robot_id][1] = loc[robot_id][1];
		
		update_self_motion(msl,msr);
		
		speed[robot_id][0] = (1/DELTA_T)*(loc[robot_id][0]-prev_loc[robot_id][0]);
		speed[robot_id][1] = (1/DELTA_T)*(loc[robot_id][1]-prev_loc[robot_id][1]);
		// for(i=0;i<FLOCK_SIZE;i++){
    		  // for(int j=0;j<2;j++){
    		    // printf("speed[%d][%d] = %f\n",i,j,speed[i][j]);
    		  // }
		// }
    
		// Reynold's rules with all previous info (updates the speed[][] table)
		if(!with_pso){
		  reynolds_rules(with_pso,buffer);
		  }
		 else{
  		   printf("Weights inside braiten_reynolds are: \n");
  		   for(int k=0;k<DATASIZE1;k++){
    		      printf("buffer[%d] = %f\n",k,buffer[k]);
  		   }
  		   reynolds_rules(with_pso,buffer);
		 }
		//printf("%f %f\n", speed[robot_id][0], speed[robot_id][1]);
    
		// Compute wheels speed from Reynold's speed
		compute_wheel_speeds(&msl, &msr);
    
		// Adapt speed instinct to distance sensor values
		if (sum_sensors > NB_SENSORS*MIN_SENS) {
			msl -= msl*max_sens/(2*MAX_SENS);
			msr -= msr*max_sens/(2*MAX_SENS);
		}
    
		// Add Braitenberg
		msl += bmsl;
		msr += bmsr;
                  
		/*Webots 2018b*/
		// Set speed
		msl_w = msl*MAX_SPEED_WEB/1000;
		msr_w = msr*MAX_SPEED_WEB/1000;
		wb_motor_set_velocity(left_motor, msl_w);
                      wb_motor_set_velocity(right_motor, msr_w);
		//wb_differential_wheels_set_speed(msl,msr);
		/*Webots 2018b*/
    
		// Send current position to neighbors, uncomment for I15, don't forget to uncomment the declration of "outbuffer" at the begining of this function.
		/*Implement your code here*/

		// Continue one step
  		wb_robot_step(TIME_STEP);
  		
  		}  




//-------------MAIN------------//

enum States{PSO_BRAITEN,PSO_REYNOLDS,BRAITEN_REYNOLDS};


void pso_braiten(enum States state){
  
  printf("Called pso_braiten \n");
  double* rbuffer = (double*) wb_receiver_get_data(rec);

  for(i=1;i<DATASIZE+2;i++){
          rbuffer[i-1] = rbuffer[i];
          }

  double buffer[255];
  if (rbuffer[DATASIZE] == -1.0) {
            braiten = 1;
            fitfunc(good_w,100);


            // Otherwise, run provided controller
        } 
   else {

        // for(k=0;k<DATASIZE;k++){
          // good_w[k] = rbuffer[k];
        // }   
        if(state == PSO_BRAITEN){
          fit = fitfunc(rbuffer,rbuffer[DATASIZE]);
          }

        else if(state == PSO_REYNOLDS){
          fit = fitfunc1(rbuffer,rbuffer[DATASIZE1]);
        }
        for(int k=0;k<22;k++){
          good_w[k] = rbuffer[k];
         }

        // for(int k=0;k<DATASIZE;k++){
        // printf("rbuffer[%d] = %lf\n",k,rbuffer[k]);
        // }
        buffer[0] = fit;
        printf("Fit = %lf\n",fit);
        wb_emitter_send(emitter,(void *)buffer,sizeof(double));
        }  
}



int main() {
    
    reset();
    //wb_differential_wheels_enable_encoders(64);
    braiten = 0; // Don't run forever
    // enum States main_state = PSO_BRAITEN;
    enum States main_state;
    double* rbuffer;
    wb_robot_step(64);
    while (1) {
                // Wait for data
        while (wb_receiver_get_queue_length(rec) == 0) {
            wb_robot_step(64);
        }
        printf("Received data from supervisor\n");
        rbuffer = (double *)wb_receiver_get_data(rec);
        
        // double buffsize = (double)sizeof(rbuffer)/sizeof(double);
        // printf("size of buffer received from supervisor is %d \n",buffsize);
        int main_state = (int)rbuffer[0];
        printf("main_state = %d\n",main_state);
        
        // if(main_state == 10){
          // Send confirmation that the best weights have been received
          // double dummy_buf[1] = {1.0};
          // wb_emitter_send(emitter,(void *)dummy_buf,sizeof(double));
        // }
        
        if(main_state == 0){  
          pso_braiten(0);
        }
        else if(main_state == -1 || main_state == 1){
          // with pso
         
          braiten_reynolds(1);
          double dummy_buf[1] = {1.0};
          wb_emitter_send(emitter,(void *)dummy_buf,sizeof(double));
        }
        else if(main_state == -2 || main_state ==2){
          /* Note that we cannot have a FSM here, because if the robots
            only do braitenberg, they will leave the other robots stuck in obstacles behing.
            Otherwise, if they only do flocking they can run into obstacles
          */
          braiten_reynolds(0);
        }
        
        wb_receiver_next_packet(rec);

    }
  
    printf("Exiting main function\n");
    return 0;
}