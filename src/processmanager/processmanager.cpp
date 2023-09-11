#include <algorithm>
#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "processmanager.h"

#if defined(_WIN32) || defined(_WIN64)
#include <processthreadsapi.h>
#include <Windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <sys/wait.h>
#endif
#include "spdlog/spdlog.h"
namespace qst {
  ChildProcess::ChildProcess(std::string args) {
#if defined(_WIN32) || defined(_WIN64)
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    auto err = CreateProcess(nullptr, args.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if(err == 0) {
      spdlog::error("CreateProcess failed: {}", GetLastError());
      pi.hProcess = INVALID_HANDLE_VALUE;
    }
#elif defined(__linux__)
    this->pid = fork();
    if(pid == 0) {
      // if(!stdio) {
      //   fclose(stdin);
      //   fclose(stdout);
      //   fclose(stderr);
      // }
      setpgid(0, 0);
      std::system(args.data());
      exit(0);
    }
#endif
    flag.test_and_set();
  }
  bool ChildProcess::is_running(void) const {
    return this->flag.test();
  }
  ChildProcess::key_type ChildProcess::key() const {
#if defined(_WIN32) || defined(_WIN64)
    return pi.hProcess;
#elif defined(__linux__)
    return this->pid;
#endif
  }
  void ChildProcess::_wait(void) {
#if defined(_WIN32) || defined(_WIN64)
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    flag.clear();
#elif defined(__linux__)
    int status;
    waitpid(this->pid, &status, 0);
    flag.clear();
#endif
  }
  void ChildProcess::wait(void) {
    this->thr = std::jthread(&ChildProcess::_wait, this);
  }
  ProcessManager::ProcessManager()
    : children() {
  }
  bool ProcessManager::new_process(std::string args) {
    // auto f = std::jthread(&qst::new_process, std::move(args));
    auto cp = std::make_unique<ChildProcess>(args);
    if(cp->is_running()) {
      auto ret = children.emplace(cp->key(), std::move(cp));
      if(ret.second) {
        ret.first->second->wait();
        return true;
      }
    }
    return false;
  }

}  // namespace qst