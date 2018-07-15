#include "gpu6DSLAM.h"


void gpu6DSLAM::registerSingleScan(pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> pc, Eigen::Affine3f mtf, std::string iso_time_str)
{
	static Eigen::Affine3f last_mtf = mtf;

	Eigen::Affine3f odometryIncrement = last_mtf.inverse() * mtf;
	//last_mRegistration = last_mRegistration * odometryIncrement;


	CCudaWrapper cudaWrapper;
	cudaWrapper.warmUpGPU(this->cudaDevice);

	// std::cout << "gpu6DSLAM::registerSingleScan" << std::endl;

	std::string scanName = std::string("scan_") + iso_time_str;
	std::string scanPCDFileName = scanName + std::string(".pcd");

	boost::filesystem::path tfModelFileName = mainPath;
	tfModelFileName/=(std::string("tfModel_" + iso_time_str + ".xml"));
	// std::cout << "tfModelFileName: " << tfModelFileName << std::endl;

	boost::filesystem::path processedDataModelFileName = mainPath;
	processedDataModelFileName/=(std::string("tfModelProcessedData_" + iso_time_str + ".xml"));
	// std::cout << "processedDataModelFileName: " << processedDataModelFileName << std::endl;

	boost::filesystem::path registeredDataModelFileName = mainPath;
	registeredDataModelFileName/=(std::string("registeredData_" + iso_time_str + ".xml"));
	// std::cout << "registeredDataModelFileName: " << registeredDataModelFileName << std::endl;


	boost::filesystem::path rawDataFileName = rawData;
	rawDataFileName/=scanPCDFileName;

	boost::filesystem::path processedDataFileName = processedData;
	processedDataFileName/=scanPCDFileName;

	//save raw data
	// std::cout << "raw scan: " << scanName << " will be saved in: " << rawDataFileName << std::endl;
	if(pcl::io::savePCDFileBinary(rawDataFileName.string(), pc) == -1)
	{
		std::cout << "ERROR: problem with saving file: " << rawDataFileName << std::endl;
	}

	//cut off
	pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> pctemp;
	pctemp.reserve(pc.size());

	for (size_t i =0; i<pc.size(); i++ )
	{
		if ((pc[i].z < 15 && pc[i].z >-1)&&(pc[i].x*pc[i].x+pc[i].y*pc[i].y > 1.5))
		{
			pctemp.push_back(pc[i]);
		}
	}
	pc = pctemp;


	//pc processing
	//noise removal
	ROS_INFO("Starting cudaWrapper.removeNoiseNaive, pc.size : %d", pc.size());
	cudaWrapper.removeNoiseNaive(pc,
			this->noise_removal_resolution,
			this->noise_removal_bounding_box_extension,
			this->noise_removal_number_of_points_in_bucket_threshold);

	//downsampling

	ROS_INFO("Starting cudaWrapper.downsampling, pc.size : %d", pc.size());
	cudaWrapper.downsampling(pc, this->downsampling_resolution, this->downsampling_resolution);

	//semantic classification
	ROS_INFO("Starting cudaWrapper.classify, pc.size : %d", pc.size());
	cudaWrapper.classify( pc,
			this->semantic_classification_normal_vectors_search_radius,
			this->semantic_classification_curvature_threshold,
			this->semantic_classification_ground_Z_coordinate_threshold,
			this->semantic_classification_number_of_points_needed_for_plane_threshold,
			this->semantic_classification_bounding_box_extension,
			this->semantic_classification_max_number_considered_in_INNER_bucket,
			this->semantic_classification_max_number_considered_in_OUTER_bucket,
			this->viewpointX,
			this->viewpointY,
			this->viewpointZ) ;

	//save processed data
	// std::cout << "processed scan: " << processedDataFileName << " will be saved in: " << processedDataFileName << std::endl;
	ROS_INFO("Starting pcl::io::savePCDFileBinary, pc.size : %d", pc.size());
	if(pcl::io::savePCDFileBinary(processedDataFileName.string(), pc) == -1)
	{
		std::cout << "ERROR: problem with saving file: " << processedDataFileName << std::endl;
	}

	if(vpc.size() == 0)
	{
		this->vpc.push_back(pc);
		this->cloud_ids.push_back(scanName);
		this->vmtf.push_back(mtf);
		this->vmregistered.push_back(mtf);
	}else
	{
		this->vpc.push_back(pc);
		this->cloud_ids.push_back(scanName);
		Eigen::Affine3f m = this->vmtf[this->vmtf.size()-1] * odometryIncrement;
		this->vmtf.push_back(m);

		Eigen::Affine3f mr = this->vmregistered[this->vmregistered.size()-1] * odometryIncrement;
		this->vmregistered.push_back(mr);

		//
		Eigen::Affine3f mLastInv = this->vmregistered[this->vmregistered.size()-1].inverse();

		for( size_t i = 0; i < this->vmregistered.size(); i++) {
			this->vmregistered[i] = mLastInv * this->vmregistered[i];
		}

		for( size_t i = 0; i < this->vmregistered.size(); i++) {
			this->vmregistered[i] = mtf * this->vmregistered[i];
		}

		//

		pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> pc_before;
		for(size_t i = 0 ;i < vpc.size(); i++)
		{
			pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> pc_before_local = vpc[i];
			transformPointCloud(pc_before_local, this->vmregistered[i]);
			pc_before += pc_before_local;
		}
		// pcl::io::savePCDFileBinary(std::string("/tmp/pc_before_")+iso_time_str + std::string(".pcd"), pc_before);



		/*Eigen::Affine3f mBestYaw = Eigen::Affine3f::Identity();

		cudaWrapper.findBestYaw(
				this->vpc[this->vpc.size()-2],
				this->vmregistered[this->vmregistered.size()-2],
				this->vpc[this->vpc.size()-1],
				this->vmregistered[this->vmregistered.size()-1],
				this->findBestYaw_bucket_size,
				this->findBestYaw_bounding_box_extension,
				this->findBestYaw_search_radius,
				this->findBestYaw_max_number_considered_in_INNER_bucket,
				this->findBestYaw_max_number_considered_in_OUTER_bucket,
				this->findBestYaw_start_angle,
				this->findBestYaw_finish_angle,
				this->findBestYaw_step_angle,
				mBestYaw);

		//std::cout << mBestYaw.matrix() << std::endl;

		this->vmregistered[this->vmregistered.size()-1] = this->vmregistered[this->vmregistered.size()-1] * mBestYaw;
		 */

		// std::cout << "registerLastArrivedScan START" << std::endl;
		ROS_INFO("Starting registerLastArrivedScan - step 1");
		for(int i = 0 ; i < this->slam_registerLastArrivedScan_number_of_iterations_step1; i++)
		{
			registerLastArrivedScan(cudaWrapper, this->slam_search_radius_step1, this->slam_bucket_size_step1);
		}
		ROS_INFO("Starting registerLastArrivedScan - step 2");
		for(int i = 0 ; i < this->slam_registerLastArrivedScan_number_of_iterations_step2; i++)
		{
			registerLastArrivedScan(cudaWrapper, this->slam_search_radius_step2, this->slam_bucket_size_step2);
		}
		ROS_INFO("Starting registerLastArrivedScan - step 3");
		for(int i = 0 ; i < this->slam_registerLastArrivedScan_number_of_iterations_step3; i++)
		{
			registerLastArrivedScan(cudaWrapper, this->slam_search_radius_step3, this->slam_bucket_size_step3);
		}
		ROS_INFO("Starting slam_registerAll - step 1");
		for(int i = 0 ; i < this->slam_registerAll_number_of_iterations_step1; i++)
		{
			registerAll(cudaWrapper, this->slam_search_radius_step1, this->slam_bucket_size_step1, 3);
		}
		ROS_INFO("Starting slam_registerAll - step 2");
		for(int i = 0 ; i < this->slam_registerAll_number_of_iterations_step2; i++)
		{
			registerAll(cudaWrapper, this->slam_search_radius_step2, this->slam_bucket_size_step2, 3);
		}
		ROS_INFO("Starting slam_registerAll - step 3");
		for(int i = 0 ; i < this->slam_registerAll_number_of_iterations_step3; i++)
		{
			registerAll(cudaWrapper, this->slam_search_radius_step3, this->slam_bucket_size_step3, 3);
		}

		pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> pc_after;
		for(size_t i = 0 ;i < vpc.size(); i++)
		{
			pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> pc_before_local = vpc[i];
			transformPointCloud(pc_before_local, this->vmregistered[i]);
			pc_after += pc_before_local;
		}
		ROS_INFO("Done");
		// pcl::io::savePCDFileBinary(std::string("/tmp/pc_after_")+iso_time_str + std::string(".pcd"), pc_after);


		// std::cout << "registerLastArrivedScan FINISHED" << std::endl;
	}

	this->tfModel.setAffine(cloud_ids[cloud_ids.size()-1], mtf.matrix());
	this->tfModel.setPointcloudName(cloud_ids[cloud_ids.size()-1], scanPCDFileName);
	this->tfModel.saveFile(tfModelFileName.string());

	this->processedDataModel.setAffine(cloud_ids[cloud_ids.size()-1], mtf.matrix());
	this->processedDataModel.setPointcloudName(cloud_ids[cloud_ids.size()-1], scanPCDFileName);
	this->processedDataModel.saveFile(processedDataModelFileName.string());

	this->registeredModel.setPointcloudName(cloud_ids[cloud_ids.size()-1], scanPCDFileName);
	for(size_t i = 0 ; i < this->vmregistered.size(); i++)
	{
		this->registeredModel.setAffine(cloud_ids[i], vmregistered[i].matrix());
	}
	this->registeredModel.saveFile(registeredDataModelFileName.string());


	last_mtf = mtf;

	return;
}


pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> gpu6DSLAM::getMetascan(Eigen::Affine3f m)
{
	// std::cout << "gpu6DSLAM::getMetascan" << std::endl;
	// printf("gpu6DSLAM::getMetascan(Eigen::Affine3f m) vpc.size() %u\n", vpc.size());

	//Eigen::Affine3f mLastInv = this->vmregistered[this->vmregistered.size()-1].inverse();

	pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> metascan;

	for(size_t i = 0; i < vpc.size(); i++)
	{
		pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> pc = vpc[i];
		transformPointCloud(pc, vmregistered[i]);
		metascan += pc;
	}

	//transformPointCloud(metascan, mLastInv);
	//transformPointCloud(metascan, m);

	return metascan;
}

pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> gpu6DSLAM::getMetascan()
{
	// std::cout << "gpu6DSLAM::getMetascan" << std::endl;
	// printf("gpu6DSLAM::getMetascan() vpc.size() %u\n", vpc.size());

	pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> metascan;

	for(size_t i = 0; i < vpc.size(); i++)
	{
		pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> pc = vpc[i];
		transformPointCloud(pc, vmregistered[i]);
		metascan += pc;
	}

	return metascan;
}

void gpu6DSLAM::registerLastArrivedScan(CCudaWrapper &cudaWrapper, float slam_search_radius, float slam_bucket_size)
{
	//this->slam_registerLastArrivedScan_distance_threshold = 10.0f;

	//std::vector<Eigen::Affine3f> v_poses;

	size_t i = vmregistered.size() - 1;

	Eigen::Vector3f omfika1;
	Eigen::Vector3f xyz1;
	Eigen::Affine3f pose1;

	cudaWrapper.Matrix4ToEuler(vmregistered[i], omfika1, xyz1);
	cudaWrapper.EulerToMatrix(omfika1, xyz1, pose1);

	pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> _point_cloud_1 = vpc[i];
	transformPointCloud(_point_cloud_1, pose1);

	observations_t obs;
	obs.om = omfika1.x();
	obs.fi = omfika1.y();
	obs.ka = omfika1.z();
	obs.tx = xyz1.x();
	obs.ty = xyz1.y();
	obs.tz = xyz1.z();

	for(size_t j = i-1; j < i; j++)
	{
		ROS_INFO("registerLastArrivedScan node: %d of %d", j, i-1);

		Eigen::Vector3f omfika2;
		Eigen::Vector3f xyz2;
		Eigen::Affine3f pose2;
		cudaWrapper.Matrix4ToEuler(vmregistered[j], omfika2, xyz2);
		cudaWrapper.EulerToMatrix(omfika2, xyz2, pose2);

		float dist = sqrtf(  (xyz1.x() - xyz2.x()) * (xyz1.x() - xyz2.x()) +
							(xyz1.y() - xyz2.y()) * (xyz1.y() - xyz2.y()) +
							(xyz1.z() - xyz2.z()) * (xyz1.z() - xyz2.z()) );

		// std::cout << "dist: " << dist << std::endl;

		if(dist < this->slam_registerLastArrivedScan_distance_threshold)
		{
			pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> _point_cloud_2 = vpc[j];
			transformPointCloud(_point_cloud_2, pose2);

			std::vector<int> nearest_neighbour_indexes;
			nearest_neighbour_indexes.resize(_point_cloud_2.size());
			std::fill(nearest_neighbour_indexes.begin(), nearest_neighbour_indexes.end(), -1);

			cudaWrapper.semanticNearestNeighbourhoodSearch(
									_point_cloud_1,
									_point_cloud_2,
									slam_search_radius,
									slam_bucket_size,
									this->slam_bounding_box_extension,
									this->slam_max_number_considered_in_INNER_bucket,
									this->slam_max_number_considered_in_OUTER_bucket,
									nearest_neighbour_indexes);

			int number_of_observations_plane = 0;
			int number_of_observations_edge = 0;
			int number_of_observations_ceiling = 0;
			int number_of_observations_ground = 0;

			for(size_t ii = 0 ; ii < nearest_neighbour_indexes.size(); ii++)
			{
				if(nearest_neighbour_indexes[ii] != -1)
				{
					switch (_point_cloud_2[ii].label)
					{
						case LABEL_PLANE:
						{
							number_of_observations_plane ++;
							break;
						}
						case LABEL_EDGE:
						{
							number_of_observations_edge ++;
							break;
						}
						case LABEL_CEILING:
						{
							number_of_observations_ceiling ++;
							break;
						}
						case LABEL_GROUND:
						{
							number_of_observations_ground ++;
							break;
						}
					}
				}
			}


			for(size_t ii = 0 ; ii < nearest_neighbour_indexes.size(); ii++)
			{
				if(nearest_neighbour_indexes[ii] != -1)
				{
					pcl::PointXYZ p1(_point_cloud_1[nearest_neighbour_indexes[ii]].x, _point_cloud_1[nearest_neighbour_indexes[ii]].y, _point_cloud_1[nearest_neighbour_indexes[ii]].z);
					pcl::PointXYZ p2(_point_cloud_2[ii].x, _point_cloud_2[ii].y, _point_cloud_2[ii].z);

					obs_nn_t obs_nn;
					obs_nn.x0 = vpc[i].points[nearest_neighbour_indexes[ii]].x;
					obs_nn.y0 = vpc[i].points[nearest_neighbour_indexes[ii]].y;
					obs_nn.z0 = vpc[i].points[nearest_neighbour_indexes[ii]].z;
					obs_nn.x_diff = p1.x - p2.x;
					obs_nn.y_diff = p1.y - p2.y;
					obs_nn.z_diff = p1.z - p2.z;
					switch(_point_cloud_2[ii].label)
					{
						case 0://plane
						{
							obs_nn.P = slam_observation_weight_plane/number_of_observations_plane;
							break;
						}
						case 1: //edge
						{
							obs_nn.P = slam_observation_weight_edge/number_of_observations_edge;
							break;
						}
						case 2: //ceiling
						{
							obs_nn.P = slam_observation_weight_ceiling/number_of_observations_ceiling;
							break;
						}
						case 3: //floor/ground
						{
							obs_nn.P = slam_observation_weight_ground/number_of_observations_ground;
							break;
						}
					}
					obs.vobs_nn.push_back(obs_nn);
				}
			}

			// std::cout << "obs.vobs_nn.size(): " << obs.vobs_nn.size() << std::endl;

			if(obs.vobs_nn.size() > this->slam_number_of_observations_threshold )
			{
			//std::cout << "obs.vobs_nn.size(): " << obs.vobs_nn.size() << std::endl;
				bool registered=false;
				if(use4DOF )
				{
					registered = cudaWrapper.registerLS_4DOF(obs);
					
				}
				else
				{
					registered = cudaWrapper.registerLS(obs);
				}
				if (registered)
				{
					Eigen::Vector3f omfika1_res(obs.om, obs.fi, obs.ka);
					Eigen::Vector3f xyz1_res(obs.tx, obs.ty, obs.tz);
					Eigen::Affine3f pose1_res;
					cudaWrapper.EulerToMatrix(omfika1_res, xyz1_res, pose1_res);
					vmregistered[i] = pose1_res;	
				}
			}
			else
			{
				// std::cout << "WARNING: obs.vobs_nn.size() < this->slam_number_of_observations_threshold" << std::endl;
			}
		}
	}
}

void gpu6DSLAM::registerAll(CCudaWrapper &cudaWrapper, float slam_search_radius, float slam_bucket_size, size_t number_of_last_EOZ)
{
	std::vector<Eigen::Affine3f> v_poses;

	if(vpc.size() < number_of_last_EOZ)return;

	for(size_t i = 0 ; i < vpc.size() - number_of_last_EOZ ;i++)v_poses.push_back(vmregistered[i]);

	for(size_t i = vpc.size() - number_of_last_EOZ; i < vpc.size(); i++)
	{
		// std::cout << "registerAll node: " << i << " of: " << vpc.size() - 1 << std::endl;

		ROS_INFO("registerAll node: %d of %d", i, vpc.size() - 1);

		Eigen::Vector3f omfika1;
		Eigen::Vector3f xyz1;
		Eigen::Affine3f pose1;

		cudaWrapper.Matrix4ToEuler(vmregistered[i], omfika1, xyz1);
		cudaWrapper.EulerToMatrix(omfika1, xyz1, pose1);

		pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> _point_cloud_1 = vpc[i];
		transformPointCloud(_point_cloud_1, pose1);

		observations_t obs;
		obs.om = omfika1.x();
		obs.fi = omfika1.y();
		obs.ka = omfika1.z();
		obs.tx = xyz1.x();
		obs.ty = xyz1.y();
		obs.tz = xyz1.z();

		for(size_t j = 0; j < vpc.size(); j++)
		{
			if(i != j)
			{
				Eigen::Vector3f omfika2;
				Eigen::Vector3f xyz2;
				Eigen::Affine3f pose2;
				cudaWrapper.Matrix4ToEuler(vmregistered[j], omfika2, xyz2);
				cudaWrapper.EulerToMatrix(omfika2, xyz2, pose2);


				float dist = sqrt(  (xyz1.x() - xyz2.x()) * (xyz1.x() - xyz2.x()) +
									(xyz1.y() - xyz2.y()) * (xyz1.y() - xyz2.y()) +
									(xyz1.z() - xyz2.z()) * (xyz1.z() - xyz2.z()) );

				if(dist < this->slam_registerAll_distance_threshold)
				{
					pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> _point_cloud_2 = vpc[j];
					transformPointCloud(_point_cloud_2, pose2);

					std::vector<int> nearest_neighbour_indexes;
					nearest_neighbour_indexes.resize(_point_cloud_2.size());
					std::fill(nearest_neighbour_indexes.begin(), nearest_neighbour_indexes.end(), -1);

					cudaWrapper.semanticNearestNeighbourhoodSearch(
							_point_cloud_1,
							_point_cloud_2,
							slam_search_radius,
							slam_bucket_size,
							this->slam_bounding_box_extension,
							this->slam_max_number_considered_in_INNER_bucket,
							this->slam_max_number_considered_in_OUTER_bucket,
							nearest_neighbour_indexes);



					int number_of_observations_plane = 0;
					int number_of_observations_edge = 0;
					int number_of_observations_ceiling = 0;
					int number_of_observations_ground = 0;

					for(size_t ii = 0 ; ii < nearest_neighbour_indexes.size(); ii++)
					{
						if(nearest_neighbour_indexes[ii] != -1)
						{
							switch (_point_cloud_2[ii].label)
							{
								case LABEL_PLANE:
								{
									number_of_observations_plane ++;
									break;
								}
								case LABEL_EDGE:
								{
									number_of_observations_edge ++;
									break;
								}
								case LABEL_CEILING:
								{
									number_of_observations_ceiling ++;
									break;
								}
								case LABEL_GROUND:
								{
									number_of_observations_ground ++;
									break;
								}
							}
						}
					}


					for(size_t ii = 0 ; ii < nearest_neighbour_indexes.size(); ii++)
					{
						if(nearest_neighbour_indexes[ii] != -1)
						{
							pcl::PointXYZ p1(_point_cloud_1[nearest_neighbour_indexes[ii]].x, _point_cloud_1[nearest_neighbour_indexes[ii]].y, _point_cloud_1[nearest_neighbour_indexes[ii]].z);
							pcl::PointXYZ p2(_point_cloud_2[ii].x, _point_cloud_2[ii].y, _point_cloud_2[ii].z);

							obs_nn_t obs_nn;
							obs_nn.x0 = vpc[i].points[nearest_neighbour_indexes[ii]].x;
							obs_nn.y0 = vpc[i].points[nearest_neighbour_indexes[ii]].y;
							obs_nn.z0 = vpc[i].points[nearest_neighbour_indexes[ii]].z;
							obs_nn.x_diff = p1.x - p2.x;
							obs_nn.y_diff = p1.y - p2.y;
							obs_nn.z_diff = p1.z - p2.z;
							switch(_point_cloud_2[ii].label)
							{
								case 0://plane
								{
									obs_nn.P = slam_observation_weight_plane/number_of_observations_plane;
									break;
								}
								case 1: //edge
								{
									obs_nn.P = slam_observation_weight_edge/number_of_observations_edge;
									break;
								}
								case 2: //ceiling
								{
									obs_nn.P = slam_observation_weight_ceiling/number_of_observations_ceiling;
									break;
								}
								case 3: //floor/ground
								{
									obs_nn.P = slam_observation_weight_ground/number_of_observations_ground;
									break;
								}
							}
							obs.vobs_nn.push_back(obs_nn);
						}
					}
				}
			}//if(i != j)
		}//for(size_t j = 0; j < vpointcloud.size(); j++)

		// std::cout << "obs.vobs_nn.size() " << obs.vobs_nn.size() << std::endl;

		if(obs.vobs_nn.size() > this->slam_number_of_observations_threshold )
		{
			//if(!cudaWrapper.registerLS(obs))
			if(!cudaWrapper.registerLS_4DOF(obs))
			{
				std::cout << "PROBLEM: cudaWrapper.registerLS(obs2to1)" << std::endl;
				//do nothing
			}
		}else
		{
			std::cout << "WARNING: obs.vobs_nn.size() < this->slam_number_of_observations_threshold" << std::endl;
			//std::cout << "WARNING: obs.vobs_nn.size(): " << obs.vobs_nn.size() << std::endl;
		}

		Eigen::Vector3f omfika1_res(obs.om, obs.fi, obs.ka);
		Eigen::Vector3f xyz1_res(obs.tx, obs.ty, obs.tz);
		Eigen::Affine3f pose1_res;
		cudaWrapper.EulerToMatrix(omfika1_res, xyz1_res, pose1_res);

		// std::cout << pose1_res.matrix() << std::endl;

		v_poses.push_back(pose1_res);
	}

	vmregistered = v_poses;
}

void gpu6DSLAM::registerAll()
{
	// std::cout << "this->cudaDevice: " << this->cudaDevice << std::endl;
	/*Eigen::Vector3f omfika (0.1,0,0);
	Eigen::Vector3f xyz(0,0,0);
	Eigen::Affine3f m;

	CCudaWrapper::EulerToMatrix(omfika, xyz, m);

	for( size_t i = 0; i < this->vmregistered.size(); i++) {
		this->vmregistered[i] = m * this->vmregistered[i];
	}*/


	/*Eigen::Affine3f mLastInv = this->vmregistered[this->vmregistered.size()-1].inverse();

			for( size_t i = 0; i < this->vmregistered.size(); i++) {
				this->vmregistered[i] = this->vmregistered[i] * mLastInv;
			}

			for( size_t i = 0; i < this->vmregistered.size(); i++) {
				this->vmregistered[i] = this->vmregistered[i] * mtf;
			}*/


	CCudaWrapper cudaWrapper;
	cudaWrapper.warmUpGPU(this->cudaDevice);

	//this->slam_search_radius_register_all = 0.5f;
	//this->slam_bucket_size_step_register_all = 0.5f;
	if (vpc.size() > 1) {
		registerAll(cudaWrapper, this->slam_search_radius_register_all, this->slam_bucket_size_step_register_all, vpc.size());
		// std::cout << "registerAll() DONE" << std::endl;
	}
}

void gpu6DSLAM::transformPointCloud(pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> &pointcloud, Eigen::Affine3f transform)
{
	for(size_t i = 0; i < pointcloud.size(); i++)
	{
		Eigen::Vector3f p(pointcloud[i].x, pointcloud[i].y, pointcloud[i].z);
		Eigen::Vector3f pt;

		pt = transform * p;

		pointcloud[i].x = pt.x();
		pointcloud[i].y = pt.y();
		pointcloud[i].z = pt.z();

		Eigen::Affine3f tr = transform;

		tr(0,3) = 0.0f;
		tr(1,3) = 0.0f;
		tr(2,3) = 0.0f;

		Eigen::Vector3f n(pointcloud[i].normal_x, pointcloud[i].normal_y, pointcloud[i].normal_z);
		Eigen::Vector3f nt;

		nt = tr * n;
		pointcloud[i].normal_x = nt.x();
		pointcloud[i].normal_y = nt.y();
		pointcloud[i].normal_z = nt.z();
	}
	return;
}

bool gpu6DSLAM::loadmapfromfile(std::string map_filename)
{
	std::vector<Eigen::Affine3f> _vtransform;
	std::vector<pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> > _vpc;

	data_model dSets;
	std::vector<std::string> indices;

	if(dSets.loadFile(map_filename))
	{
		dSets.getAllScansId(indices);

		for (size_t i=0; i< indices.size(); i++)
		{
			std::string fn;
			dSets.getPointcloudName(indices[i], fn);
			// std::cout << indices[i]<<"\t"<<fn<<"\n";
		}

		for (size_t i=0; i< indices.size(); i++)
		{
			std::string fn;
			Eigen::Affine3f transform;
			fn = dSets.getFullPathOfPointcloud(indices[i]);
			bool isOkTr = dSets.getAffine(indices[i], transform.matrix());
			_vtransform.push_back(transform);

			if (isOkTr)
			{
				pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> pointcloud;
				if(pcl::io::loadPCDFile(fn, pointcloud) == -1)
				{
					std::cout << "problem with pcl::io::loadPCDFile: " << fn << std::endl;
					return false;
				}
				_vpc.push_back(pointcloud);
			}
		}
	}


	this->vpc.clear();
	this->vmtf.clear();
	this->vmregistered.clear();
	this->cloud_ids.clear();


	this->vpc = _vpc;
	this->vmtf = _vtransform;
	this->vmregistered = _vtransform;
	this->cloud_ids = indices;

	return true;
}

void gpu6DSLAM::callbackInitialPose(Eigen::Affine3f initial_pose)
{

	//find closest pose
	float min_dist = 10000000.0f;
	Eigen::Affine3f m = Eigen::Affine3f::Identity();
	for(size_t i = 0 ; i < this->vmregistered.size(); i++){
		float dist = sqrtf (
				(initial_pose(0,3) - this->vmregistered[i](0,3)) * (initial_pose(0,3) - this->vmregistered[i](0,3)) +
				(initial_pose(1,3) - this->vmregistered[i](1,3)) * (initial_pose(1,3) - this->vmregistered[i](1,3)) +
				(initial_pose(2,3) - this->vmregistered[i](2,3)) * (initial_pose(2,3) - this->vmregistered[i](2,3))
		);

		if( dist < min_dist) {
			min_dist = dist;
			m = this->vmregistered[i];
		}
	}

	// std::cout << m.matrix() << std::endl;
	// std::cout << initial_pose.matrix() << std::endl;

	for( size_t i = 0; i < this->vmregistered.size(); i++) {
		this->vmregistered[i] = this->vmregistered[i] * (m.inverse());
	}

	for( size_t i = 0; i < this->vmregistered.size(); i++) {
		this->vmregistered[i] = this->vmregistered[i] * initial_pose;
	}
}

void gpu6DSLAM::downsample(pcl::PointCloud<lidar_pointcloud::PointXYZIRNLRGB> &_pc, float grid_res)
{
	CCudaWrapper cudaWrapper;
	cudaWrapper.warmUpGPU(this->cudaDevice);

	cudaWrapper.downsampling(_pc, grid_res, grid_res);
}
