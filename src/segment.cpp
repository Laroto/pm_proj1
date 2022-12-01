#include <iostream>
#include <string.h>

#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>

#include <ros/ros.h>
#include <sensor_msgs/Image.h>
#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>
#include <geometry_msgs/Point.h>

#include "pm_proj1/seg_params.h"

int H_MIN, H_MAX, S_MIN, S_MAX, V_MIN, V_MAX; 
int H_MIN_old, H_MAX_old, S_MIN_old, S_MAX_old, V_MIN_old, V_MAX_old; 
int kernel_size;
std::string debug_raw;
ros::Publisher pub;
bool debug;
cv::Mat img, mask,mask_small,hsv, contours, ccomps, contours_small, ccomps_small;
float scale_down = 0.5;
bool checked_params = false;
int frame_n = 0;

void eval_debug ()
{
    if ((debug_raw == "True") || (debug_raw == "true"))
        debug = true;
    else
        debug = false;
}


bool param_update()
{
    if (H_MAX_old != H_MAX)
    {
        H_MAX_old = H_MAX;
        return true;
    }

    if (H_MIN_old != H_MIN)
    {
        H_MIN_old = H_MIN;
        return true;
    }

    if (S_MAX_old != S_MAX)
    {
        S_MAX_old = S_MAX;
        return true;
    }

    if (S_MIN_old != S_MIN)
    {
        S_MIN_old = S_MIN;
        return true;
    }

    if (V_MAX_old != V_MAX)
    {
        V_MAX_old = V_MAX;
        return true;
    }

    if (V_MAX_old != V_MAX)
    {
        V_MAX_old = V_MAX;
        return true;
    }
    return false;
}

void update_params (const pm_proj1::seg_paramsConstPtr msg)
{
    H_MIN = msg->H_MIN;
    H_MAX = msg->H_MAX;
    S_MIN = msg->S_MIN;
    S_MAX = msg->S_MAX;
    V_MIN = msg->V_MIN;
    V_MAX = msg->V_MAX;

    checked_params = true;
}

void update_image_cont()
{
    cv::inRange(hsv, cv::Scalar(H_MIN, S_MIN, V_MIN), cv::Scalar(H_MAX, S_MAX, V_MAX), mask);

    cv::Mat kernel = cv::Mat::ones(kernel_size,kernel_size, CV_8U);
    cv::dilate(mask, img, kernel);
    cv::erode(img,contours,kernel);

    cv::Moments m = cv::moments(contours, true);
    cv::Point p(m.m10/m.m00, m.m01/m.m00);
    cv::circle(contours, p, 5, cv::Scalar(0,0,255), -1);

    if (debug)
    {
        cv::resize(mask,mask_small, cv::Size(),scale_down,scale_down, cv::INTER_LINEAR);
        cv::imshow("Mask", mask_small);

        cv::resize(contours,contours_small, cv::Size(),scale_down,scale_down, cv::INTER_LINEAR);
        cv::imshow("contours", contours_small);
    }

    geometry_msgs::Point pt_msg;
    pt_msg.x = p.x;
    pt_msg.y = p.y;
    pt_msg.z = frame_n;

    pub.publish(pt_msg);
}

void update_image_ccomp()
{
    cv::inRange(hsv, cv::Scalar(H_MIN, S_MIN, V_MIN), cv::Scalar(H_MAX, S_MAX, V_MAX), mask);

    cv::Mat kernel = cv::Mat::ones(kernel_size,kernel_size, CV_8U);
    cv::dilate(mask, img, kernel);
    cv::erode(img,img,kernel);

    cv::Mat labels, centroids,stats, img_color;
    int i, nccomps = cv::connectedComponentsWithStats(img,labels,stats,centroids, 8);

    std::vector<cv::Vec3b> colors(nccomps+1);
    colors[0] = cv::Vec3b(0,0,0); // background pixels remain black.
    for( i = 1; i <= nccomps; i++ ) 
    {
        colors[i] = cv::Vec3b(rand()%256, rand()%256, rand()%256);
        if( stats.at<int>(i-1, cv::CC_STAT_AREA) < 100 )
        colors[i] = cv::Vec3b(0,0,0); // small regions are painted with black too.
    }
    ccomps = cv::Mat::zeros(img.size(), CV_8UC3);
    for( int y = 0; y < img_color.rows; y++ )
    for( int x = 0; x < img_color.cols; x++ )
    {
    int label = labels.at<int>(y, x);
    CV_Assert(0 <= label && label <= nccomps);
    ccomps.at<cv::Vec3b>(y, x) = colors[label];
    }
    
    cv::Vec3b color = (0,0,0);
    
    for (int i=0; i<nccomps; i++)
    {
        double x = centroids.at<double>(i, 0);
        double y = centroids.at<double>(i, 1);
        cv::Point2d pt(x,y);
        
        cv::circle(ccomps,pt,20,color[1]);
    }

    if (debug)
    {
        // cv::resize(mask,mask_small, cv::Size(),scale_down,scale_down, cv::INTER_LINEAR);
        // cv::imshow("Mask", mask_small);
        cv::resize(ccomps,ccomps_small, cv::Size(),scale_down,scale_down, cv::INTER_LINEAR);    
        cv::imshow("Labeled map", ccomps_small);
    }
    //cv::waitKey(1);
}

void callback (const sensor_msgs::ImageConstPtr& cam_msg)
{
    cv_bridge::CvImagePtr cam, res;
    sensor_msgs::ImagePtr out_msg;

    cv_bridge::CvImagePtr teste;

    try
    {
        cam = cv_bridge::toCvCopy(cam_msg, sensor_msgs::image_encodings::BGR8);
    }
    catch (cv_bridge::Exception& e)
    {
        ROS_ERROR("cv_bridge exception: %s", e.what());
        return;
    }

    cv::cvtColor(cam->image,hsv, cv::COLOR_RGB2HSV);
    
    frame_n++;

    //update_image_ccomp();
    update_image_cont();
}

int main(int argc, char** argv)
{
    ros::init(argc, argv, "segment");
    ros::NodeHandle nh;
  
    ros::Subscriber sub, sub_params;
    pub = nh.advertise<geometry_msgs::Point>("/point", 1);

    nh.param<int>("/H_MIN", H_MIN, 0);
    nh.param<int>("/H_MAX", H_MAX, 255);
    nh.param<int>("/S_MIN", S_MIN, 0);
    nh.param<int>("/S_MAX", S_MAX, 255);
    nh.param<int>("/V_MIN", V_MIN, 0);
    nh.param<int>("/V_MAX", V_MAX, 255);

    nh.param<int>("/kernel_size", kernel_size, 3);

    nh.param<bool>("/debug", debug, "False");
    //eval_debug();

    sub_params = nh.subscribe("/seg_params",1,update_params);

    sub = nh.subscribe("/video_raw", 1, callback);    

    while (ros::ok())
    {
        if (hsv.channels() >= 3)
        {
            if (debug)
            {
                ROS_INFO("\n\nUsing these parameters:");
                ROS_INFO("\nH_MIN: %d",H_MIN);
                ROS_INFO("\nH_MAX: %d",H_MAX);
                ROS_INFO("\nS_MIN: %d",S_MIN);
                ROS_INFO("\nS_MAX: %d",S_MAX);
                ROS_INFO("\nV_MIN: %d",V_MIN);
                ROS_INFO("\nV_MAX: %d",V_MAX);
            }

            if (param_update())
            {
                //update_image_ccomp(); 
                update_image_cont();
            }           
        }

        ros::spinOnce();
        
        cv::waitKey(5);
    }

    return 0;
}