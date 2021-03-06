/**
* CLASS: TaskUnguidedCleanDebrisServer
* DATE: 29/03/17
* AUTHOR: Kyle Morris
* DESCRIPTION: Toggles random wander schema, until debris is located.
* At this time, the robot will head towards the debris and clean it.
* After there is no debris in sight, it will continue wandering.
*/

#include <ros/ros.h>
#include <tf/transform_listener.h>
#include "arc_msgs/ArcTaskAction.h"
#include "arc_msgs/DetectedDebris.h"
#include "arc_msgs/NavigationRequest.h"
#include "std_srvs/Trigger.h"
#include <actionlib/server/simple_action_server.h>
#include "nav_msgs/Odometry.h"
#include "TaskServer.h"

#ifndef ARC_TASKS_TASKUNGUIDEDCLEANDEBRISSERVER_H
#define ARC_TASKS_TASKUNGUIDEDCLEANDEBRISSERVER_H
typedef actionlib::SimpleActionServer<arc_msgs::ArcTaskAction> ActionServer;

class TaskUnguidedCleanDebrisServer : public TaskServer {
private:
    ros::NodeHandle global_handle;
    ros::NodeHandle local_handle;

    ActionServer server;

    /**
     * Timer to keep track of how much longer we explore the environment.
     * Once started, it will continuously cycle through a specified countdown and then call the
     * explore_timer_cb
     */
    ros::Timer explore_timer;

    /**
     * Counts down how long since we failed to clean debris. Allows for some time to
     * move away from where the failed debris was, so we can find somewhere else to clean.
     */
    ros::Timer abandon_failed_debris_timer;

    /**
     * Counting down from the moment we start cleaning debris, until it is assumed to be gone.
     */
    ros::Timer clean_debris_timer;

    /**
     * Subscribes to list of debris that robot has found in it's view.
     */
    ros::Subscriber debris_sub;

    /**
     * Subscribes to our current position
     */
     //TODO: Instead of tracking position using base_pose an such, just use the TF support. You can do this in quite a few places in the framework now instead of this gibberish.
    ros::Subscriber base_pos_sub;
    tf::TransformListener pose_listener;

    /**
     * List of the debris we most recently detected. It is always being updated
     */
    arc_msgs::DetectedDebris debris_list;

    /**
     * Keep tracking of most recent position of the robot so we can identify our relative goal towards debris.
     */
    nav_msgs::Odometry recent_pose;

    /**
     * The debris in debris_list that we have targeted to clean.
     */
    arc_msgs::Debris target_debris;

    /**
     * After getting the target debris location relative to us, we set it's position as navigation goal
     */
    geometry_msgs::Pose target_pose;


    /**
     * used to call the arc_base node and toggle the schema of our choice
     */
    ros::ServiceClient arc_base_client;

    /**
     * Used to send movement request to this node, so we can navigate to debris
     */
    ros::ServiceClient move_to_debris_client;

    /**
     * Used to tell navigation adapter to stop, so we remain close to debris to clean.
     */
    ros::ServiceClient abort_all_goals_client;

    //control the state of the debris exploration/removal
    typedef enum {
        STATE_StartExploring, //Enable schemas and begin exploring behaviour
        STATE_Exploring, //We are currently exploring
        STATE_FoundDebris, //Found the debris
        STATE_SeekingDebrisLocation, //Heading to where the debris is
        STATE_RemovingDebris, //cleaning debris up
        STATE_DoneDebrisRemoval, //Debris is gone
        STATE_FailedDebrisRemoval, //We were cleaning but debris is still there
        STATE_AbandonFailedDebris //We failed to remove debris, so we need to randomly wander around to get away from the debris. then we can resume STATE_StartExploring
    } State;
    State state;

    /**
     * The results of running this task. Nothing required for the explore task.
     */
    arc_msgs::ArcTaskResult result;

    //TODO: Too many parameters all willy nilly in the class as attributes. Make a struct to keep them coupled only with the state they are corresponding to
    //These variables are only here to ensure we don't repeatedly call ROS services, we only have to do it once.
    /**
     * Whether or not the task is currently active.
     */
    bool active = false;

//Referring to specific states, to ensure we only do costly work once.
//TODO: Really should have these in an object or struct, so they can be auto reset at end of task. It's a pain remembering when to reset these, and if you don't you'll introduce weird bugs.
    /**
     * Track if we are currently moving towards a debris object
     */
    bool currently_seeking_debris = false;

    /**
     * Keep track of if we are currently cleaning debris nearby. Used to prevent
     * spamming requests to debris_cleaner_ms toggle every cycle of process()
     */
    bool currently_cleaning = false;

    /**
     * Whether or not we are currently abandoning debris in the AbandonFailedDebris State.
     */
    bool currently_abandoning = false;

    /**
     * Keeping track of amount of debris cleaned
     */
    int debris_success_count = 0;

//PARAMETERS and their default vlaues.
    double stopping_distance_from_debris = 0.5;
    double cleaning_debris_time = 8.0; //How long (in seconds) to spend cleaning the debris
    int explore_time = 120; //How long (in seconds) to explore before task is considered complete.

public:
    TaskUnguidedCleanDebrisServer();

    /**
     * perform any routine startup procedures when this task instance is started.
     * Load request parameters, check for existence of nodes needed for this task.
     */
    void startup(const arc_msgs::ArcTaskGoalConstPtr &goal);

    /**
     * the main state machine loop for the task
     */
    void process();

    /**
     * Ensure after this task instance is no longer of use, we've shut down everything that was required for it.
     * Shouldn't be working with task server here, just shutting stuff down. Individual states
     * handle setting server to aborted/succeeded.
     */
    void shutdown();

//STATES
    /**
     * Perform the shutdown for a task, disabling anything that was originally set to the complete it.
     */
    void StateStartExploring();

    /**
     * Startup the task, enabling anything that takes place in it's task instance.
     */
    void StateExploring();

    /**
     * While exploring we found debris
     */
    void StateFoundDebris();

    /**
     * Disable the random wander and head to debris location
     */
    void StateSeekingDebrisLocation();

    /**
     * Clean debris once we are there.
     */
    void StateRemovingDebris();

    /**
     * Succesfully removed debris. Begin exploring again.
     */
    void StateDoneDebrisRemoval();

    /**
     * We tried removing debris but it's still there after some amount of time... welp.
     */
    void StateFailedDebrisRemoval();

    /**
     * Couldn't clean the debris, so let's escape it's location, as something is goofy and we have to retry from
     * a different angle.
     */
    void StateAbandonFailedDebris();

    std::string stateToString(State state);

//CALLBACKS
    /**
     * Perform the main task. Explore for some amount of time
     */
    void goal_cb(const arc_msgs::ArcTaskGoalConstPtr &goal);

     /**
     * Callback after the explore timer gets set off
     * @param event some data about how long we explored.
     */
    void explore_timer_cb(const ros::TimerEvent &event);
    void abandon_failed_debris_timer_cb(const ros::TimerEvent &event);
    void clean_debris_timer_cb(const ros::TimerEvent &event);
    void debris_locations_cb(const arc_msgs::DetectedDebris &debris);
    void base_pose_cb(const nav_msgs::Odometry &odom);
};

#endif //ARC_TASKS_TASKUNGUIDEDCLEANDEBRISSERVER_H
