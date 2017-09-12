#include <memory>
#include <string>
#include <thread>
#include <iostream>
#include <mutex>
#include "demux_decode.h"
#include "callback.h"
#include "openssl/crypto.h"
#include <stdlib.h>
#include <sstream>

#include <grpc++/grpc++.h>
#include "sr.grpc.pb.h"
#include <pthread.h>
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using sr::Empty;
using sr::ReplyInfo;
using sr::TaskId;
using sr::TaskInfo;
using sr::TaskConfig;
using sr::TaskListsInfo;
using sr::TaskManager;

using namespace std;

// Logic and data behind the server's behavior.
class TaskManagerImpl final : public TaskManager::Service {
public:
    enum {
        MaxTaskNum = 150,
    };
    TaskManagerImpl():index(0),last_time(std::chrono::steady_clock::now()){}
    Status StartTask(ServerContext* context, const TaskConfig* task, ReplyInfo* reply) override {
        std::cout<<"Start task"<<std::endl;
        reply->set_result(false); 
        if(task->uri() == ""){
            std::cout<<"uri was null"<<std::endl;
            reply->set_errmsg("uri was NULL!");
            return Status::OK; 
        }
        if(task->channel() == ""){
            reply->set_errmsg("channel was NULL!");
            return Status::OK; 
        }
        if(task->event() == ""){
            reply->set_errmsg("event was NULL!");
            return Status::OK; 
        }
        time_mutex.lock();
        auto current_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> diff = current_time - last_time;
        int diff_time = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
        std::cout<<"diff_time: "<<diff_time<<std::endl;
        while(diff_time < time_val){
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
            diff_time += 50;
        }
        std::cout<<"diff_time: "<<diff_time<<std::endl;
        last_time = std::chrono::steady_clock::now();
        time_mutex.unlock();
        //start task: create a new task and add it in map
        InputFile* input = new InputFile(task->uri().c_str());
        input->channel = task->channel();
        input->event = task->event();
        input->workid = task->workid();
        input->type = task->type();
        input->decodeThread = new std::thread(decode_thread, input);
        input->sendDataThread = new std::thread(send_data, input);
        t_mutex.lock();
        int ret = addTask(input);
        t_mutex.unlock();
        input->taskid = ret;
        //set reply
        reply->set_result(true);
        reply->set_id(ret);
        std::cout<<"start task done! id: "<<ret<<std::endl;
        return Status::OK;
    }

    Status RestoreTask(ServerContext* context, const TaskInfo* task, ReplyInfo* reply) override {
        reply->set_result(false); 
        if(task->uri() == ""){
            reply->set_errmsg("uri was NULL!");
            return Status::OK; 
        }
        if(task->channel() == ""){
            reply->set_errmsg("channel was NULL!");
            return Status::OK; 
        }
        if(task->event() == ""){
            reply->set_errmsg("event was NULL!");
            return Status::OK; 
        }
        time_mutex.lock();
        auto current_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> diff = current_time - last_time;
        int diff_time = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
        std::cout<<"diff_time: "<<diff_time<<std::endl;
        while(diff_time < time_val){
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
            diff_time += 50;
        }
        std::cout<<"diff_time: "<<diff_time<<std::endl;
        last_time = std::chrono::steady_clock::now();
        time_mutex.unlock();

        //start task: create a new task and add it in map
        InputFile* input = new InputFile(task->uri().c_str());
        input->channel = task->channel();
        input->event = task->event();
        input->workid = task->workid();
        input->type = task->type();
        input->decodeThread = new std::thread(decode_thread, input);
        input->sendDataThread = new std::thread(send_data, input);
        t_mutex.lock();
        //int ret = addTask(input);
        int id = task->id();
        input->taskid = id;
        index = id;
        tasks_[index] = input;
        t_mutex.unlock();
        //set reply
        reply->set_result(true);
        reply->set_id(index);
        return Status::OK;
    }

    Status StopTask(ServerContext* context, const TaskId* taskId, ReplyInfo* reply) override{
        //stop
        std::cout<<"stop task"<<std::endl;
        if(tasks_.find(taskId->id()) == tasks_.end()){
            reply->set_result(true);
            reply->set_errmsg("id: " + std::to_string(taskId->id()) + " does not exist!");
            return Status::OK;
        }
        time_mutex.lock();
        auto current_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> diff = current_time - last_time;
        int diff_time = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
        std::cout<<"diff_time: "<<diff_time<<std::endl;
        while(diff_time < time_val){
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
            diff_time += 50;
        }
        std::cout<<"delay, diff_time: "<<diff_time<<std::endl;
        last_time = std::chrono::steady_clock::now();
        time_mutex.unlock();
        InputFile* input = tasks_[taskId->id()];
        t_mutex.lock();
        tasks_.erase(taskId->id());
        t_mutex.unlock();
        //quit
        input->abort_request = 1;
        std::cout<<"id:"<<taskId->id()<<" abort request"<<std::endl;
        input->sendDataThread->join();
        std::cout<<"id:"<<taskId->id()<<" send data join done"<<std::endl;
        input->decodeThread->join();
        std::cout<<"id:"<<taskId->id()<<" decode thread join done"<<std::endl;
        delete input->sendDataThread;
        std::cout<<"id:"<<taskId->id()<<" delete send data thread done"<<std::endl;
        delete input->decodeThread;
        std::cout<<"id:"<<taskId->id()<<" delete decode thread done"<<std::endl;
        delete input;
        std::cout<<"close:"<<taskId->id()<<std::endl;
        //reply
        reply->set_result(true);
        reply->set_id(taskId->id());
        std::cout<<"stop task done! id: "<<taskId->id()<<std::endl;
        return Status::OK;
    }

    Status ListTasks(ServerContext* context, const Empty* empty, TaskListsInfo* taskLists) override {
       std::cout<<"list task"<<std::endl;
       std::unique_lock<std::mutex> lock(t_mutex);
        for(auto& t : tasks_){
            auto info = taskLists->add_tasklists();
            info->set_id(t.first);
            info->set_uri(t.second->filename);
            info->set_channel(t.second->channel);
            info->set_event(t.second->event);
	    info->set_workid(t.second->workid);
            info->set_type(t.second->type);
            //if(t.second->status == Running){
            //    info->set_status("Running");
            //}else if(t.second->status == Waiting){
            //    info->set_status("Waitting");
            //}
        }
        std::cout<<"list task done"<<std::endl;
        return Status::OK;
    }
private:
    std::map<int, InputFile*> tasks_;
    int addTask(InputFile* is){
        index++;
        tasks_[index] = is;
        return index;
    }
    int index;
    std::mutex t_mutex;
    std::chrono::time_point<std::chrono::steady_clock> last_time;
    std::mutex time_mutex;
};

void RunServer(int port) {
  std::string server_address("0.0.0.0:" + std::to_string(port));
  TaskManagerImpl service;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
    if(argc != 2){
        std::cout<<"input server port!"<<std::endl;
        return -1;
    }
    thread_setup();
    Pusher_Init_And_Run(NULL, NULL);
    av_register_all();
    avformat_network_init();
    avfilter_register_all();
    RunServer(atoi(argv[1]));
    thread_cleanup();
    return 0;
}
