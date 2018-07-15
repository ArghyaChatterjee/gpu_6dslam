#ifndef __GPU_6DSLAM__
#define __GPU_6DSLAM__

#include <thrust/host_vector.h>
#include <thrust/device_vector.h>
#include <thrust/sort.h>
#include <thrust/extrema.h>

#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/point_representation.h>
#include <pcl/io/pcd_io.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/filter.h>
#include <pcl/features/normal_3d.h>
#include <pcl/registration/transforms.h>
#include <pcl/registration/ndt.h>
#include <pcl/console/parse.h>
#include <pcl/registration/icp.h>
#include <pcl/common/time.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl_conversions/pcl_conversions.h>

#include "custom_point_types.h"
#include "data_model.hpp"
#include "cudaWrapper.h"

class gpu6DSLAM
{
public:
	std::vector<pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> > vpc;
	std::vector<Eigen::Affine3f> vmtf;
	std::vector<Eigen::Affine3f> vmregistered;
	std::vector <std::string> cloud_ids;


	data_model tfModel;
	data_model processedDataModel;
	data_model registeredModel;

	std::string root_folder_name;
	boost::filesystem::path mainPath;
	boost::filesystem::path rawData;
	boost::filesystem::path processedData;
	bool use4DOF;
	float noise_removal_resolution;
	int noise_removal_number_of_points_in_bucket_threshold;
	float noise_removal_bounding_box_extension;

	float downsampling_resolution;

	float semantic_classification_normal_vectors_search_radius;
	float semantic_classification_curvature_threshold;
	float semantic_classification_ground_Z_coordinate_threshold;
	int semantic_classification_number_of_points_needed_for_plane_threshold;
	int semantic_classification_max_number_considered_in_INNER_bucket;
	int semantic_classification_max_number_considered_in_OUTER_bucket;
	float semantic_classification_bounding_box_extension;

	float slam_registerLastArrivedScan_distance_threshold;
	float slam_registerAll_distance_threshold;
	size_t slam_number_of_observations_threshold;

	float slam_search_radius_step1;
	float slam_bucket_size_step1;
	int slam_registerLastArrivedScan_number_of_iterations_step1;

	float slam_search_radius_step2;
	float slam_bucket_size_step2;
	int slam_registerLastArrivedScan_number_of_iterations_step2;

	float slam_search_radius_step3;
	float slam_bucket_size_step3;
	int slam_registerLastArrivedScan_number_of_iterations_step3;

	float slam_search_radius_register_all;
	float slam_bucket_size_step_register_all;

	int slam_registerAll_number_of_iterations_step1;
	int slam_registerAll_number_of_iterations_step2;
	int slam_registerAll_number_of_iterations_step3;

	float slam_bounding_box_extension;
	float slam_max_number_considered_in_INNER_bucket;
	float slam_max_number_considered_in_OUTER_bucket;

	float slam_observation_weight_plane;
	float slam_observation_weight_edge;
	float slam_observation_weight_ceiling;
	float slam_observation_weight_ground;

	float findBestYaw_start_angle;
	float findBestYaw_finish_angle;
	float findBestYaw_step_angle;
	float findBestYaw_bucket_size;
	float findBestYaw_bounding_box_extension;
	float findBestYaw_search_radius;
	float findBestYaw_max_number_considered_in_INNER_bucket;
	float findBestYaw_max_number_considered_in_OUTER_bucket;

	float viewpointX;
	float viewpointY;
	float viewpointZ;

	int cudaDevice;

	gpu6DSLAM(){};
	gpu6DSLAM(std::string _root_folder_name)
	{
		this->use4DOF = true;
		this->root_folder_name = _root_folder_name;

		mainPath = this->root_folder_name;
		std::cout <<"creating directory (if does not exist):" << mainPath << std::endl;
		if(!boost::filesystem::create_directories(mainPath))
		{
			if(!boost::filesystem::is_directory(mainPath))
			{
				std::cout<<"Could not create dir: "<< mainPath << std::endl;
			}
		}

		rawData = mainPath;
		rawData/=("rawData");
		std::cout <<"creating directory (if does not exist):" << rawData << std::endl;
		if(!boost::filesystem::create_directories(rawData))
		{
			if(!boost::filesystem::is_directory(rawData))
			{
				std::cout<<"Could not create dir: "<< rawData << std::endl;
			}
		}

		processedData = mainPath;
		processedData/=("processedData");
		std::cout <<"creating directory (if does not exist):" << processedData << std::endl;
		if(!boost::filesystem::create_directories(processedData))
		{
			if(!boost::filesystem::is_directory(processedData))
			{
				std::cout<<"Could not create dir: "<< processedData << std::endl;
			}
		}

		boost::filesystem::path photosData = mainPath;
		photosData/=("photos");
		std::cout <<"creating directory (if does not exist):" << photosData << std::endl;
		if(!boost::filesystem::create_directories(photosData))
		{
			if(!boost::filesystem::is_directory(photosData))
			{
				std::cout<<"Could not create dir: "<< photosData << std::endl;
			}
		}

		this->tfModel.setAlgorithmName("localisation from tf");
		this->tfModel.setDataSetPath("rawData");

		this->processedDataModel.setAlgorithmName("processed data: 1: noise removal, 2: downsampling, 3: semantic classification");
		this->processedDataModel.setDataSetPath("processedData");

		this->registeredModel.setAlgorithmName("registration: semantic point to point");
		this->registeredModel.setDataSetPath("processedData");

		this->noise_removal_resolution = 0.5f;
		this->noise_removal_number_of_points_in_bucket_threshold = 3;
		this->noise_removal_bounding_box_extension = 1.0f;

		this->downsampling_resolution = 0.3f;


		this->semantic_classification_normal_vectors_search_radius = 1.0f;
		this->semantic_classification_curvature_threshold = 10.0;
		this->semantic_classification_ground_Z_coordinate_threshold = 1.0f;
		this->semantic_classification_number_of_points_needed_for_plane_threshold = 15;
		this->semantic_classification_max_number_considered_in_INNER_bucket = 100;
		this->semantic_classification_max_number_considered_in_OUTER_bucket = 100;
		this->semantic_classification_bounding_box_extension = 1.0f;

		this->slam_registerLastArrivedScan_distance_threshold = 100.0f;
		this->slam_registerAll_distance_threshold = 10.0f;

		this->slam_number_of_observations_threshold = 100;

		this->slam_search_radius_step1 = 2.5f;
		this->slam_bucket_size_step1 = 2.5f;
		this->slam_registerLastArrivedScan_number_of_iterations_step1 = 30.0f;

		this->slam_search_radius_step2 = 2.0f;
		this->slam_bucket_size_step2 = 2.0f;
		this->slam_registerLastArrivedScan_number_of_iterations_step2 = 30.0f;

		this->slam_search_radius_step3 = 1.0f;
		this->slam_bucket_size_step3 = 1.0f;
		this->slam_registerLastArrivedScan_number_of_iterations_step3 = 30.0f;

		this->slam_registerAll_number_of_iterations_step1 = 10;
		this->slam_registerAll_number_of_iterations_step2 = 10;
		this->slam_registerAll_number_of_iterations_step3 = 10;

		this->slam_search_radius_register_all = 0.5f;
		this->slam_bucket_size_step_register_all = 0.5f;

		this->slam_bounding_box_extension = 1.0f;
		this->slam_max_number_considered_in_INNER_bucket = 100.0f;
		this->slam_max_number_considered_in_OUTER_bucket = 100.0f;

		this->slam_observation_weight_plane = 10.0f;
		this->slam_observation_weight_edge = 1.0f;
		this->slam_observation_weight_ceiling = 10.0f;
		this->slam_observation_weight_ground = 10.0f;

		this->findBestYaw_start_angle = -30.0f;
		this->findBestYaw_finish_angle = 30.0f;
		this->findBestYaw_step_angle = 0.5f;
		this->findBestYaw_bucket_size = 1.0f;
		this->findBestYaw_bounding_box_extension = 1.0f;
		this->findBestYaw_search_radius = 0.3f;
		this->findBestYaw_max_number_considered_in_INNER_bucket = 50;
		this->findBestYaw_max_number_considered_in_OUTER_bucket = 50;

		this->viewpointX = 0.0f;
		this->viewpointY = 0.0f;
		this->viewpointZ = 2.0f;

		this->cudaDevice = 0;

	};
	~gpu6DSLAM(){};

	void registerSingleScan(pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> pc, Eigen::Affine3f mtf, std::string iso_time_str);
	pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> getMetascan(Eigen::Affine3f m);
	pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> getMetascan();

	void registerLastArrivedScan(CCudaWrapper &cudaWrapper, float slam_search_radius, float slam_bucket_size);

	void registerAll(CCudaWrapper &cudaWrapper, float slam_search_radius, float slam_bucket_size, size_t number_of_last_EOZ);
	void registerAll();

	void transformPointCloud(pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> &pointcloud, Eigen::Affine3f transform);

	bool loadmapfromfile(std::string map_filename);

	void callbackInitialPose(Eigen::Affine3f initial_pose);

	void downsample(pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> &_pc, float grid_res);
};

#endif
