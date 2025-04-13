#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>

#include <ros/ros.h>
#include <std_srvs/SetBool.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <actionlib/client/simple_action_client.h>
#include "rosGoalClient/bot3gpio.h"

#define NODE_NAME	"rosGoalClient"
#define SRV_NAME	"gpio_server"

#define BUF_SIZE	100
#define NAME_SIZE	30
#define ARR_CNT		10
#define GOALCNT		6
#define WATERCNT	1
#define BATHCNT		1
#define LIVINGCNT	1
#define BEDCNT		1

typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;

void * send_msg(void * arg);
void * recv_msg(void * arg);
void error_handling(const char *msg);

char name[NAME_SIZE] = "[Default]";
char msg[BUF_SIZE];
int location = 0;

int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in serv_addr;
	pthread_t snd_thread, rcv_thread;
	void * thread_return;

	ros::init(argc, argv, NODE_NAME);
	if(argc != 4) {
		printf("Usage :rosrun rosGoalClient %s <IP> <port> <name>\n", argv[0]);
		exit(1);
	}

	
	sprintf(name, "%s", argv[3]);
	
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if(sock == -1)
		error_handling("socket() error");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(atoi(argv[2]));

	if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
	error_handling("connect() error");
	sprintf(msg, "%s", name);
	write(sock, msg, strlen(msg));
	pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);
	pthread_create(&snd_thread, NULL, send_msg, (void *)&sock);

	pthread_join(snd_thread, &thread_return);
	pthread_join(rcv_thread, &thread_return);

	close(sock);
	return 0;
}

void *send_msg(void *arg)
{
	int *sock = (int *)arg;
	int str_len;
	int ret;
	fd_set initset, newset;
	struct timeval tv;
	char name_msg[NAME_SIZE + BUF_SIZE+2];

	FD_ZERO(&initset);
	FD_SET(STDIN_FILENO, &initset);

	fputs("Input a message! [ID]msg (Default ID:ALLMSG)\n",stdout);
	while(1) {
		memset(msg,0,sizeof(msg));
		name_msg[0]='\0';
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		newset = initset;
		ret = select(STDIN_FILENO + 1, &newset, NULL, NULL, &tv);
		if(FD_ISSET(STDIN_FILENO, &newset))
		{
			fgets(msg, BUF_SIZE, stdin);
			if(!strncmp(msg,"quit\n",5)) 
			{
				*sock = -1;
				return NULL;
			}
			else if(msg[0] != '[')
			{
				strcat(name_msg, "[ALLMSG]");
				strcat(name_msg, msg);
			}
			else
				strcpy(name_msg,msg);
			if(write(*sock, name_msg, strlen(name_msg))<=0)
			{
				*sock = -1;
				return NULL;
			}
		}
		if(ret == 0)
		{
			if(*sock == -1)
				return NULL;
		}
	}
}

void *recv_msg(void *arg)
{
	double water[WATERCNT][3] = {
		{-0.31, 3.22, 0.99}	
	};
	double bath[BATHCNT][3] = {
		{2.15, 0.21, 0.02}	
	};
	double living[LIVINGCNT][3] = {
		{0.65, 1.06, 0.02}	
	};
	double bed[BEDCNT][3] = {
		{1.85, 2.22, 0.86}	//수정필요! 장애물있어서 찍고, 찍고, 찍고 가야함	
	};

	int *sock = (int *)arg;
	int i;
	char *pToken;
	char *pArray[ARR_CNT]={0};

	char name_msg[NAME_SIZE + BUF_SIZE + 1];
	int str_len;


	ros::NodeHandle nh;

	ros::ServiceClient srv_client;
	srv_client = nh.serviceClient<rosGoalClient::bot3gpio>(SRV_NAME);
	rosGoalClient::bot3gpio srv;
	srv.request.a = 0;
	srv.request.b = 1;

	MoveBaseClient ac("move_base", true);
	move_base_msgs::MoveBaseGoal goal;

	while(!ac.waitForServer(ros::Duration(5.0))){
		ROS_INFO("Waiting for the move_base action server to come up");
	}

	goal.target_pose.header.frame_id = "map";

	while(1) {
		memset(name_msg, 0x0, sizeof(name_msg));
		str_len = read(*sock, name_msg, NAME_SIZE + BUF_SIZE);
		if(str_len<= 0)
		{
			*sock = -1;
			return NULL;
		}
		name_msg[str_len] = '\0';
		fputs(name_msg, stdout);

		pToken = strtok(name_msg, "[:@]");
		i = 0;
		while(pToken != NULL)
		{
			pArray[i] = pToken;
			// 줄바꿈 문자 제거
			if(i++ >= ARR_CNT)
					break;
			pToken = strtok(NULL, "[:@]");
		}

		switch(location)
		{
			case 0:
			if( !strcmp(pArray[1], "SOUNDON") || !strcmp(pArray[1], "WATERGO")){
				printf("WATER");
				for (int j=0; j<WATERCNT; j++){
					goal.target_pose.header.stamp = ros::Time::now();
	
					goal.target_pose.pose.position.x = water[j][0];
					goal.target_pose.pose.position.y = water[j][1];
					goal.target_pose.pose.orientation.w = water[j][2];
					ROS_INFO("Sending goal WATER");
					ac.sendGoalAndWait(goal, ros::Duration(120.0,0), ros::Duration(120.0,0));
	
					if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
						ROS_INFO("Goal WATER!");
					else
						ROS_INFO("The base failed to move to goal for some season");
				}
				location = 1;
				break;
			}
			else if(!strcmp(pArray[1], "BATHGO")){
				printf("BATH");
				for (int j=0; j<BATHCNT; j++){
					goal.target_pose.header.stamp = ros::Time::now();
	
					goal.target_pose.pose.position.x = bath[j][0];
					goal.target_pose.pose.position.y = bath[j][1];
					goal.target_pose.pose.orientation.w = bath[j][2];
					ROS_INFO("Sending goal BATH");
					ac.sendGoalAndWait(goal, ros::Duration(120.0,0), ros::Duration(120.0,0));
	
					if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
						ROS_INFO("Goal BATH!");
					else
						ROS_INFO("The base failed to move to goal for some season");
				}
				location = 2;
				break;
			}
			else if(!strcmp(pArray[1], "LIVINGGO")){
				printf("LIVING");
				for (int j=0; j<LIVINGCNT; j++){
					goal.target_pose.header.stamp = ros::Time::now();
	
					goal.target_pose.pose.position.x = living[j][0];
					goal.target_pose.pose.position.y = living[j][1];
					goal.target_pose.pose.orientation.w = living[j][2];
					ROS_INFO("Sending goal LIVING");
					ac.sendGoalAndWait(goal, ros::Duration(120.0,0), ros::Duration(120.0,0));
	
					if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
						ROS_INFO("Goal LIVING!");
					else
						ROS_INFO("The base failed to move to goal for some season");
				}
				location = 3;
				break;
			}
			case 1:
			if(!strcmp(pArray[1], "BATHGO")){
				printf("BATH");
				for (int j=0; j<BATHCNT; j++){
					goal.target_pose.header.stamp = ros::Time::now();
	
					goal.target_pose.pose.position.x = bath[j][0];
					goal.target_pose.pose.position.y = bath[j][1];
					goal.target_pose.pose.orientation.w = bath[j][2];
					ROS_INFO("Sending goal BATH");
					ac.sendGoalAndWait(goal, ros::Duration(120.0,0), ros::Duration(120.0,0));
	
					if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
						ROS_INFO("Goal BATH!");
					else
						ROS_INFO("The base failed to move to goal for some season");
				}
				location = 2;
				break;
			}
			else if(!strcmp(pArray[1], "LIVINGGO")){
				printf("LIVING");
				for (int j=0; j<LIVINGCNT; j++){
					goal.target_pose.header.stamp = ros::Time::now();
	
					goal.target_pose.pose.position.x = living[j][0];
					goal.target_pose.pose.position.y = living[j][1];
					goal.target_pose.pose.orientation.w = living[j][2];
					ROS_INFO("Sending goal LIVING");
					ac.sendGoalAndWait(goal, ros::Duration(120.0,0), ros::Duration(120.0,0));
	
					if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
						ROS_INFO("Goal LIVING!");
					else
						ROS_INFO("The base failed to move to goal for some season");
				}
				location = 3;
				break;
			}
			else if(!strcmp(pArray[1], "BEDGO")){
				printf("BED");
				for (int j=0; j<BEDCNT; j++){
					goal.target_pose.header.stamp = ros::Time::now();
	
					goal.target_pose.pose.position.x = bed[j][0];
					goal.target_pose.pose.position.y = bed[j][1];
					goal.target_pose.pose.orientation.w = bed[j][2];
					ROS_INFO("Sending goal BED");
					ac.sendGoalAndWait(goal, ros::Duration(120.0,0), ros::Duration(120.0,0));
	
					if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
						ROS_INFO("Goal BED!");
					else
						ROS_INFO("The base failed to move to goal for some season");
				}
				location = 0;
				break;
			}
			case 2:
			if( !strcmp(pArray[1], "SOUNDON") || !strcmp(pArray[1], "WATERGO")){
				printf("WATER");
				for (int j=0; j<WATERCNT; j++){
					goal.target_pose.header.stamp = ros::Time::now();
	
					goal.target_pose.pose.position.x = water[j][0];
					goal.target_pose.pose.position.y = water[j][1];
					goal.target_pose.pose.orientation.w = water[j][2];
					ROS_INFO("Sending goal WATER");
					ac.sendGoalAndWait(goal, ros::Duration(120.0,0), ros::Duration(120.0,0));
	
					if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
						ROS_INFO("Goal WATER!");
					else
						ROS_INFO("The base failed to move to goal for some season");
				}
				location = 1;
			}
			else if(!strcmp(pArray[1], "LIVINGGO")){
				printf("LIVING");
				for (int j=0; j<LIVINGCNT; j++){
					goal.target_pose.header.stamp = ros::Time::now();
	
					goal.target_pose.pose.position.x = living[j][0];
					goal.target_pose.pose.position.y = living[j][1];
					goal.target_pose.pose.orientation.w = living[j][2];
					ROS_INFO("Sending goal LIVING");
					ac.sendGoalAndWait(goal, ros::Duration(120.0,0), ros::Duration(120.0,0));
	
					if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
						ROS_INFO("Goal LIVING!");
					else
						ROS_INFO("The base failed to move to goal for some season");
				}
				location = 3;
				break;
			}
			else if(!strcmp(pArray[1], "BEDGO")){
				printf("BED");
				for (int j=0; j<BEDCNT; j++){
					goal.target_pose.header.stamp = ros::Time::now();
	
					goal.target_pose.pose.position.x = bed[j][0];
					goal.target_pose.pose.position.y = bed[j][1];
					goal.target_pose.pose.orientation.w = bed[j][2];
					ROS_INFO("Sending goal BED");
					ac.sendGoalAndWait(goal, ros::Duration(120.0,0), ros::Duration(120.0,0));
	
					if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
						ROS_INFO("Goal BED!");
					else
						ROS_INFO("The base failed to move to goal for some season");
				}
				location = 0;
				break;
			}
			case 3:
			if( !strcmp(pArray[1], "SOUNDON") || !strcmp(pArray[1], "WATERGO")){
				printf("WATER");
				for (int j=0; j<WATERCNT; j++){
					goal.target_pose.header.stamp = ros::Time::now();
	
					goal.target_pose.pose.position.x = water[j][0];
					goal.target_pose.pose.position.y = water[j][1];
					goal.target_pose.pose.orientation.w = water[j][2];
					ROS_INFO("Sending goal WATER");
					ac.sendGoalAndWait(goal, ros::Duration(120.0,0), ros::Duration(120.0,0));
	
					if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
						ROS_INFO("Goal WATER!");
					else
						ROS_INFO("The base failed to move to goal for some season");
				}
				location = 1;
				break;
			}
			else if(!strcmp(pArray[1], "BATHGO")){
				printf("BATH");
				for (int j=0; j<BATHCNT; j++){
					goal.target_pose.header.stamp = ros::Time::now();
	
					goal.target_pose.pose.position.x = bath[j][0];
					goal.target_pose.pose.position.y = bath[j][1];
					goal.target_pose.pose.orientation.w = bath[j][2];
					ROS_INFO("Sending goal BATH");
					ac.sendGoalAndWait(goal, ros::Duration(120.0,0), ros::Duration(120.0,0));
	
					if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
						ROS_INFO("Goal BATH!");
					else
						ROS_INFO("The base failed to move to goal for some season");
				}
				location = 2;
				break;
			}
			else if(!strcmp(pArray[1], "BEDGO")){
				printf("BED");
				for (int j=0; j<BEDCNT; j++){
					goal.target_pose.header.stamp = ros::Time::now();
	
					goal.target_pose.pose.position.x = bed[j][0];
					goal.target_pose.pose.position.y = bed[j][1];
					goal.target_pose.pose.orientation.w = bed[j][2];
					ROS_INFO("Sending goal BED");
					ac.sendGoalAndWait(goal, ros::Duration(120.0,0), ros::Duration(120.0,0));
	
					if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
						ROS_INFO("Goal BED!");
					else
						ROS_INFO("The base failed to move to goal for some season");
				}
				location = 0;
				break;
			}
		}
		printf("id:%s, msg:%s\n",pArray[0], pArray[1]);
	}
}

void error_handling(const char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
