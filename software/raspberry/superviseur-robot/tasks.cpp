/*
 * Copyright (C) 2018 dimercur
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tasks.h"
#include <stdexcept>

// Déclaration des priorités des taches
#define PRIORITY_TSERVER 30
#define PRIORITY_TOPENCOMROBOT 20
#define PRIORITY_TMOVE 40
#define PRIORITY_TSENDTOMON 22
#define PRIORITY_TRECEIVEFROMMON 25
#define PRIORITY_TSTARTROBOT 20
#define PRIORITY_TCAMERA 10
#define PRIORITY_TBATTERY 35
#define PRIORITY_TWATCHDOG 50

#define PERIOD_1000MS 1000 * 1000 * 1000
#define PERIOD_100MS 1000 * 1000 * 100
#define PERIOD_500MS 1000 * 1000 * 500
#define PERIOD_10MS 1000 * 1000 * 10

/*
 * Some remarks:
 * 1- This program is mostly a template. It shows you how to create tasks, semaphore
 *   message queues, mutex ... and how to use them
 * 
 * 2- semDumber is, as name say, useless. Its goal is only to show you how to use semaphore
 * 
 * 3- Data flow is probably not optimal
 * 
 * 4- Take into account that ComRobot::Write will block your task when serial buffer is full,
 *   time for internal buffer to flush
 * 
 * 5- Same behavior existe for ComMonitor::Write !
 * 
 * 6- When you want to write something in terminal, use cout and terminate with endl and flush
 * 
 * 7- Good luck !
 */

/**
 * @brief Initialisation des structures de l'application (tâches, mutex, 
 * semaphore, etc.)
 */
void Tasks::Init() {
    int status;
    int err;

    /**************************************************************************************/
    /* 	Mutex creation                                                                    */
    /**************************************************************************************/
    if (err = rt_mutex_create(&mutex_monitor, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_robot, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_robotStarted, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_move, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_activate_camera, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_com_robot_status, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_search_arena, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_arena_confirmed, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_watchdog, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_position_requested, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Mutexes created successfully" << endl << flush;

    /**************************************************************************************/
    /* 	Semaphors creation       							  */
    /**************************************************************************************/
    if (err = rt_sem_create(&sem_barrier, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_openComRobot, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_serverOk, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_startRobot, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_wd_active, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_monitor_reset_connection, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_arena_confirmation, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Semaphores created successfully" << endl << flush;

    /**************************************************************************************/
    /* Tasks creation                                                                     */
    /**************************************************************************************/
    if (err = rt_task_create(&th_server, "th_server", 0, PRIORITY_TSERVER, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_sendToMon, "th_sendToMon", 0, PRIORITY_TSENDTOMON, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_receiveFromMon, "th_receiveFromMon", 0, PRIORITY_TRECEIVEFROMMON, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_openComRobot, "th_openComRobot", 0, PRIORITY_TOPENCOMROBOT, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_startRobot, "th_startRobot", 0, PRIORITY_TSTARTROBOT, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_move, "th_move", 0, PRIORITY_TMOVE, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_camera, "th_camera", 0, PRIORITY_TCAMERA, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_battery, "th_battery", 0, PRIORITY_TBATTERY, 0)){
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_watchdog, "th_watchdog", 0, PRIORITY_TWATCHDOG, 0)){
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Tasks created successfully" << endl << flush;

    /**************************************************************************************/
    /* Message queues creation                                                            */
    /**************************************************************************************/
    if ((err = rt_queue_create(&q_messageToMon, "q_messageToMon", sizeof (Message*)*50, Q_UNLIMITED, Q_FIFO)) < 0) {
        cerr << "Error msg queue create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Queues created successfully" << endl << flush;

}

/**
 * @brief Démarrage des tâches
 */
void Tasks::Run() {
    rt_task_set_priority(NULL, T_LOPRIO);
    int err;

    if (err = rt_task_start(&th_server, (void(*)(void*)) & Tasks::ServerTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_sendToMon, (void(*)(void*)) & Tasks::SendToMonTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_receiveFromMon, (void(*)(void*)) & Tasks::ReceiveFromMonTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_openComRobot, (void(*)(void*)) & Tasks::OpenComRobot, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_startRobot, (void(*)(void*)) & Tasks::StartRobotTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_move, (void(*)(void*)) & Tasks::MoveTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_camera, (void(*)(void*)) & Tasks::CameraTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_battery, (void(*)(void*)) & Tasks::BatteryLevelTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_watchdog, (void(*)(void*)) & Tasks::WatchdogUpdateTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }


    cout << "Tasks launched" << endl << flush;
}

/**
 * @brief Arrêt des tâches
 */
void Tasks::Stop() {
    monitor.Close();
    robot.Close();
}

/**
 */
void Tasks::Join() {
    cout << "Tasks synchronized" << endl << flush;
    rt_sem_broadcast(&sem_barrier);
    pause();
}

/**
 * @brief Thread handling server communication with the monitor.
 */
void Tasks::ServerTask(void *arg) {
    int status;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are started)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task server starts here                                                        */
    /**************************************************************************************/
    while(1) {
        rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
        status = monitor.Open(SERVER_PORT);
        rt_mutex_release(&mutex_monitor);

        cout << "Open server on port " << (SERVER_PORT) << " (" << status << ")" << endl;

        if (status < 0) throw std::runtime_error {
            "Unable to start server on port " + std::to_string(SERVER_PORT)
        };
        monitor.AcceptClient(); // Wait the monitor client
        cout << "Rock'n'Roll baby, client accepted!" << endl << flush;
        rt_sem_broadcast(&sem_serverOk);
        rt_sem_p(&sem_monitor_reset_connection, TM_INFINITE);
        cout << "Resetting server" << endl << flush;
        rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
        monitor.Close();
        rt_mutex_release(&mutex_monitor);
    }
}

/**
 * @brief Thread sending data to monitor.
 */
void Tasks::SendToMonTask(void* arg) {
    Message *msg;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task sendToMon starts here                                                     */
    /**************************************************************************************/
    rt_sem_p(&sem_serverOk, TM_INFINITE);

    while (1) {
        cout << "wait msg to send" << endl << flush;
        msg = ReadInQueue(&q_messageToMon);
        cout << "Send msg to mon: " << msg->ToString() << endl << flush;
        rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
        monitor.Write(msg); // The message is deleted with the Write
        rt_mutex_release(&mutex_monitor);
    }
}

/**
 * @brief Thread receiving data from monitor.
 */
void Tasks::ReceiveFromMonTask(void *arg) {
    Message *msgRcv;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task receiveFromMon starts here                                                */
    /**************************************************************************************/
    
    while(1) {
        rt_sem_p(&sem_serverOk, TM_INFINITE);
        cout << "Received message from monitor (re)activated" << endl << flush;
        while (1) {
            msgRcv = monitor.Read();
            cout << "Rcv <= " << msgRcv->ToString() << endl << flush;

            if (msgRcv->CompareID(MESSAGE_MONITOR_LOST)) {
                delete(msgRcv);
                cout << "Connection to Monitor lost, stopping robot and returning to initial state" << endl << flush;
                // tell move robot thread to send a stop message to the robot
                rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
                if (robotStarted) {
                    rt_mutex_acquire(&mutex_robot, TM_INFINITE);
                    robot.Write(robot.Reset());
                    rt_mutex_release(&mutex_robot);
                }
                rt_mutex_release(&mutex_robotStarted);

                cout << "Stopped robot movement" << endl << flush;

                StopRobotCommunication();

                rt_sem_v(&sem_monitor_reset_connection);
                // close server
                rt_mutex_acquire(&mutex_activate_camera, TM_INFINITE);
                activate_camera = 0;
                rt_mutex_release(&mutex_activate_camera);
                break;
                
            } else if(msgRcv->CompareID(MESSAGE_ROBOT_RESET)){
                StopRobotCommunication();
                rt_mutex_acquire(&mutex_robot, TM_INFINITE);
                robot.Write(robot.Reset());
                rt_mutex_release(&mutex_robot);

            } else if (msgRcv->CompareID(MESSAGE_ROBOT_COM_OPEN)) {
                rt_sem_v(&sem_openComRobot);
            } else if (msgRcv->CompareID(MESSAGE_ROBOT_START_WITHOUT_WD)) {
                rt_mutex_acquire(&mutex_watchdog, TM_INFINITE);
                watchdog = false;
                rt_mutex_release(&mutex_watchdog);
                rt_sem_v(&sem_startRobot);
            }else if (msgRcv->CompareID(MESSAGE_ROBOT_START_WITH_WD)) {
                rt_mutex_acquire(&mutex_watchdog, TM_INFINITE);
                watchdog = true;
                rt_mutex_release(&mutex_watchdog);
                rt_sem_v(&sem_startRobot);
            } else if (msgRcv->CompareID(MESSAGE_ROBOT_GO_FORWARD) ||
                    msgRcv->CompareID(MESSAGE_ROBOT_GO_BACKWARD) ||
                    msgRcv->CompareID(MESSAGE_ROBOT_GO_LEFT) ||
                    msgRcv->CompareID(MESSAGE_ROBOT_GO_RIGHT) ||
                    msgRcv->CompareID(MESSAGE_ROBOT_STOP)) {

                rt_mutex_acquire(&mutex_move, TM_INFINITE);
                move = msgRcv->GetID();
                rt_mutex_release(&mutex_move);
            } else if (msgRcv->CompareID(MESSAGE_CAM_OPEN)) {
                rt_mutex_acquire(&mutex_activate_camera, TM_INFINITE);
                activate_camera = true;
                rt_mutex_release(&mutex_activate_camera);
            } else if (msgRcv->CompareID(MESSAGE_CAM_CLOSE)) {
                rt_mutex_acquire(&mutex_activate_camera, TM_INFINITE);
                activate_camera = false;
                rt_mutex_release(&mutex_activate_camera);
            } else if (msgRcv->CompareID(MESSAGE_CAM_ASK_ARENA)) {
                rt_mutex_acquire(&mutex_search_arena, TM_INFINITE);
                search_arena = true;
                rt_mutex_release(&mutex_search_arena);
            } else if (msgRcv->CompareID(MESSAGE_CAM_ARENA_CONFIRM)) {
                rt_mutex_acquire(&mutex_search_arena, TM_INFINITE);
                search_arena = false;
                rt_mutex_release(&mutex_search_arena);
                rt_mutex_acquire(&mutex_arena_confirmed, TM_INFINITE);
                arena_confirmed = true;
                rt_mutex_release(&mutex_arena_confirmed);
                rt_sem_v(&sem_arena_confirmation);
            } else if (msgRcv->CompareID(MESSAGE_CAM_ARENA_INFIRM)) {
                rt_mutex_acquire(&mutex_search_arena, TM_INFINITE);
                search_arena = false;
                rt_mutex_release(&mutex_search_arena);
                rt_mutex_acquire(&mutex_arena_confirmed, TM_INFINITE);
                arena_confirmed = false;
                rt_mutex_release(&mutex_arena_confirmed);
                rt_sem_v(&sem_arena_confirmation);
            } else if (msgRcv->CompareID(MESSAGE_CAM_POSITION_COMPUTE_START)) {
                rt_mutex_acquire(&mutex_position_requested, TM_INFINITE);
                position_requested = true;
                rt_mutex_release(&mutex_position_requested);
                
            } else if (msgRcv->CompareID(MESSAGE_CAM_POSITION_COMPUTE_STOP)) {
                rt_mutex_acquire(&mutex_position_requested, TM_INFINITE);
                position_requested = false;
                rt_mutex_release(&mutex_position_requested);
            }
            delete(msgRcv); // mus be deleted manually, no consumer
            com_monitor_status = 0;
        }
        cout << "Communication with monitor closed" << endl << flush;
    }
}

/**
 * @brief Thread opening communication with the robot.
 */
void Tasks::OpenComRobot(void *arg) {
    int status;
    int err;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task openComRobot starts here                                                  */
    /**************************************************************************************/
    while (1) {
        rt_sem_p(&sem_openComRobot, TM_INFINITE);
        cout << "Open serial com (";
        rt_mutex_acquire(&mutex_robot, TM_INFINITE);
        status = robot.Open();
        rt_mutex_release(&mutex_robot);
        cout << status;
        cout << ")" << endl << flush;

        Message * msgSend;
        if (status < 0) {
            msgSend = new Message(MESSAGE_ANSWER_NACK);
        } else {
            msgSend = new Message(MESSAGE_ANSWER_ACK);
        }
        WriteInQueue(&q_messageToMon, msgSend); // msgSend will be deleted by sendToMon
    }
}

/**
 * @brief Thread starting the communication with the robot.
 */
void Tasks::StartRobotTask(void *arg) {
    bool wd;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task startRobot starts here                                                    */
    /**************************************************************************************/
    while (1) {

        Message * msgSend;
        rt_sem_p(&sem_startRobot, TM_INFINITE);
        rt_mutex_acquire(&mutex_watchdog, TM_INFINITE);
        wd = watchdog;
        rt_mutex_release(&mutex_watchdog);
        if (wd){
            cout << "Start robot with watchdog (";
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            msgSend = robot.Write(robot.StartWithWD());
            rt_mutex_release(&mutex_robot);
            CheckRobotMessage(msgSend);
            cout << msgSend->GetID();
            cout << ")" << endl;

            cout << "Movement answer: " << msgSend->ToString() << endl << flush;
            WriteInQueue(&q_messageToMon, msgSend);  // msgSend will be deleted by sendToMon

            if (msgSend->GetID() == MESSAGE_ANSWER_ACK) {
                rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
                robotStarted = 1;
                rt_mutex_release(&mutex_robotStarted);
                rt_sem_v(&sem_wd_active);
                
            }

        }
        else{
            cout << "Start robot without watchdog (";
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            msgSend = robot.Write(robot.StartWithoutWD());
            rt_mutex_release(&mutex_robot);
            CheckRobotMessage(msgSend);
            cout << msgSend->GetID();
            cout << ")" << endl;

            cout << "Movement answer: " << msgSend->ToString() << endl << flush;
            WriteInQueue(&q_messageToMon, msgSend);  // msgSend will be deleted by sendToMon

            if (msgSend->GetID() == MESSAGE_ANSWER_ACK) {
                rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
                robotStarted = 1;
                rt_mutex_release(&mutex_robotStarted);
            }
        }
    }
}

/**
 * @brief Thread handling control of the robot.
 */
void Tasks::MoveTask(void *arg) {
    int rs;
    int cpMove;
    Message * msgSend;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task starts here                                                               */
    /**************************************************************************************/
    rt_task_set_periodic(NULL, TM_NOW, 100000000);

    while (1) {
        rt_task_wait_period(NULL);
        //cout << "Periodic movement update";
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        rs = robotStarted;
        rt_mutex_release(&mutex_robotStarted);
        if (rs == 1) {
            rt_mutex_acquire(&mutex_move, TM_INFINITE);
            cpMove = move;
            rt_mutex_release(&mutex_move);
            
            cout << " move: " << cpMove;
            
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            msgSend = robot.Write(new Message((MessageID)cpMove));
            rt_mutex_release(&mutex_robot);
            CheckRobotMessage(msgSend);

            // reset new move since move has been sent to robot
            rt_mutex_acquire(&mutex_new_move, TM_INFINITE);
            new_move = false;
            rt_mutex_release(&mutex_new_move);
        }
        //cout << endl << flush;
    }
}

void Tasks::CameraTask(void *arg) {
    int ac, status;
    Message *msgSend;
    MessagePosition *msgPos;
    MessageImg *msgImg;
    Camera cam(sm, 10);
    Img *i;
    bool aconf,sa,pr;
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    
    rt_task_set_periodic(NULL, TM_NOW, PERIOD_100MS);

    while (1) {
        rt_task_wait_period(NULL);

        rt_mutex_acquire(&mutex_activate_camera, TM_INFINITE);
        ac = activate_camera;
        rt_mutex_release(&mutex_activate_camera);

        // check if camera is requested and not already active -> start camera
        if(ac && !camera_active) {
            status = cam.Open();
            if (status) {
                cout << "Camera started successfully" << endl << flush;
                msgSend = new Message(MESSAGE_ANSWER_ACK);
                camera_active = true;
            }
            else {
                cout << "Error starting camera.... " << endl << flush;
                msgSend = new Message(MESSAGE_ANSWER_NACK);
                
                // delete request to open camera
                rt_mutex_acquire(&mutex_activate_camera, TM_INFINITE);
                activate_camera = false;
                rt_mutex_release(&mutex_activate_camera);
            }
            WriteInQueue(&q_messageToMon, msgSend);
        }
        // check if shutdown requested and still active -> shutdown
        else if (!ac && camera_active) {
            cout << "Closing Camera.... " << endl << flush;
            cam.Close();
            camera_active = false;
            msgSend = new Message(MESSAGE_ANSWER_ACK);
            WriteInQueue(&q_messageToMon, msgSend);
        }

        // check if camera is active -> send image
        if (camera_active) {
            cout << "New image" << endl << flush;
            i = new Img(cam.Grab());
            // reqest for arena
            rt_mutex_acquire(&mutex_search_arena, TM_INFINITE);
            sa = search_arena;
            rt_mutex_release(&mutex_search_arena);
            if (sa) {
                arena  = i->SearchArena();
                i->DrawArena(arena);
                msgImg = new MessageImg();
                msgImg->SetImage(i);
                msgImg->SetID(MESSAGE_CAM_IMAGE);
                WriteInQueue(&q_messageToMon, msgImg);
                rt_sem_p(&sem_arena_confirmation, TM_INFINITE);
                rt_mutex_acquire(&mutex_search_arena, TM_INFINITE);
                search_arena = false;
                rt_mutex_release(&mutex_search_arena);
                continue;
            }
            msgImg = new MessageImg();
            msgImg->SetImage(i);
            msgImg->SetID(MESSAGE_CAM_IMAGE);
                        rt_mutex_acquire(&mutex_arena_confirmed, TM_INFINITE);
            aconf = arena_confirmed;
            rt_mutex_release(&mutex_arena_confirmed);
            if (aconf) {
                i->DrawArena(arena);
            }
            rt_mutex_acquire(&mutex_position_requested, TM_INFINITE);
            bool pr = position_requested;
            rt_mutex_release(&mutex_position_requested);
            if (pr && aconf){
                std::list<Position> positionList = i->SearchRobot(arena);
                cout << "Nombre de robots touvés : " << positionList.size() << endl << flush;
                Position position;
                position.center.x = -1.0;
                position.center.y = -1.0;
                position.robotId = 9;
                for (Position p: positionList){
                    i->DrawRobot(p);
                    if (p.robotId == 9){
                        position = p;
                    }
                }
                
                msgPos = new MessagePosition();
                msgPos->SetID(MESSAGE_CAM_POSITION);
                msgPos->SetPosition(position);
                WriteInQueue(&q_messageToMon, msgPos);
            }
            WriteInQueue(&q_messageToMon, msgImg);
            
        }
    }
}


/**
 * @brief Thread checking the battery level of the robot.
 */
void Tasks::BatteryLevelTask(void *arg) {
    int rs;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task BatteryLevelTask starts here                                                  */
    /**************************************************************************************/
    rt_task_set_periodic(NULL, TM_NOW, PERIOD_500MS);
    
    while (1){
        rt_task_wait_period(NULL);
        //cout << "Periodic battery update" << endl << flush;
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        rs = robotStarted;
        rt_mutex_release(&mutex_robotStarted);
        if (rs == 1) {
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            Message * m = robot.Write(robot.GetBattery());
            rt_mutex_release(&mutex_robot);
            CheckRobotMessage(m);
            

            if ((*m).CompareID(MESSAGE_ROBOT_BATTERY_LEVEL)){
                WriteInQueue(&q_messageToMon,m);
            }
            else{
                cout << "Error" << m->ToString() << endl << flush;

            }
        }
    }
}

void Tasks::WatchdogUpdateTask(void *arg) {
    int rs;
    Message *m;
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;

    /**************************************************************************************/
    /* The task WatchdogUpdateTask starts here                                                  */
    /**************************************************************************************/
    
    rt_task_set_periodic(NULL, TM_NOW, PERIOD_1000MS);
    cout << "Status of robot started" << endl << flush;


    while (1){
        rt_task_wait_period(NULL);
        rt_sem_p(&sem_wd_active, TM_INFINITE);
        rt_mutex_acquire(&mutex_robot, TM_INFINITE);
        m = robot.Write(robot.ReloadWD());
        rt_mutex_release(&mutex_robot);
        CheckRobotMessage(m);
        cout << "WD MESSAGE SENT " << endl << flush;
        rt_sem_v(&sem_wd_active);
    }
    
}

/**
 * Write a message in a given queue
 * @param queue Queue identifier
 * @param msg Message to be stored
 */
void Tasks::WriteInQueue(RT_QUEUE *queue, Message *msg) {
    int err;
    if ((err = rt_queue_write(queue, (const void *) &msg, sizeof ((const void *) &msg), Q_NORMAL)) < 0) {
        cerr << "Write in queue failed: " << strerror(-err) << endl << flush;
        throw std::runtime_error{"Error in write in queue"};
    }
}

/**
 * Read a message from a given queue, block if empty
 * @param queue Queue identifier
 * @return Message read
 */
Message *Tasks::ReadInQueue(RT_QUEUE *queue) {
    int err;
    Message *msg;

    if ((err = rt_queue_read(queue, &msg, sizeof ((void*) &msg), TM_INFINITE)) < 0) {
        cout << "Read in queue failed: " << strerror(-err) << endl << flush;
        throw std::runtime_error{"Error in read in queue"};
    }/** else {
        cout << "@msg :" << msg << endl << flush;
    } /**/

    return msg;
}

void Tasks::CheckRobotMessage(Message* msg) {
    int crs;

    if(msg->CompareID(MESSAGE_ANSWER_ROBOT_TIMEOUT)){
        rt_mutex_acquire(&mutex_com_robot_status, TM_INFINITE);
        com_robot_status += 1;
        crs = com_robot_status;
        rt_mutex_release(&mutex_com_robot_status);
    }
    else {
        com_robot_status = 0;
        crs = com_robot_status;
        rt_mutex_release(&mutex_com_robot_status);
    }
    cout << "Message checked, crs= " << crs << endl << flush;
    if (crs == 3) {
        StopRobotCommunication();
        Message *msgSend;
        msgSend = new Message(MESSAGE_ANSWER_COM_ERROR);
        WriteInQueue(&q_messageToMon, msgSend);
        rt_mutex_acquire(&mutex_com_robot_status, TM_INFINITE);
        com_robot_status = 0;
        rt_mutex_release(&mutex_com_robot_status);

    }
}

void Tasks::StopRobotCommunication(){
    bool wd;
    int rs;
    
    
    rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
    rs = robotStarted;
    rt_mutex_release(&mutex_robotStarted);
    
    // stop the watchdog if it was active by stealing its semaphore
    rt_mutex_acquire(&mutex_watchdog, TM_INFINITE);
    wd = watchdog;
    rt_mutex_release(&mutex_watchdog);
    if(wd && rs) {
        rt_sem_p(&sem_wd_active, TM_INFINITE);
    }
    
    rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
    robotStarted = 0;
    rt_mutex_release(&mutex_robotStarted);
}
