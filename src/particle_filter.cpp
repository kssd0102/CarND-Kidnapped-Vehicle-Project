/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "helper_functions.h"

using std::string;
using std::vector;
static std::default_random_engine gen;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * TODO: Set the number of particles. Initialize all particles to 
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1. 
   * TODO: Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */  
  num_particles = 0;  // TODO: Set the number of particles

  std::normal_distribution<double> dist_x(0.0, std[0]);
  std::normal_distribution<double> dist_y(0.0, std[1]);
  std::normal_distribution<double> dist_theta(0.0, std[2]);

  for(int i = 0; i < num_particles; ++i){
    Particle p;
    p.x = x;
    p.y = y;
    p.theta = theta;
    p.id = i;
    p.weight = 1.0;
    //add noise
    p.x += dist_x(gen);
    p.y += dist_y(gen);
    p.theta += dist_theta(gen);
    
    particles.push_back(p);
  }
is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], 
                                double velocity, double yaw_rate) {
  /**
   * TODO: Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */
  std::normal_distribution<double> dist_x(0.0, std_pos[0]);
  std::normal_distribution<double> dist_y(0.0, std_pos[1]);
  std::normal_distribution<double> dist_theta(0.0, std_pos[2]);

  for(int i = 0; i < num_particles; i++){
    if(fabs(yaw_rate) < 0.00001){
      particles[i].x += velocity*delta_t*cos(particles[i].theta);
      particles[i].y += velocity*delta_t*sin(particles[i].theta);
    }
    else{
      particles[i].x = velocity / yaw_rate * (sin(particles[i].theta + yaw_rate*delta_t) - sin(particles[i].theta));
      particles[i].y = velocity / yaw_rate * (cos(particles[i].theta) - cos(particles[i].theta + yaw_rate*delta_t));     
    }
    particles[i].x += dist_x(gen);
    particles[i].y += dist_y(gen);
    particles[i].theta += dist_theta(gen);
  }
}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted, 
                                     vector<LandmarkObs>& observations) {
  /**
   * TODO: Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */
  for(int i = 0; i < observations.size(); i++){
    LandmarkObs o = observations[i];
    //init mimimun distance
    double min_dist = std::numeric_limits<double>::max();
    int map_id = -1;

    for(int j = 0; j < predicted.size(); j++){
      LandmarkObs p = predicted[i];
      double current_dist = dist(p.x, p.y, o.x, o.y);
      if(current_dist < min_dist){
        min_dist = current_dist;
        map_id = p.id;
      }
    }
  o.id = map_id;
  }

}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
                                   const vector<LandmarkObs> &observations, 
                                   const Map &map_landmarks) {
  /**
   * TODO: Update the weights of each particle using a mult-variate Gaussian 
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */
  double s_x = std_landmark[0];
  double s_y = std_landmark[1];

  for(int i = 0; i < num_particles; ++i){
    double p_x = particles[i].x;
    double p_y = particles[i].y;
    double p_theta = particles[i].theta;

    vector<LandmarkObs> predicted;

    for(int j = 0; j < map_landmarks.landmark_list.size(); ++j){
      double lm_x = map_landmarks.landmark_list[j].x_f;
      double lm_y = map_landmarks.landmark_list[j].y_f;
      int lm_id = map_landmarks.landmark_list[j].id_i;

      if(dist(p_x, p_y, lm_x, lm_y) < sensor_range){
        predicted.push_back(LandmarkObs{lm_id, lm_x, lm_y});
      }
    }

    vector<LandmarkObs> transformed_obs;

    for(int j = 0; j < observations.size(); ++j){
      double t_x = p_x + cos(p_theta)*observations[j].x - sin(p_theta)*observations[j].y;
      double t_y = p_y + sin(p_theta)*observations[j].x + cos(p_theta)*observations[j].y;
      transformed_obs.push_back(LandmarkObs{observations[j].id, t_x, t_y});
    }

    dataAssociation(predicted, transformed_obs);

    //reinit the weight
    particles[i].weight = 1.0;

    for(int k = 0; k < transformed_obs.size(); ++k){
      int idx = 0;
      while(transformed_obs[k].id != predicted[idx].id){
        idx ++;
      }
      double pr_x = predicted[idx].x;
      double pr_y = predicted[idx].y;
      double o_x = transformed_obs[k].x;
      double o_y = transformed_obs[k].y;
      particles[i].weight *= ( 1/(2*M_PI*s_x*s_y)) * exp(-(pow(pr_x-o_x,2)/(2*pow(s_x, 2)) + (pow(pr_y-o_y,2)/(2*pow(s_y, 2)))));
    }
  }
}

void ParticleFilter::resample() {
  /**
   * TODO: Resample particles with replacement with probability proportional 
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */
  std::vector<Particle> new_particles;
  std::vector<double> weights;
  for(int i = 0; i < num_particles; ++i){
    weights.push_back(particles[i].weight);
  }
  std::discrete_distribution<int> dist(weights.begin(), weights.end());

  for (int i=0; i < num_particles; i++) {
		new_particles.push_back(particles[dist(gen)]);
	}

	particles = new_particles;

}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association, 
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}