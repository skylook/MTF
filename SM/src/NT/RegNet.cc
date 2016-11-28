#include "mtf/SM/NT/RegNet.h"
#include "mtf/Utilities/miscUtils.h"
#include "mtf/Utilities/imgUtils.h"

#include <fstream> 

_MTF_BEGIN_NAMESPACE

namespace nt{
	RegNet::RegNet(AM _am, SSM _ssm, const ParamType *rg_params) :
		SearchMethod(_am, _ssm), params(rg_params){
		printf("\n");
		printf("Using Regression Network SM with:\n");
		printf("max_iters: %d\n", params.max_iters);
		printf("n_samples: %d\n", params.n_samples);
		printf("epsilon: %f\n", params.epsilon);
		if(params.pix_sigma.empty() || params.pix_sigma[0] <= 0){
			if(params.ssm_sigma.empty()){
				throw std::invalid_argument("Sigma must be provided for at least one sampler");
			}
			using_pix_sigma = false;
			n_samplers = max(params.ssm_sigma.size(), params.ssm_mean.size());
		} else{
			using_pix_sigma = true;
			n_samplers = params.pix_sigma.size();
			utils::printMatrix(Map<const RowVectorXd>(params.pix_sigma.data(), params.pix_sigma.size()),
				"pix_sigma");
		}
		printf("n_samplers: %d\n", n_samplers);
		printf("appearance model: %s\n", am->name.c_str());
		printf("state space model: %s\n", ssm->name.c_str());
		printf("\n");

		name = "reg_net";
		log_fname = "log/mtf_rn_log.txt";
		time_fname = "log/mtf_rn_times.txt";
		frame_id = 0;

		ssm_state_size = ssm->getStateSize();
		am_dist_size = am->getDistFeatSize();

		state_sigma.resize(n_samplers);
		state_mean.resize(n_samplers);
		sampler_n_samples.resize(n_samplers);
		if(!using_pix_sigma){
			if(params.ssm_mean.empty()){
				//! default to zero mean
				params.ssm_mean.push_back(vectord(ssm_state_size, 0));
			}
			int sampler_id = 0, sigma_id = 0, mean_id = 0;
			while(sampler_id < n_samplers){
				if(params.ssm_sigma[sigma_id].size() < ssm_state_size){
					throw std::invalid_argument(
						cv::format("NN :: SSM sigma for sampler %d has invalid size: %d",
						sampler_id, params.ssm_sigma[sigma_id].size()));
				}
				if(params.ssm_mean[mean_id].size() < ssm_state_size){
					throw std::invalid_argument(
						cv::format("NN :: SSM mean for sampler %d has invalid size: %d",
						sampler_id, params.ssm_mean[mean_id].size()));
				}
				state_sigma[sampler_id] = Map<const VectorXd>(params.ssm_sigma[sigma_id].data(), ssm_state_size);
				state_mean[sampler_id] = Map<const VectorXd>(params.ssm_mean[mean_id].data(), ssm_state_size);
				++sampler_id;
				if(sampler_id < params.ssm_sigma.size()){ ++sigma_id; }
				if(sampler_id < params.ssm_mean.size()){ ++mean_id; }
			}
		}
		//! get the no. of samples generated by each sampler
		int samples_per_sampler = params.n_samples / n_samplers;
		for(int sampler_id = 0; sampler_id < n_samplers; ++sampler_id){
			sampler_n_samples[sampler_id] = samples_per_sampler;
		}
		int residual_samples = params.n_samples - n_samplers*samples_per_sampler;
		if(residual_samples >= n_samplers){
			throw std::logic_error(
				cv::format("Residual particle count: %d exceeds the no. of samplers: %d",
				residual_samples, n_samplers));
		}
		//! distribute the residual particles evenly among the samplers;
		for(int sampler_id = 0; sampler_id < residual_samples; ++sampler_id){
			++sampler_n_samples[sampler_id];
		}
		printf("ssm_state_size: %d\n", ssm_state_size);
		printf("am_dist_size: %d\n", am_dist_size);

		eig_dataset.resize(params.n_samples, am_dist_size);
		eig_result.resize(1);
		eig_dists.resize(1);

		ssm_perturbations.resize(params.n_samples);
		inv_state_update.resize(ssm_state_size);

		string fname_template = cv::format("%s_%s_%d_%d", am->name.c_str(), ssm->name.c_str(),
			params.n_samples, am_dist_size);
		saved_db_path = cv::format("%s/%s.db", params.saved_index_dir.c_str(), fname_template.c_str());
		saved_idx_path = cv::format("%s/%s.idx", params.saved_index_dir.c_str(),
			fname_template.c_str());
	}

	void RegNet::initialize(const cv::Mat &corners){
		start_timer();

		am->clearInitStatus();
		ssm->clearInitStatus();

		ssm->initialize(corners);

		if(using_pix_sigma){
			//! estimate SSM parameter sigma from pixel sigma
			for(int sampler_id = 0; sampler_id < n_samplers; ++sampler_id){
				state_sigma[sampler_id].resize(ssm_state_size);
				state_mean[sampler_id] = VectorXd::Zero(ssm_state_size);
				ssm->estimateStateSigma(state_sigma[sampler_id], params.pix_sigma[sampler_id]);
			}
		}
		//! print the sigma for the SSM samplers
		for(int sampler_id = 0; sampler_id < n_samplers; ++sampler_id){
			printf("%s: ", cv::format("state_sigma[%d]", sampler_id).c_str());
			utils::printMatrix(state_sigma[sampler_id].transpose(), nullptr, "%e", " ");
		}
		//! initialize SSM sampler with the first distribution
		ssm->initializeSampler(state_sigma[0], state_mean[0]);

		am->initializePixVals(ssm->getPts());
		am->initializeDistFeat();

		//utils::printMatrix(ssm->getCorners(), "init_corners original");
		//utils::printMatrix(ssm->getCorners(), "init_corners after");
		//utils::printScalarToFile("initializing RegNet...", " ", log_fname, "%s", "w");

		if(params.show_samples){
			am->getCurrImg().convertTo(curr_img_uchar, CV_8UC1);
		}
		bool dataset_loaded = false;
		if(params.load_index){
			ifstream in_file(saved_db_path, ios::in | ios::binary);
			if(in_file.good()){
				printf("Loading feature dataset from: %s\n", saved_db_path.c_str());
				mtf_clock_get(db_start_time);
				in_file.read((char*)(eig_dataset.data()), sizeof(double)*eig_dataset.size());
				for(int sample_id = 0; sample_id < params.n_samples; ++sample_id){
					ssm_perturbations[sample_id].resize(ssm_state_size);
					in_file.read((char*)(ssm_perturbations[sample_id].data()), sizeof(double)*ssm_state_size);
				}
				in_file.close();
				double db_time;
				mtf_clock_get(db_end_time);
				mtf_clock_measure(db_start_time, db_end_time, db_time);
				printf("Time taken: %f secs\n", db_time);
				dataset_loaded = true;
				if(!ifstream(saved_idx_path, ios::in | ios::binary).good()){
					// index file does not exist or is unreadable
					params.load_index = false;
				} else{
					params.save_index = false;
				}
			} else{
				printf("Failed to load feature dataset from: %s\n", saved_db_path.c_str());
				// index must be rebuilt if dataset could not loaded
				params.load_index = false;
			}
		}
		if(!dataset_loaded){
			int pause_after_show = 1;
			printf("building feature dataset...\n");
			mtf_clock_get(db_start_time);
			int sample_id = 0;
			for(int sampler_id = 0; sampler_id < n_samplers; ++sampler_id){
				if(n_samplers > 1){
					//! need to reset SSM sampler only if multiple samplers are in use
					//! since it was initialized with the first one
					ssm->setSampler(state_sigma[sampler_id], state_mean[sampler_id]);
				}
				for(int dist_sample_id = 0; dist_sample_id < sampler_n_samples[sampler_id]; ++dist_sample_id){
					ssm_perturbations[sample_id].resize(ssm_state_size);
					//utils::printScalar(sample_id, "sample_id");
					//utils::printMatrix(ssm->getCorners(), "Corners before");
					ssm->generatePerturbation(ssm_perturbations[sample_id]);
					//utils::printMatrix(ssm_perturbations[sample_id], "state_update");

					if(params.additive_update){
						inv_state_update = -ssm_perturbations[sample_id];
						ssm->additiveUpdate(inv_state_update);
					} else{
						ssm->invertState(inv_state_update, ssm_perturbations[sample_id]);
						ssm->compositionalUpdate(inv_state_update);
					}
					//utils::printMatrix(inv_state_update, "inv_state_update");


					am->updatePixVals(ssm->getPts());
                    //eig_dataset.row(sample_id)= (am->getCurrPixVals()-am->getInitPixVals()).transpose();
					am->updateDistFeat(eig_dataset.row(sample_id).data());

					if(params.show_samples){
						cv::Point2d sample_corners[4];
						ssm->getCorners(sample_corners);
						utils::drawCorners(curr_img_uchar, sample_corners,
							cv::Scalar(0, 0, 255), to_string(sample_id + 1));
						if((sample_id + 1) % params.show_samples == 0){
							cv::imshow("Samples", curr_img_uchar);
							int key = cv::waitKey(1 - pause_after_show);
							if(key == 27){
								cv::destroyWindow("Samples");
								params.show_samples = 0;
							} else if(key == 32){
								pause_after_show = 1 - pause_after_show;
							}
							am->getCurrImg().convertTo(curr_img_uchar, CV_8UC1);
						}
					}
					// reset SSM to previous state
					if(params.additive_update){
						ssm->additiveUpdate(ssm_perturbations[sample_id]);
					} else{
						ssm->compositionalUpdate(ssm_perturbations[sample_id]);
					}
					++sample_id;
					//utils::printMatrix(ssm->getCorners(), "Corners after");
				}
			}
			double db_time;
			mtf_clock_get(db_end_time);
			mtf_clock_measure(db_start_time, db_end_time, db_time);
			printf("Time taken: %f secs\n", db_time);
			if(params.save_index){
				ofstream out_file(saved_db_path, ios::out | ios::binary);
				if(out_file.good()){
					printf("Saving dataset to: %s\n", saved_db_path.c_str());
					out_file.write((char*)(eig_dataset.data()), sizeof(double)*eig_dataset.size());
					for(int sample_id = 0; sample_id < params.n_samples; ++sample_id){
						out_file.write((char*)(ssm_perturbations[sample_id].data()), sizeof(double)*ssm_state_size);
					}
					out_file.close();
				} else{
					printf("Failed to save dataset to: %s\n", saved_db_path.c_str());
				}
			}
		}

		double idx_time;
		mtf_clock_get(idx_start_time);
		printf("building regression network...\n");
		std::vector<cv::Mat> training_data;
		std::vector<cv::Mat> training_labels;
		training_data.resize(params.n_samples);
		training_labels.resize(params.n_samples);
		for(int sample_id = 0; sample_id < params.n_samples; ++sample_id){
            /*** Modify Mat images and Mat warp parameters back to 32F and 64F**/
            //Convert to cv::Mat
            cv::Mat singlech_img = cv::Mat(am->getResY(), am->getResX(), CV_64FC1,(double*) eig_dataset.row(sample_id).data());

            //Convert from 64F to 32F
            singlech_img.convertTo(singlech_img, CV_32FC1);

            //Convert from Grayscale to BGR
            cv::Mat multich_img(am->getResY(), am->getResX(), CV_32FC3);
            cv::cvtColor(singlech_img, multich_img, CV_GRAY2BGR);
            
            // Print ssm perturbations
			//printf("ssm_perturbations[%d].size:%ld\n", sample_id, ssm_perturbations[sample_id].size());
			//utils::printMatrix(ssm_perturbations[sample_id], cv::format("ssm_perturbations[%d]", sample_id).c_str());
   //             std::cout<<"Perturbation is ";
   //             for (int i=0; i<ssm->getStateSize(); i++)
   //                 std::cout<<ssm_perturbations[sample_id](i)<<" ";
   //             std::cout<<std::endl;

            // Convert from 32FC3 to 8UC3
            training_data[sample_id]= utils::convertFloatImgToUchar(multich_img, 3);
/*            if(sample_id==0)
            {
                cv::imshow("Second test", training_data[sample_id]);
                cv::waitKey();
            }
*/		
            cv::Mat warp_params(1, ssm->getStateSize(), CV_64FC1, (double*)ssm_perturbations[sample_id].data());
            training_labels[sample_id].create(1,ssm->getStateSize(), CV_32FC1);
            warp_params.convertTo(training_labels[sample_id], training_labels[sample_id].type() );
		}
		reg_net = utils::createNetwork();

        printf("start training of regression network\n");
		utils::train(reg_net, training_data, training_labels);
		mtf_clock_get(idx_end_time);
		mtf_clock_measure(idx_start_time, idx_end_time, idx_time);
		printf("Time taken: %f secs\n", idx_time);

		ssm->getCorners(cv_corners_mat);

		end_timer();
		write_interval(time_fname, "w");
	}

	void RegNet::update(){
		++frame_id;
		write_frame_id(frame_id);

		am->setFirstIter();
		for(int i = 0; i < params.max_iters; i++){
			init_timer();

			am->updatePixVals(ssm->getPts());
			record_event("am->updatePixVals");
            

//            VectorXd diff=am->getCurrPixVals()-am->getInitPixVals();
//            const cv::Mat singlech_img = cv::Mat(am->getResY(), am->getResX(), CV_64FC1, (double*) diff.data()); //(double*) am->getCurrPixVals().data());
            const cv::Mat singlech_img = cv::Mat(am->getResY(), am->getResX(), CV_64FC1, (double*) am->getCurrPixVals().data());

            //Convert from 64F to 32F
            cv::Mat singlech_img_32F;
            singlech_img.convertTo(singlech_img_32F, CV_32FC1);
            
            //Convert from Grayscale to BGR
            cv::Mat multich_img(am->getResY(), am->getResX(), CV_32FC3);
            cv::cvtColor(singlech_img_32F, multich_img, CV_GRAY2BGR);
            cv::Mat imgU= utils::convertFloatImgToUchar(multich_img, 3);
//            cv::imshow("testing", imgU);
//            cv::waitKey();


//            std::cout<<"reached here "<<std::endl;
			float *output = utils::forwardPass(reg_net, imgU);
            VectorXd double_output(ssm->getStateSize());
            for (int i=0; i<ssm->getStateSize(); i++)
                double_output(i)= output[i];

			prev_corners = ssm->getCorners();

			if(params.additive_update){
				ssm->additiveUpdate(double_output);
				record_event("ssm->additiveUpdate");
			} else{
				ssm->compositionalUpdate(double_output);
				record_event("ssm->compositionalUpdate");
			}

			double update_norm = (prev_corners - ssm->getCorners()).squaredNorm();
			record_event("update_norm");

			write_data(time_fname);

			if(update_norm < params.epsilon){
				if(params.debug_mode){
					printf("n_iters: %d\n", i + 1);
				}
				break;
			}
			am->clearFirstIter();
		}
		ssm->getCorners(cv_corners_mat);
	}
}
_MTF_END_NAMESPACE

