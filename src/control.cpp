#include "ros/ros.h"
#include "std_msgs/String.h"
#include "geometry_msgs/PoseWithCovarianceStamped.h"
#include "geometry_msgs/PoseWithCovariance.h"
#include <sstream>
#include "std_msgs/UInt16MultiArray.h"
#include <vector>
#include <move_base_msgs/MoveBaseAction.h>
#include <actionlib/client/simple_action_client.h>
#include <std_msgs/Float64.h>
#include <stdbool.h>
#include "geometry_msgs/Vector3.h"
#include "geometry_msgs/Twist.h"
#include <unistd.h>
#include "geometry_msgs/Pose.h"
#include "geometry_msgs/Point.h"
#include <nav_msgs/MapMetaData.h>
#include <stdint.h>
#include <cstdint>

//Estados
const int TRUE = 1;
const int FALSE	=0;
const int STARTED	=0;
const int STOPPED	=1;
const int	BALL_APROX =2;
const int BALL_RECOLECT= 3;
const int EXPLORING= 4;
const int LOST =5;
const int HOME_RETURN =6;
int STATE=0;
int LAST_STATE=0;
int CHANGE_STATE=0;

//variables de aproximación a pelota
const double factorX=0.0004; //Factor de escalado direccional.
const double factorA=0.0008; //Factor de escalado angular
const double PRECISION=3; //precisión que se ha de cumplir para recojer la pelota. Mas pequeño mas precisión
const double MINIUM_X=0.003; // Mínimo factor de movimiento en X
const double MINIUM_Z=0.0060; // Mínimo factor de movimiento angular
geometry_msgs::Vector3 aprox_vector;
geometry_msgs::Twist movetoball;

typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;
//declaración del vector de la covarianza
std_msgs::Float64* covariance = new std_msgs::Float64[36]();
geometry_msgs::PoseWithCovariance pose;


const double C_LIMIT = 0.09;
const double C_LIMIT_D = 0.05;

bool perdido =false;


move_base_msgs::MoveBaseGoal goal;

bool moves(move_base_msgs::MoveBaseGoal goals){
MoveBaseClient ac("move_base", true);
  goals.target_pose.header.stamp = ros::Time::now();
      //ROS_INFO("Enviar goal");

      while(!ac.waitForServer(ros::Duration(5.0))){
        ROS_INFO("Waiting for the move_base action server to come up");
      }
      ac.sendGoal(goals);

      return true;
}

void chatterCallback(const geometry_msgs::PoseWithCovarianceStamped vector)
{
    double Cx = vector.pose.covariance[0];
    double Cxy = vector.pose.covariance[1];
    double Cy= vector.pose.covariance[6];
    double Cyx= vector.pose.covariance[7];
    //double Cy1= ¡
    if (fabs(Cx)>C_LIMIT || fabs(Cy)>C_LIMIT || fabs(Cyx)>C_LIMIT ||fabs(Cxy)>C_LIMIT ){
      //ROS_INFO("Limite superado %lf", Cx );
      if(perdido==false){
        ROS_INFO("Me estoy perdiendo");
        goal.target_pose.header.frame_id = "map";
        goal.target_pose.pose.position.x = -0.4;
        goal.target_pose.pose.position.y = -0.4;
        goal.target_pose.pose.orientation.w = 1.5;

        ROS_INFO("Volviendo a casa");
        moves(goal);
        //ac.sendGoal(goal);
      }
       perdido=true;

    }else{
        //ROS_INFO("Limite no superado %lf", Cx);
      if(fabs(Cx)>C_LIMIT_D || fabs(Cy)>C_LIMIT_D || fabs(Cyx)>C_LIMIT_D ||fabs(Cxy)>C_LIMIT_D ){
        if(perdido==true){
          ROS_INFO("Ya se donde estoy");
          //parar
          goal.target_pose.header.frame_id = "base_link";
          goal.target_pose.pose.position.x = 0.0;
          goal.target_pose.pose.position.y = 0.0;
          goal.target_pose.pose.orientation.w = 1.0;
            ROS_INFO("Me paro");
          moves(goal);
          //ac.sendGoal(goal);
        }
        perdido=false;
      }
    }
}

void ballCallback(const geometry_msgs::Vector3& vector){


aprox_vector=vector;
  if (vector.z>0){

    aprox_vector=vector;
      //ROS_INFO("estado: %d",STATE);
      if(STATE==EXPLORING||STATE==STARTED){
        //ROS_INFO("dentro: %d",STATE);
        LAST_STATE=STATE;
        STATE=BALL_APROX;
        CHANGE_STATE=TRUE;
      }
  }else if(STATE==BALL_APROX){

      STATE=LAST_STATE;
      CHANGE_STATE=TRUE;
  }
}

int main(int argc, char **argv)
{

  ros::init(argc, argv, "control");

  //declaración Subscriber
  ros::NodeHandle n;
  ros::Subscriber sub = n.subscribe("amcl_pose", 10,chatterCallback);
  ros::Subscriber dir = n.subscribe("/camera/direction", 10,ballCallback);
  //declaración Publisher STATE
  std_msgs::String msg;
  std::stringstream ss;
  ros::Publisher chatter_pub = n.advertise<std_msgs::String>("state", 1000);
  //declaración Publisher TELEOP
  ros::Publisher ball_aprox = n.advertise<geometry_msgs::Twist>("/cmd_vel_mux/input/teleop", 1000);

  ros::Rate loop_rate(10);


  while (ros::ok())
  {
//ROS_INFO("looop");


//------------------------------
//BALL_APROX
//------------------------------
if(STATE==BALL_APROX){
  //ROS_INFO("I like balls");
  //ROS_INFO("1 - Canvi estat a BALL_RECOLECT? %d",STATE );
  double xb = aprox_vector.y*factorX;
  double zb = -aprox_vector.x*factorA;
  //ROS_INFO ("INI xb: %lf  zb: %lf", xb,zb);
  if(fabs(xb)<MINIUM_X){
    //ROS_INFO ("MID abs(xb): %lf  MINIUM_X: %lf", abs(xb),MINIUM_X);
    //ROS_INFO("Minimo X");
    if(xb>0){
        //ROS_INFO("Minimo X +");
        xb=MINIUM_X;
    }else if(xb<0){
      //ROS_INFO("Minimo X -");
      xb=-MINIUM_X;
    }
  }

  if(fabs(zb)<MINIUM_Z){
      //ROS_INFO("Minimo Z");
    if(zb>0){
        //ROS_INFO("Minimo Z +");
      zb=MINIUM_Z;
    }else if(zb<0){
      //ROS_INFO("Minimo Z -");
      zb=-MINIUM_Z;
    }
  }
  ROS_INFO ("Movimiento en x: %lf  Angular: %lf", xb,zb);

  movetoball.linear.x =xb;
  movetoball.angular.z=zb;

  ball_aprox.publish(movetoball);

  if(abs(aprox_vector.y)<=PRECISION && abs(aprox_vector.x)<=PRECISION){
    STATE=BALL_RECOLECT;
    CHANGE_STATE=TRUE;
    //ROS_INFO("2 - Canvi estat a BALL_RECOLECT? %d",STATE );
  }
}

//------------------------------
//BALL_RECOLECT
//------------------------------
else if(STATE==BALL_RECOLECT){
ROS_INFO("Simulacion de Recolectar pelota: 5 segundos");
usleep(3000000);
STATE=LAST_STATE;
CHANGE_STATE=TRUE;
ROS_INFO("Volviendo al estado anterior: %d", STATE);
}
//------------------------------
//Enviar el último estado
//------------------------------
if(CHANGE_STATE==TRUE){
  if(STATE==STARTED){
      msg.data = "STARTED";
   chatter_pub.publish(msg);
  }else if(STATE==STOPPED){
    msg.data = "STOPPED";
   chatter_pub.publish(msg);
  }else if(STATE==BALL_APROX){
    msg.data =  "BALL_APROX";
   chatter_pub.publish(msg);
  }else if(STATE==BALL_RECOLECT){
   msg.data = "BALL_RECOLECT";
   chatter_pub.publish(msg);
  }else if(STATE==EXPLORING){
      msg.data =  "EXPLORING";
     chatter_pub.publish(msg);
  }else if(STATE==LOST){
      msg.data =  "LOST";
     chatter_pub.publish(msg);
  }else if(STATE==HOME_RETURN){
        msg.data =  "HOME_RETURN";
       chatter_pub.publish(msg);
  }

  CHANGE_STATE=FALSE;
}


    //ROS_INFO("%s", msg.data.c_str());

    ros::spinOnce();

    loop_rate.sleep();

  }
}
