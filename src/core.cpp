#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "core.h"
#include "xcl/xcl.h"
// #include <wayland-client.h>
// #include "spdlog/async.h"
// #include "spdlog/sinks/stdout_color_sinks.h"
#include "qst.pb.h"
#include "spdlog/common.h"
#include "spdlog/spdlog.h"
#ifdef __linux__
#endif
namespace qst {

  QstBackendCore::QstBackendCore(int argc, char *argv[])
    : addr()
    , server()
    , searcher()
    // , xcl(std::string(_dupenv_s("HOME")) + "/.config/qst/config.xcl")
    // , xcl(std::string(std::getenv("HOME")) + "/.config/qst/config.xcl")
    // , xcl("")
  // , logger(
  // spdlog::create_async<spdlog::sinks::stdout_color_sink_mt>("backend")
  // )
  {
    // spdlog::set_default_logger(logger);
    std::cout<<"QstBackendCore\t: start"<<std::endl;
    spdlog::debug("QstBackendCore\t: start");
    if(argc < 2) {
      showHelp();
    }
    for(int i = 1; i < argc; ++i) {
      if(std::strcmp(argv[i], "--addr") == 0) {
        this->addr = argv[++i];
        spdlog::debug("Set address to listen on: {}", addr);
      } else if(std::strcmp(argv[i], "--help") == 0) {
        showHelp();
      }
    }
    if(this->addr.empty()) {
      spdlog::error("QstBackendCore\t: no addr");
      std::exit(1);
    }
    // xcl.try_insert("run_count");
  }
  QstBackendCore::~QstBackendCore() {
    server->Shutdown();
    // xcl.save();
  }

  void QstBackendCore::exec() {
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(this);
    server = builder.BuildAndStart();
    spdlog::debug("Start server");
    server->Wait();
  }
  ::grpc::Status QstBackendCore::ListApp(
    ::grpc::ServerContext *context, const ::qst_comm::Input *request, ::qst_comm::DisplayList *response) {
    spdlog::debug("ListApp\t: input\t= {}", request->str());
    ::qst_comm::Display display;
    // Display display;
    this->last_result = searcher.search(request->str());
    // for(auto info : this->last_result) {
    //   if(!(info->is_config)) {
    //     auto run_count = xcl.find<long>(std::string("run_count'") + info->name);
    //     if(run_count) {
    //       info->run_count = *run_count;
    //     }
    //     spdlog::debug("ListApp\t: load run_count\t= {}", info->run_count);
    //     info->is_config = true;
    //   }
    // }
    std::sort(this->last_result.begin(), this->last_result.end(), [](AppInfo *a, AppInfo *b) { return a->run_count > b->run_count; });
    for(auto info : this->last_result) {
      spdlog::debug("ListApp\t: name\t= {}", info->name, info->run_count);
      spdlog::debug("ListApp\t: exec\t= {}", info->exec);
      spdlog::debug("ListApp\t: argHint\t= {}", info->args_hint);
      spdlog::debug("ListApp\t: run_count\t= {}", info->run_count);
      display.set_name(std::string(info->name));
      display.set_arghint(std::string(info->args_hint));
      response->add_list()->CopyFrom(display);
    }
    return ::grpc::Status::OK;
  }
  ::grpc::Status QstBackendCore::RunApp(
    ::grpc::ServerContext *context, const ::qst_comm::ExecHint *request, ::qst_comm::Empty *response) {
    AppInfo *info =this->last_result[request->idx()];
    std::string args(info->exec);
    if(auto p= args.find("%"); p != std::string::npos) {
      args.replace(p,2,request->args());
    }
    spdlog::debug("RunApp\t: {}",args);
    pm.new_process(std::move(args));
    info->run_count++;
    // xcl.insert_or_assign<long>(std::string("run_count'") + info->name, info->run_count);
    // xcl.save(true);
    return ::grpc::Status::OK;
  }
  void QstBackendCore::showHelp() {
    std::cout << "Usage: qst [options]" << std::endl
              << "Options:" << std::endl
              << "  --addr <addr>       Set the address to listen on" << std::endl
              << "  --help              Show this help message" << std::endl;
    std::exit(0);
  }
}  // namespace qst