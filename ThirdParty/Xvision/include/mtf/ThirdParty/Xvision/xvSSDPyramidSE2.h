#ifndef MTF_XVSSD_PYRAMID_SE2
#define MTF_XVSSD_PYRAMID_SE2

#include "xvSSDMain.h"

class XVSSDPyramidSE2: public XVSSDMain {

	typedef XVPyramidStepper<XVSE2Stepper< IMAGE_TYPE > >  STEPPER_TYPE;
    typedef XVSSD< IMAGE_TYPE, STEPPER_TYPE > TRACKER_TYPE;
    typedef TRACKER_TYPE::SP STATE_PAIR_TYPE;
    typedef STEPPER_TYPE::STATE_TYPE STATE_TYPE;

public:

    TRACKER_TYPE *ssd;
	STEPPER_TYPE *stepper;
	STATE_PAIR_TYPE current_state;	

	int no_of_levels;
	double scale;


	XVSSDPyramidSE2(const ParamType *xv_params = NULL,
               int no_of_levels=2, double scale=0.5):
        XVSSDMain(xv_params) {
					  this->no_of_levels=no_of_levels;
					  this->scale=scale;
					  name="pyramid_se2";
		}

    void initTracker() {
        //printf("Using Pyramidal SE2 SSD Tracker with:\n\t");
		printf("\tno_of_levels=%d\n\t",no_of_levels);
		printf("scale=%f\n\t",scale);

		stepper=new STEPPER_TYPE(init_template, scale, no_of_levels);
		STATE_TYPE state;
		state.trans=*init_pos;
		state.angle=0.0;
		state.scale=1.0;
		ssd=new TRACKER_TYPE;
		ssd->setStepper(*(stepper));
		ssd->initState(state);
		template_img = ssd->getTarget();
    }

	double updateTrackerState() {
		current_state=ssd->step(*xv_frame);
		warped_img=ssd->warpedImage();
		return current_state.error;		
	}

	void updateCorners(){
		XVAffineMatrix angleMat(-current_state.state.angle);
		XVAffineMatrix scaleMat( 1 / current_state.state.scale,
			1 / current_state.state.scale);
		XVAffineMatrix tformMat((XVMatrix)scaleMat * (XVMatrix)angleMat);
		for(int i=0; i<NCORNERS; ++i)
			corners[i] = (tformMat * points[i]) + current_state.state.trans;
	}

	void getTransformedPoints(vector<XVPositionD> &in_pts, vector<XVPositionD> &out_pts){
		XVAffineMatrix angleMat(-current_state.state.angle);
		XVAffineMatrix scaleMat( 1 / current_state.state.scale,
			1 / current_state.state.scale);
		XVAffineMatrix tformMat((XVMatrix)scaleMat * (XVMatrix)angleMat);
		for(int i=0; i<NCORNERS; ++i)
			out_pts[i] = (tformMat * in_pts[i]) + current_state.state.trans;
	}
	void resetTrackerPosition(double pos_x, double pos_y){
		STATE_TYPE temp_state(current_state.state);
		temp_state.trans.setX(pos_x);
		temp_state.trans.setY(pos_y);
		ssd->initState(temp_state);
	}
	void resetTrackerTemplate(IMAGE_TYPE *img, double pos_x, double pos_y,
		double size_x, double size_y){
			updateTemplate(img, pos_x, pos_y, size_x, size_y);
			ssd->setStepper(STEPPER_TYPE(init_template, scale, no_of_levels));
			resetTrackerPosition(pos_x, pos_y);
	}

};
#endif